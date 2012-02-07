/* faFilterN - Get rid of sequences with too many N's. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faFilterN - Get rid of sequences with too many N's\n"
  "usage:\n"
  "   faFilterN in.fa out.fa maxPercentN\n"
  "options:\n"
  "   -out=in.fa.out\n"
  "   -uniq=self.psl\n"
  );
}


struct range
/* Range of sequence. */
    {
    struct range *next;	/* Next in list. */
    int start;		/* Start (zero based). */
    int end;		/* End (not inclusive). */
    };

struct repInfo
/* Repeat information for a sequence. */
    {
    struct repInfo *next;	/* Next in list. */
    char *name;			/* Name of sequence - allocated in hash. */
    struct range *repList;	/* List of repeats. */
    };

struct hash *hashOut(char *fileName)
/* Load file-name into a hash keyed by accession with
 * repeat mask info. */
{
struct hash *hash;
struct lineFile *lf;
char *line, *words[18];
int wordCount;
struct repInfo *ri;
struct range *range;

/* Open file, read header. */
if (fileName == NULL)
    return NULL;
lf = lineFileOpen(fileName, TRUE);
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", fileName);
line = skipLeadingSpaces(line);
if (!startsWith("SW", line))
    errAbort("%s doesn't seem to be a RepeatMasker .out file", fileName);
lineFileNext(lf, &line, NULL);
lineFileNext(lf, &line, NULL);

hash = newHash(0);
while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    char *acc;
    if (wordCount < 8)
        errAbort("Short line %d of %s", lf->lineIx, lf->fileName);
    acc = words[4];
    if ((ri = hashFindVal(hash, acc)) == NULL)
        {
	AllocVar(ri);
	hashAddSaveName(hash, acc, ri, &ri->name);
	}
    AllocVar(range);
    range->start = lineFileNeedNum(lf, words, 5) - 1;
    range->end = lineFileNeedNum(lf, words, 6);
    slAddHead(&ri->repList, range);
    }
lineFileClose(&lf);
return hash;
}

int countN(struct hash *hash, char *acc, int size)
/* Return number of N's in acc. */
{
struct repInfo *ri;
struct range *range;
int count = 0;

if ((ri = hashFindVal(hash, acc)) == NULL)
    return 0;
for (range = ri->repList; range != NULL; range = range->next)
    count += range->end - range->start;
return count;
}

boolean overlapsPrevious(char *acc, struct hash *usedHash, struct psl *pslList)
/* Return true if acc overlaps a sequence in usedHash. */
{
struct psl *psl;

for (psl = pslList; psl != NULL; psl = psl->next)
    {
    boolean match = FALSE;
    if (sameString(acc, psl->qName))
        {
	if (hashLookup(usedHash, psl->tName))
	    match = TRUE;
	}
    else if (sameString(acc, psl->tName))
        {
	if (hashLookup(usedHash, psl->qName))
	    match = TRUE;
	}
    if (match && psl->match > 30)
        return TRUE;
    }
return FALSE;
}

void faFilterN(char *inFile, char *outFile, double maxPercent)
/* faFilterN - Get rid of sequences with too many N's. */
{
double maxRatio = maxPercent*0.01;
double ratio;
struct dnaSeq seq;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
int seqCount = 0, passCount = 0;
char *outName = cgiOptionalString("out");
struct hash *outHash = hashOut(outName);
char *pslName = cgiOptionalString("uniq");
struct psl *pslList = NULL;
struct hash *usedHash = newHash(0);
ZeroVar(&seq);

if (pslName != NULL)
    pslList = pslLoadAll(pslName);
while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    if (outHash)
        ratio = countN(outHash, seq.name, seq.size);
    else
	ratio = countChars(seq.dna, 'n');
    ratio /= seq.size;
    seqCount += 1;
    if (ratio <= maxRatio)
        {
	if (!overlapsPrevious(seq.name, usedHash, pslList))
	    {
	    passCount += 1;
	    faWriteNext(f, seq.name, seq.dna, seq.size);
	    hashAdd(usedHash, seq.name, NULL);
	    }
	else
	    {
	    uglyf("Filtering out %s\n", seq.name);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4 || !isdigit(argv[3][0]))
    usage();
faFilterN(argv[1], argv[2], atof(argv[3]));
return 0;
}
