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

static char const rcsid[] = "$Id: atomToSeq.c,v 1.1 2007/06/10 22:37:54 braney Exp $";

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
  );
}

static struct optionSpec options[] = {
   {"minLen", OPTION_INT},
   {NULL, 0},
};

extern int minLen;



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
struct seqBlock *sb;
int lastEnd = 0;
si->min = si->blockList->start;

for(sb=si->blockList; sb ;  sb = sb->next)
    {
    si->count++;
    if (lastEnd > sb->start)
	errAbort("overlapping blocks:  lastEnd %d start %d",lastEnd,sb->start);
    lastEnd = sb->end;
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

    printf("build sequence for %s\n",si->name);
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

    printf("sorting sequence for %s\n",si->name);
    slSort(&si->blockList, blockCmp);

    validateSI(si);

    printf("outputting sequence for %s\n",si->name);
    outputSI(f, si);

    printf("clearing sequence for %s\n",si->name);
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

atomToSeq(argv[1],argv[2], argv[3]);
return 0;
}
