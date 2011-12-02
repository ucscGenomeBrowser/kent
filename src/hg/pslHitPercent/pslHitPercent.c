/* pslHitPercent - Figure out percentage of reads in FA file that hit.. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslHitPercent - Figure out percentage of reads in FA file that hit.\n"
  "usage:\n"
  "   pslHitPercent file.psl file.fa\n");
}

struct readInfo
/* Info about a single read. */
    {
    struct readInfo *next;      /* Next in list. */
    char *name;			/* Read name, not allocated here. */
    int nCount;                 /* Total N's in read. */
    bool good;			/* Good in sense of 100 uniq bases in row. */
    int hits;                  /* How many times hit . */
    };

int countN(DNA *dna, int size)
/* Count number of N's and other ambiguous chars. */
{
int count = 0;
int i;

for (i=0; i<size; ++i)
    if (ntVal[(int)dna[i]] < 0)
        ++count;
return count;
}

boolean isGoodRead(DNA *dna, int size)
/* See if have at least 100 non-N bases in a row. */
{
int count = 0;
int i;

for (i=0; i<size; ++i)
    {
    if (ntVal[(int)dna[i]] >= 0)
	{
        if (++count >= 100)
	    {
	    return TRUE;
	    }
	}
    else
	{
        count = 0;
	}
    }
return FALSE;
}

void loadReads(char *fileName, struct hash *readHash, struct readInfo **pRiList)
/* Load reads from .fa file. */
{
FILE *f = mustOpen(fileName, "r");
struct readInfo *ri;
DNA *dna;
int dnaSize;
char *dnaName;
struct hashEl *hel;

while (faFastReadNext(f, &dna, &dnaSize, &dnaName))
    {
    AllocVar(ri);
    hel = hashAddUnique(readHash, dnaName, ri);
    ri->name = hel->name;
    ri->nCount = countN(dna, dnaSize);
    ri->good = isGoodRead(dna, dnaSize);
    slAddHead(pRiList, ri);
    }
fclose(f);
}

double ratioPercent(double p, double q)
/* Return 100.0 * p/q. */
{
return 100.0 * p / q;
}

void findHits(char *pslFile, struct hash *readHash)
/* Read psl file and set flag in reads that are hit. */
{
struct lineFile *lf = pslFileOpen(pslFile);
struct psl *psl;
struct readInfo *ri;

while ((psl = pslNext(lf)) != NULL)
    {
    ri = hashMustFindVal(readHash, psl->qName);
    ++ri->hits;
    pslFree(&psl);
    }
lineFileClose(&lf);
}


void pslHitPercent(char *pslFile, char *faFile)
/* pslHitPercent - Figure out percentage of reads in FA file that hit.. */
{
struct hash *readHash = newHash(20);
struct readInfo *riList = NULL, *ri;
int allCount = 0, goodCount = 0, allAliCount = 0, goodAliCount = 0;
int totalHits = 0;

printf("Scanning %s\n", faFile);
loadReads(faFile, readHash, &riList);
for (ri = riList; ri != NULL; ri = ri->next)
    {
    ++allCount;
    if (ri->good)
        ++goodCount;
    }
printf("%d of %d 'good' reads (%4.2f%%) have 100 bases of non-repeats in a row\n",
	goodCount, allCount,
	ratioPercent(goodCount, allCount));

findHits(pslFile, readHash);
for (ri = riList; ri != NULL; ri = ri->next)
    {
    if (ri->hits)
	{
	totalHits += ri->hits;
	++allAliCount;
	if (ri->good)
	    ++goodAliCount;
	}
    }

printf("%d of %d hits to all (%4.2f%%)\n",
    allAliCount, allCount, ratioPercent(allAliCount, allCount));
printf("%d of %d good hit (%4.2f%%)\n",
    goodAliCount, goodCount, ratioPercent(goodAliCount, goodCount));
printf("%d total hits\n", totalHits);

freeHash(&readHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
dnaUtilOpen();
pslHitPercent(argv[1], argv[2]);
return 0;
}
