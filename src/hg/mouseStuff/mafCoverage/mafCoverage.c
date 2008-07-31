/* mafCoverage - Analyse coverage by maf files - chromosome by chromosome 
 * and genome-wide.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"
#include "agpFrag.h"
#include "memalloc.h"
#include "maf.h"

#define MAXALIGN 30  /* max number of species to align */
#define DEFCOUNT 3   /* require 3 species to match before counting as covered */
static char const rcsid[] = "$Id: mafCoverage.c,v 1.6.70.1 2008/07/31 02:24:41 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafCoverage - Analyse coverage by maf files - chromosome by \n"
  "chromosome and genome-wide.\n"
  "usage:\n"
  "   mafCoverage database mafFile\n"
  "Note maf file must be sorted by chromosome,tStart\n"
  "   -restrict=restrict.bed Restrict to parts in restrict.bed\n"
  "   -count=N Number of matching species to count coverage. Default = 3 \n"
  );
}

struct optionSpec options[] = {
   {"count", OPTION_INT},
   {"restrict", OPTION_STRING},
   {NULL,0}
};
struct chromInfo *getAllChromInfo(char *database)
/* Return list of info for all chromosomes. */
{
struct sqlConnection *conn = hAllocConn(database);
//struct sqlConnection *conn = sqlConnectRemote(hGetDb());
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
sqlDisconnect(&conn);
//hFreeConn(&conn);
slReverse(&ciList);
return ciList;
}

#define maxDepth 100    /* Maximum depth we track, if change this showStats and shortStats need updating. */
#define restricted 255	/* Special value for masked out. */

struct simpleRange
/* A range inside of a chromosome. */
   {
   struct simpleRange *next;
   int start;	/* Start (zero based) */
   int end;	/* End (not included) */
   };

struct chromSizes
/* Sizes of each chromosome, with and without gaps. */
   {
   struct chromSizes *next;
   char *name;	/* Allocated in hash */
   double seqSize;   /* Size without N's. */
   double totalSize; /* Size with N's. */
   double unrestrictedSize;	/* Size unrestricted. */
   double totalDepth;  /* Sum of coverage of all bases. */
   double totalCov;    /* Sum of bases covered at least once. */
   double totalAlign;    /* Sum of aligning bases covered at least once. */
   double totalId;    /* Sum of aligning exact match bases covered at least once. */

   double histogram[maxDepth+1]; /* Coverage histogram. */
   double histogramAlign[maxDepth+1]; /* Coverage histogram. */
   boolean completed;   /* True if completed. */
   struct simpleRange *restrictList;	/* List of ranges to restrict to. */
   };


