/* bedCoverage - Analyse coverage by bed files - chromosome by chromosome 
 * and genome-wide.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"
#include "agpFrag.h"
#include "memalloc.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedCoverage - Analyse coverage by bed files - chromosome by \n"
  "chromosome and genome-wide.\n"
  "usage:\n"
  "   bedCoverage database bedFile\n"
  "Note bed file must be sorted by chromosome\n"
  );
}

struct chromInfo *getAllChromInfo()
/* Return list of info for all chromosomes. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct chromInfo *ci, *ciList = NULL;

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    ci = chromInfoLoad(row);
    slAddHead(&ciList, ci);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&ciList);
return ciList;
}

#define maxCover 100

struct chromSizes
/* Sizes of each chromosome, with and without gaps. */
   {
   struct chromSizes *next;
   char *name;	/* Allocated in hash */
   double seqSize;   /* Size without N's. */
   double totalSize; /* Size with N's. */
   double totalDepth;  /* Sum of coverage of all bases. */
   double totalCov;    /* Sum of bases covered at least once. */

   double histogram[maxCover+1]; /* Coverage histogram. */
   boolean completed;   /* True if completed. */
   };

void showStats(struct chromSizes *cs)
/* Print out stats. */
{
int i, j;
printf("%-6s %9.0f depth %10.0f %5.2f anyCov %10.0f %5.2f%%\n", 
	cs->name, cs->seqSize, 
	cs->totalDepth, 100.0 * cs->totalDepth/cs->seqSize,
	cs->totalCov, 100.0 * cs->totalCov/cs->seqSize);
for (i=1; i<10; ++i)
    {
    double sum = cs->histogram[i];
    printf("%2d  %10.0f %5.3f%%\n", i, sum, 100.0 * sum/cs->seqSize);
    }
for (i=0; i<100; i += 10)
    {
    double sum = 0;
    for (j=0; j<10; ++j)
        {
	int ix = i+j;
	sum += cs->histogram[ix];
	}
    printf("%2d to %2d:  %10f %5.3f%%\n", i, i+9, sum, 100.0 * sum/cs->seqSize);
    }
printf(">=100  %10f %5.3f%%\n", cs->histogram[100], 
	100.0 * cs->histogram[100]/cs->seqSize);
}

void shortStats(struct chromSizes *cs)
/* Display short form of stats. */
{
int i;
double totalCov, twoOrMore, tenOrMore=0, hundredOrMore=0;
totalCov = cs->totalCov;
twoOrMore = totalCov - cs->histogram[1];
for (i=10; i<=100; ++i)
    tenOrMore += cs->histogram[i];
hundredOrMore = cs->histogram[100];
printf("%s\t%1.3f\t%1.3f\t%1.3f\t%1.3f\n", 
	cs->name, 
	totalCov * 100.0 / cs->seqSize, 
	twoOrMore * 100.0 / cs->seqSize, 
	tenOrMore * 100.0 / cs->seqSize, 
	hundredOrMore * 100.0 / cs->seqSize);
}

void getChromSizes(struct hash **retHash, 
	struct chromSizes **retList)
/* Return hash of chromSizes. */
{
struct sqlConnection *conn = hAllocConn();
struct chromInfo *ci, *ciList = getAllChromInfo();
struct sqlResult *sr;
char **row;
struct chromSizes *cs, *csList = NULL;
struct hash *hash = newHash(8);
char query[256];
int rowOffset;

for (ci = ciList; ci != NULL; ci = ci->next)
    {
    AllocVar(cs);
    hashAddSaveName(hash, ci->chrom, cs, &cs->name);
    slAddHead(&csList, cs);
    cs->totalSize = ci->size;
    sr = hChromQuery(conn, "gold", ci->chrom, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct agpFrag frag;
	agpFragStaticLoad(row + rowOffset, &frag);
	cs->seqSize += frag.chromEnd - frag.chromStart;
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
slReverse(&csList);
*retHash = hash;
*retList = csList;
}

void incNoOverflow(UBYTE *cov, int size)
/* Add one to each member of cov, so long as it's
 * not over maxCover. */
{
UBYTE c;
while (--size >= 0)
   {
   c = *cov;
   if (c < maxCover)
       {
       ++c;
       *cov = c;
       }
   ++cov;
   }
}

void closeChromCov(struct lineFile *lf, struct chromSizes *cs, UBYTE **pCov)
/* Fill in cs->histogram from cov, and free cov. */
{
UBYTE *cov = *pCov, c;
int i, size = cs->totalSize;
for (i=0; i<size; ++i)
   {
   c = cov[i];
   if (c > 0)
       {
       cs->histogram[c] += 1;
       cs->totalCov += 1;
       }
   }
freez(pCov);
if (cs->completed)
     errAbort("Bed file not sorted by chromosome line %d of %s.", lf->lineIx, lf->fileName);
cs->completed = TRUE;
showStats(cs);
}


void scanBed(char *fileName, struct hash *chromHash)
/* Scan through bed file (which must be sorted by
 * chromosome) and fill in coverage histograms on 
 * each chromosome. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chromSizes *lastCs = NULL, *cs = NULL;
char *chrom;
int start, end, size;
char *row[3];
UBYTE *cov = NULL;

while (lineFileRow(lf, row))
    {
    chrom = row[0];
    start = lineFileNeedNum(lf, row, 1);
    end = lineFileNeedNum(lf, row, 2);
    size = end - start;
    cs = hashMustFindVal(chromHash, chrom);
    if (cs != lastCs)
        {
	if (lastCs != NULL)
	    closeChromCov(lf, lastCs, &cov);
	AllocArray(cov, cs->totalSize);
	lastCs = cs;
	}
    if (end > cs->totalSize)
        errAbort("End %d past end %d of %s", end, cs->totalSize, cs->name);
    incNoOverflow(cov+start, size);
    cs->totalDepth += size;
    }
closeChromCov(lf, cs, &cov);
lineFileClose(&lf);
}

struct chromSizes *genoSize(struct chromSizes *chromSizes)
/* Sum up all chromSizes and return result. */
{
struct chromSizes *cs, *g;
int i;
AllocVar(g);
g->name = "all";
for (cs = chromSizes; cs != NULL; cs = cs->next)
    {
    g->seqSize += cs->seqSize;
    g->totalSize += cs->totalSize;
    g->totalDepth += cs->totalDepth;
    g->totalCov += cs->totalCov;
    for (i=0; i<=maxCover; ++i)
        g->histogram[i] += cs->histogram[i];
    }
return g;
}

void bedCoverage(char *database, char *bedFile)
/* bedCoverage - Analyse coverage by bed files - chromosome by 
 * chromosome and genome-wide.. */
{
struct chromSizes *cs, *csList = NULL;
struct hash *chromHash = NULL;
struct chromSizes *genome;
hSetDb(database);

getChromSizes(&chromHash, &csList);
scanBed(bedFile, chromHash);
genome = genoSize(csList);
showStats(genome);

printf("\n");
shortStats(genome);
for (cs = csList; cs != NULL; cs = cs->next)
    shortStats(cs);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(350000000);
optionHash(&argc, argv);
if (argc != 3)
    usage();
bedCoverage(argv[1], argv[2]);
return 0;
}
