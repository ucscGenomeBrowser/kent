#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hdb.h"
#include "hash.h"
#include "bed.h"
#include "psl.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"
#include "dystring.h"
#include "values.h"
#include "atom.h"

static char const rcsid[] = "$Id: atomToSeq.c,v 1.2 2007/10/16 19:03:39 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomToSeq - take an atom file and generate the sequence\n"
  "usage:\n"
  "   atomToSeq regions.bed in.atom out.seq\n"
  "arguments:\n"
  "   regions.bed    bed file with ranges for all species\n"
  "   in.atom        list of atoms to string\n"
  "   out.seq        list of sequence (atom orderings)\n"
  "options:\n"
  "   -minLen=N       minimum size of atom to consider\n"
  "   -overlap        allow blocks to overlap with just a warning\n"
  "   -cover=100.atom output a set of atoms with 100% coverage\n"
  );
}

static struct optionSpec options[] = {
   {"overlap", OPTION_BOOLEAN},
   {"minLen", OPTION_INT},
   {"cover", OPTION_STRING},
   {NULL, 0},
};

extern int minLen;

boolean allowOverlap = FALSE;

struct seqBlock
{
struct seqBlock *next;
char *chrom;
int start, end;
char strand;
unsigned atomNum;
int instanceNum;
};

struct speciesInfo
{
struct speciesInfo *next;
int min, max, count;
struct seqBlock *blockList;
char *name;
char *chrom;
int chromStart, chromEnd;
};

int blockCmp(const void *va, const void *vb)
{
const struct seqBlock *a = *((struct seqBlock **)va);
const struct seqBlock *b = *((struct seqBlock **)vb);

return a->start - b->start;
}


void validateSI(struct speciesInfo *si)
{
struct seqBlock *sb, *lastSb = NULL;
int lastEnd = 0;

si->min = si->blockList->start;

for(sb=si->blockList; sb ;  sb = sb->next)
    {
    if (lastEnd > sb->start) 
	{

	int overlap=0;

	overlap = lastEnd - sb->start;
	if (overlap > .2 * (sb->end - sb->start))
	    {
	    warn("dropping: count %d instance %d atom %d with instance %d atom %d\n",overlap,sb->instanceNum, sb->atomNum,lastSb->instanceNum, lastSb->atomNum);
	    lastSb->next = sb->next;
	    continue;
	    }
	if (allowOverlap)
	    warn("overlapping blocks:  lastEnd %d start %d",lastEnd,sb->start);
	else
	    errAbort("overlapping blocks:  lastEnd %d start %d",lastEnd,sb->start);
	}
    si->count++;
    lastEnd = sb->end;
    lastSb = sb;
    }
si->max = lastEnd;
}

void outputSI( FILE *f, struct speciesInfo *si)
{
fprintf(f, ">%s.%s:%d-%d %d\n",si->name,si->chrom, 
    si->min+1,si->max, si->count);

struct seqBlock *sb = si->blockList;
int count = 0;

for(; sb ; sb = sb->next)
    {
    if (sb->strand == '-')
	fprintf(f, "-");
    fprintf(f, "%d.%d ",sb->atomNum, sb->instanceNum);
    count++;
    if ((count & 0x7) == 0x7)
	fprintf(f,"\n");
    }
if (!((count & 0x7) == 0x7))
    fprintf(f,"\n");
}

struct speciesInfo *getSpeciesRanges(char *bedName)
{
struct lineFile *lf = lineFileOpen(bedName, TRUE);
struct speciesInfo *new, *list = NULL;
char *row[4];

while (lineFileRow(lf, row))
    {
    AllocVar(new);
    slAddHead(&list, new);
    new->chrom = cloneString(row[0]);
    new->chromStart = sqlUnsigned(row[1]);
    new->chromEnd = sqlUnsigned(row[2]);
    new->name = cloneString(row[3]);
    }

lineFileClose(&lf);
return list;
}

void atomToSeq(char *bedName, char *inAtomName, char *outSeqName)
{
struct speciesInfo *speciesList = getSpeciesRanges(bedName);
struct speciesInfo *si;
FILE *f = mustOpen(outSeqName, "w");

for(si = speciesList; si ; si = si->next)
    {
    struct lineFile *lf = lineFileOpen(inAtomName, TRUE);
    struct seqBlock *sb, *nextSb;
    int wordsRead;
    char *words[16];
    int intCount = 0;
    int atomNum = 0;
    int count = 0;

    verbose(2,"build sequence for %s\n",si->name);
    while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
	{
	if (wordsRead == 0)
	    continue;

	if (*words[0] == '>')
	    {
	    if (wordsRead != 3)
		errAbort("need 3 fields on atom def line");

	    atomNum = atoi(&words[0][1]);
	    intCount = atoi(words[2]);
	    count = 0;
	    }
	else
	    {
	    char *ptr, *chrom, *start;

	    intCount--;
	    if (intCount < 0)
		errAbort("bad atom: too many instances on line %d",lf->lineIx);
	    if (wordsRead != 2)
		errAbort("need 2 fields on instance def line");

	    ptr = strchr(words[0], '.');
	    *ptr++ = 0;
	    chrom = ptr;
	    count++;

	    if (!sameString(words[0],si->name))
		continue;

	    AllocVar(sb);

	    sb->instanceNum = count - 1;

	    ptr = strchr(chrom, ':');
	    *ptr++ = 0;
	    start = ptr;

	    ptr = strchr(start, '-');
	    *ptr++ = 0;

	    sb->start = atoi(start) - 1;
	    sb->end = atoi(ptr);
	    sb->strand = *words[1];

	    sb->atomNum = atomNum;

	    slAddHead(&si->blockList, sb);

	    }
	}
    lineFileClose(&lf);

    if (si->blockList == NULL)
	continue;

    verbose(2,"sorting sequence for %s\n",si->name);
    slSort(&si->blockList, blockCmp);

    validateSI(si);

    verbose(2,"outputting sequence for %s\n",si->name);
    outputSI(f, si);

    verbose(2,"clearing sequence for %s\n",si->name);
    for(sb = si->blockList; sb ; sb = nextSb)
	{
	nextSb = sb->next;
	freez(&sb);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

minLen = optionInt("minLen", minLen);
allowOverlap = optionExists("overlap");
coverFile = option

atomToSeq(argv[1],argv[2], argv[3]);
return 0;
}