void addRestrictions(struct hash *hash, char *fileName)
/* Add restrictions from maf file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
int count = 0;
while (lineFileRow(lf, row))
    {
    struct chromSizes *cs = hashMustFindVal(hash, row[0]);
    struct simpleRange *r;
    AllocVar(r);
    r->start = lineFileNeedNum(lf, row, 1);
    r->end = lineFileNeedNum(lf, row, 2);
    if (r->start < 0 || r->end > cs->totalSize)
        errAbort("%d-%d doesn't fit into %s size %ld line %d of %s",
		 r->start, r->end, row[0], (long)cs->totalSize,
		 lf->lineIx, lf->fileName);
    slAddHead(&cs->restrictList, r);
    ++count;
    }
printf("Added %d restrictions from %s\n", count, fileName);
}

void showStats(struct chromSizes *cs)
/* Print out stats. */
{
int i, j;
printf("%-6s ", cs->name);
printLongWithCommas(stdout,cs->unrestrictedSize);
printf("    Depth ");
printLongWithCommas(stdout,cs->totalDepth);
printf(" %5.2f%%    Coverage ", 100.0 * cs->totalDepth/cs->unrestrictedSize);
printLongWithCommas(stdout,cs->totalCov);
printf(" %5.2f%%    Align ", 100.0 * cs->totalCov/cs->unrestrictedSize);
printLongWithCommas(stdout,cs->totalAlign);
printf(" %5.2f%%    Exact Match ", 100.0 * cs->totalAlign/cs->unrestrictedSize);
printLongWithCommas(stdout,cs->totalId);
printf(" %5.2f%%    MisMatch ", 100.0 * cs->totalId/cs->unrestrictedSize);
printLongWithCommas(stdout,cs->totalAlign - cs->totalId);
printf(" %5.2f%%    \n", 100.0 * (cs->totalAlign - cs->totalId)/cs->unrestrictedSize);
for (i=1; i<10; ++i)
    {
    double sum = cs->histogram[i];
    if (sum > 0)
        printf("%2d  %10.0f %5.3f%%\n", i, sum, 100.0 * sum/cs->unrestrictedSize);
    }
for (i=0; i<100; i += 10)
    {
    double sum = 0;
    for (j=0; j<10; ++j)
        {
	int ix = i+j;
	sum += cs->histogram[ix];
	}
    if (sum > 0)
        printf("%2d to %2d:  %10f %5.3f%%\n", i, i+9, sum, 100.0 * sum/cs->unrestrictedSize);
    }
if (cs->histogram[100] > 0)
    printf(">=100  %10f %5.3f%%\n", cs->histogram[100], 
	100.0 * cs->histogram[100]/cs->unrestrictedSize);
printf("\n");
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
if (totalCov > 0 || cs->totalAlign > 0 || tenOrMore > 0 || hundredOrMore > 0)
    {
    printf("%s\t", cs->name); 
    printLongWithCommas(stdout,cs->totalCov);
    printf("\t%1.2f\t",totalCov * 100.0 / cs->unrestrictedSize); 
    printLongWithCommas(stdout,cs->totalAlign);
    printf("\t%1.2f\t",cs->totalAlign * 100.0 / cs->unrestrictedSize);
    printLongWithCommas(stdout,cs->totalId);
    printf("\t%1.2f\t",cs->totalId * 100.0 / cs->unrestrictedSize);
    printLongWithCommas(stdout,cs->totalAlign - cs->totalId);
    printf("\t%1.2f\n",(cs->totalAlign - cs->totalId) * 100.0 / cs->unrestrictedSize);
    }
}

void getChromSizes(char *database, struct hash **retHash, 
	struct chromSizes **retList)
/* Return hash of chromSizes.  Also calculates size without
 * gaps. */
{
struct sqlConnection *conn = hAllocConn(database);
//struct sqlConnection *conn = sqlConnectReadOnly(hGetDb());
struct chromInfo *ci, *ciList = getAllChromInfo(database);
struct sqlResult *sr;
char **row;
struct chromSizes *cs, *csList = NULL;
struct hash *hash = newHash(8);
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
sqlDisconnect(&conn);
//hFreeConn(&conn);
slReverse(&csList);
*retHash = hash;
*retList = csList;
}

void incNoOverflow(UBYTE *cov, int size)
/* Add one to each member of cov, so long as it's
 * not over maxDepth. */
{
UBYTE c;
while (--size >= 0)
   {
   c = *cov;
   if (c < maxDepth)
       {
       ++c;
       *cov = c;
       }
   ++cov;
   }
}

void closeChromCov(char *fileName, struct chromSizes *cs, UBYTE **pCov1, UBYTE **pCov2, UBYTE **pCov3)
/* Fill in cs->histogram from cov, and free cov. */
{
UBYTE *cov = *pCov1, *align = *pCov2, *id = *pCov3,  c;
int i, size = cs->totalSize;
for (i=0; i<size; ++i)
   {
   c = cov[i];
   if (c > 0 && c != restricted)
       {
       cs->histogram[c] += 1;
       cs->totalCov += 1;
       }
   c = align[i];
   if (c > 0 && c != restricted)
       {
       cs->histogramAlign[c] += 1;
       cs->totalAlign += 1;
       }
   c = id[i];
   if (c > 0 && c != restricted)
       {
       cs->totalId += 1;
       }
   }
freez(pCov1);
freez(pCov2);
freez(pCov3);
if (cs->completed)
     errAbort("maf file not sorted by chromosome in %s .", fileName);
cs->completed = TRUE;
showStats(cs);
}


void restrictCov(UBYTE *cov, int size, struct simpleRange *restrictList)
/* Set areas that are restricted (not in restrict list) to restricted
 * value. Assumes cov is all zero to begin with. */
{
struct simpleRange *r;
memset(cov, restricted, size);
for (r = restrictList; r != NULL; r = r->next)
    {
    assert(r->start >= 0);
    assert(r->end <= size);
    memset(cov + r->start, 0, r->end - r->start);
    }
}

void restrictGaps(char *database, UBYTE *cov, int size, char *chrom)
/* Mark gaps as off-limits. */
{
int rowOffset;
struct sqlConnection *conn = hAllocConn(database);
//struct sqlConnection *conn = sqlConnectReadOnly(hGetDb());
struct sqlResult *sr = hChromQuery(conn, "gap", chrom, NULL, &rowOffset);
char **row;
int s,e;

while ((row = sqlNextRow(sr)) != NULL)
    {
    s = sqlUnsigned(row[1+rowOffset]);
    e = sqlUnsigned(row[2+rowOffset]);
    assert(s >= 0);
    assert(e <= size);
    memset(cov + s, restricted, e - s);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
//hFreeConn(&conn);
}

int calcUnrestrictedSize(UBYTE *cov, int size)
/* Figure out number of bases not restricted. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (cov[i] != restricted)
        ++count;
return count;
}


void scanMaf(char *database, char *fileName, struct hash *chromHash, boolean restrict, int spCount)
/* Scan through maf file (which must be sorted by
 * chromosome) and fill in coverage histograms on 
 * each chromosome. */
{
struct mafFile *mf = mafOpen(fileName);
struct mafAli *ali = NULL;
struct mafComp *comp = NULL;
struct chromSizes *lastCs = NULL, *cs = NULL;
char *chrom = NULL;
int start = 0, end = 0, size = 0, j, k;
int idStart = 0, idEnd = 0, idSize = 0;
UBYTE *cov = NULL;
UBYTE *align = NULL;
UBYTE *id = NULL;
char *tPtr[MAXALIGN];
bool hit = FALSE;

while ((ali = mafNext(mf)) != NULL)
    {
    int cCount = slCount(ali->components);
    int i = 1;
    int nextStart, idNextStart;
    
    comp = ali->components; 
    tPtr[0] = comp->text;
    chrom = strchr(comp->src,'.')+1;
    if (chrom == NULL)
         chrom = comp->src;
    start = comp->start;
    idStart = comp->start;
    nextStart = idNextStart = start;
    cs = hashMustFindVal(chromHash, chrom);
    if (cs != lastCs)
        {
	if (lastCs != NULL)
	    closeChromCov(fileName, lastCs, &cov, &align, &id);
	AllocArray(cov, cs->totalSize);
	AllocArray(align, cs->totalSize);
	AllocArray(id, cs->totalSize);
	if (restrict)
            {
	    restrictCov(cov, cs->totalSize, cs->restrictList);
	    restrictCov(align, cs->totalSize, cs->restrictList);
	    restrictCov(id, cs->totalSize, cs->restrictList);
            }
	restrictGaps(database, cov, cs->totalSize, chrom);
	restrictGaps(database, align, cs->totalSize, chrom);
	restrictGaps(database, id, cs->totalSize, chrom);
	cs->unrestrictedSize = calcUnrestrictedSize(cov, cs->totalSize);
	lastCs = cs;
	}
    /* don't count if few alignments than spCount */
    if ((ali->components->next == NULL) || (cCount < spCount))
        {
        mafAliFree(&ali);
        continue;
        }
    //printf("coverage %d, size %d\n", start, comp->size);
    incNoOverflow(cov+start, comp->size);
    for (comp = ali->components->next; comp != NULL; comp = comp->next)
        {
        tPtr[i] = comp->text;
        i++;
        assert (i < MAXALIGN-1);
        }
    size = 0;
    assert(cs != NULL);
    /* count gapless columns */
    for (j = 0 ; j<=ali->textSize ; j++)
        {
        /* look for aligning bases in query seqs , abort if any is a gap */
        for (i = 1 ; i < cCount ; i++)
            {
            if (tPtr[i][j] == '-' || tPtr[0][j] == '-')
                {
     //   printf("align %d, size %d\n", start, size);
                incNoOverflow(align+start, size);
                cs->totalDepth += size;
                start = nextStart;
                size = 0;
                hit = FALSE;
                break;
                }
            else
                {
                hit = TRUE;
                }
            }
        if (hit)
            size++;
        /* if there is a gap in the target, start a new alignment block*/
        if (tPtr[0][j] != '-')
            nextStart++;
        }
    assert(cs!=NULL);
    end = start+size;
    if (end > cs->totalSize)
	{
        if (cs->name != NULL)
            errAbort("End %d past end %ld of %f\n", end, (long)cs->totalSize, ali->score);
        else
            {
            if (ali!=NULL)
                errAbort("End %d past end %ld %f\n", end, (long)cs->totalSize, ali->score );
            else
                errAbort("End %d past end %ld \n", end, (long)cs->totalSize);
            }
	}
    incNoOverflow(align+start, size-1);
    cs->totalDepth += size-1;

    /* count percent id */
    idSize = 0;
    assert(cs != NULL);
    for (k = 0 ; k<=ali->textSize ; k++)
        {
        char tc = toupper(tPtr[0][k]);
        for (i = 1 ; i < cCount ; i++)
            {
            if (toupper(tPtr[i][k]) != tc || tc == '-' || tc == 'N')
                {
                incNoOverflow(id+idStart, idSize);
                idStart = idNextStart;
                idSize = 0;
                hit = FALSE;
                break;
                }
            else
                {
                hit = TRUE;
                }
            }
        if (hit)
            idSize++;
        /* skip over gaps */
        if (tc != '-')
            idNextStart++;
        }
    assert(cs!=NULL);
    idEnd = idStart+idSize;
    if (idEnd > cs->totalSize)
	{
        if (cs->name != NULL)
            errAbort("End %d past end %ld of %f\n", idEnd, (long)cs->totalSize, ali->score);
        else
            {
            if (ali!=NULL)
                errAbort("End %d past end %ld %f\n", idEnd, (long)cs->totalSize, ali->score );
            else
                errAbort("End %d past end %ld \n", idEnd, (long)cs->totalSize);
            }
	}
    incNoOverflow(id+idStart, idSize-1);
    mafAliFree(&ali);
    }
closeChromCov(fileName, cs, &cov, &align, &id);
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
    g->unrestrictedSize += cs->unrestrictedSize;
    g->seqSize += cs->seqSize;
    g->totalSize += cs->totalSize;
    g->totalDepth += cs->totalDepth;
    g->totalCov += cs->totalCov;
    g->totalAlign += cs->totalAlign;
    g->totalId += cs->totalId;
    for (i=0; i<=maxDepth; ++i)
        g->histogram[i] += cs->histogram[i];
    }
return g;
}

void mafCoverage(char *database, char *mafFile, char *restrictFile, int spCount)
/* mafCoverage - Analyse coverage by maf files - chromosome by 
 * chromosome and genome-wide.. */
{
struct chromSizes *cs, *csList = NULL;
struct hash *chromHash = NULL;
struct chromSizes *genome;

getChromSizes(database, &chromHash, &csList);
if (restrictFile != NULL)
    addRestrictions(chromHash, restrictFile);
scanMaf(database, mafFile, chromHash, restrictFile != NULL, spCount);
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
pushCarefulMemHandler(950000000);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
mafCoverage(argv[1], argv[2], optionVal("restrict", NULL), optionInt("count",DEFCOUNT));
return 0;
}
