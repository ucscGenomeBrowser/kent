/* aliStats - Collect statistics on cross species alignment including
 * how many places a C briggsae sequence aligns and how often things
 * match and mismatch in coding, heavily conserved and lightly conserved
 * regions. */

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "dlist.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "gdf.h"
#include "wormdna.h"
#include "xa.h"

FILE *out;  /* Output file. */

void report(FILE *f, char *format, ...)
/* Write printf formatted report to file and standard out. */
{
va_list args;
va_start(args, format);
vfprintf(f, format, args);
vfprintf(stdout, format, args);
va_end(args);
}

#define uglyr report



FILE *openXa(char *organism)
/* Return file handle for xeno-alignments for given organism. */
{
char path[512];
sprintf(path, "%s%s/all.st", wormXenoDir(), organism);
return xaOpenVerify(path);
}




enum aliHow 
/* Conservation level. */
    {
    ahNone, ahLight, ahHeavy, ahCoding,
    };

enum aliWhere
/* Where something aligns. */
    {
    awIg, awIntron, awCoding,
    };

struct aliClass
/* We keep one of these for each base of a C. briggsae
 * cosmid to classify how and where it aligns. */
    {
    enum aliHow how;
    enum aliWhere where;
    boolean anyMatch;
    int count;
    };

/* Variables that accumulate counts. */
int counts[8][4][3];
int cbTotal;
static int minCounts[8] = {0, 1, 2, 4, 8, 16, 32, 64};
int maxCount = 0;

int addThree(int a[3])
/* Return sum of three. */
{
return a[0] + a[1] + a[2];
}


void printCounts(FILE *f)
/* Print out summary of alignment. */
{
int pIx;
int last = 0;
int totalIntron = 0;
int totalExon = 0;
int totalIg = 0;
int totalAligning = 0;

fprintf(f, "Max # of places to align is %d\n\n", maxCount);

for (pIx = 0; pIx < 8; ++pIx)
    {
    fprintf(f, "rep %d     IG       Intron    Coding    Total\n", minCounts[pIx]-1);
    fprintf(f, "heavy     %8d %8d  %8d %8d\n", 
        counts[pIx][ahHeavy][awIg], counts[pIx][ahHeavy][awIntron], counts[pIx][ahHeavy][awCoding],
        addThree(counts[pIx][ahHeavy]));
    fprintf(f, "light     %8d %8d  %8d %8d\n", 
        counts[pIx][ahLight][awIg], counts[pIx][ahLight][awIntron], counts[pIx][ahLight][awCoding],
        addThree(counts[pIx][ahLight]));
    fprintf(f, "coding     %8d %8d  %8d %8d\n", 
        counts[pIx][ahCoding][awIg], counts[pIx][ahCoding][awIntron], counts[pIx][ahCoding][awCoding],
        addThree(counts[pIx][ahCoding]));
    fprintf(f, "none      %8d %8d  %8d %8d\n", 
        counts[pIx][ahNone][awIg], counts[pIx][ahNone][awIntron], counts[pIx][ahNone][awCoding],
        addThree(counts[pIx][ahNone]));
    fprintf(f, "\n");
    last = minCounts[pIx];
    }

for (pIx = 1; pIx < 8; ++pIx)
    {
    totalIntron += counts[pIx][ahHeavy][awIntron] + counts[pIx][ahLight][awIntron] + counts[pIx][ahCoding][awIntron];
    totalExon += counts[pIx][ahHeavy][awCoding] + counts[pIx][ahLight][awCoding] + counts[pIx][ahCoding][awCoding];
    totalIg += counts[pIx][ahHeavy][awIg] + counts[pIx][ahLight][awIg] + counts[pIx][ahCoding][awIg];
    }

fprintf(f, "Totals: IG %d Intron %d coding %d\n", totalIg, totalIntron, totalExon);
fprintf(f, "Total Aligning %d\n", totalIntron+totalExon+totalIg);
fprintf(f, "Total C. briggsae bases %d\n", cbTotal);
}

boolean checkStitching(struct aliClass *tt, int ttCount, struct xaAli *xa)
/* Check that we don't have an irrational stitching or something. */
{
int symCount = xa->symCount;
int qIx = xa->qStart;
char *qSym = xa->qSym;
int i;
char q;

for (i=0; i<symCount; ++i)
    {
    q = qSym[i];
    if (q != '-')
        {
        if (qIx >= ttCount)
            {
            report(out, "warning - %s: ttCount %d exceeded by index qIx %d\n", xa->name, ttCount, qIx);
            return FALSE;
            }
        ++qIx;
        }
    }
return TRUE;
}

void recordTypes(struct aliClass *tt, int ttCount, struct xaAli *xa)
/* Record base-by-base alignment characteristics of query side in
 * tt. */
{
int symCount = xa->symCount;
int qIx = xa->qStart;
char *qSym = xa->qSym;
char *hSym = xa->hSym;
char *tSym = xa->tSym;
int i;
char t,q,h;
int hVal;

for (i=0; i<symCount; ++i)
    {
    q = qSym[i];
    t = tSym[i];
    if (q != '-')
        {
        assert(qIx < ttCount);
        h = hSym[i];
        if (h == 'L')
            hVal = ahLight;
        else if (h == '1' || h == '2' || h == '3')
	    hVal = ahCoding;
	else if (h == 'H')
            hVal = ahHeavy;
        else
            hVal = ahNone;
        if (hVal > tt[qIx].how)
            tt[qIx].how = hVal;
        if (hVal != ahNone)
            {
            int count = (tt[qIx].count += 1);
            if (count > maxCount)
                maxCount = count;
            }
        if (q == t)
            tt[qIx].anyMatch = TRUE;
        ++qIx;
        }
    }
}

void setCorrespondingSeg(struct xaAli *xa, struct aliClass *typeTable, int typeCount,
    enum aliWhere setVal, int tStart, int tEnd)
/* Set the bits of typeTable (in query coordinates) that correspond to the
 * target coordinates from tStart to tEnd to setVal.  
 * For debugging purposes returns TRUE when count typeTable[i].count is zero for
 * part of the seg. */
{
int tIx = xa->tStart;
int qIx = xa->qStart;
int symCount = xa->symCount;
char *qSym = xa->qSym;
char *tSym = xa->tSym;
char q,t;
int i;

for (i=0; i<symCount; ++i)
    {
    q = qSym[i];
    t = tSym[i];
    if (q != '-')
        {
        if (tIx >= tStart && tIx < tEnd)
            {
            assert(qIx < typeCount);
            if (typeTable[qIx].where < setVal)
                typeTable[qIx].where = setVal;
            }
        ++qIx;
        }    
    if (tIx != '-')
        ++tIx;
    }
}

void setCorresponding(struct gdfGene *gdf, struct xaAli *xa, struct aliClass *typeTable, int typeCount)
/* typeTable corresponds to the query (C. briggsae sequence) in xa.  Gdf is in terms
 * of the target (C. elegans sequence).  Set the where parts of typeTable to be 
 * intron or exon depending on the classification in gdf of the corresponding
 * parts of C. elegans. */
{
struct gdfDataPoint *dp = gdf->dataPoints;
int dpCount = gdf->dataCount;
int i;

/* Do exons */
for (i=0; i<dpCount; i += 2)
    {
    setCorrespondingSeg(xa, typeTable, typeCount, awCoding, dp[i].start, dp[i+1].start);
    }

/* Do introns */
for (i=1; i<dpCount-1; i += 2)
    {
    setCorrespondingSeg(xa, typeTable, typeCount, awIntron, dp[i].start, dp[i+1].start);
    }
}

void  recordWhere(struct xaAli *xaList, struct aliClass *typeTable, int typeCount)
/* Record where each base is (intron/exon/IG) and how many times it is aligned to. */
{
struct xaAli *xa;
struct gdfGene *gdfList, *gdf;

/* Fetch C. elegans region. */
for (xa = xaList; xa != NULL; xa = xa->next)
    {
    gdfList = wormGdfGenesInRange(xa->target, xa->tStart, xa->tEnd, &wormSangerGdfCache);
    for (gdf = gdfList; gdf != NULL; gdf = gdf->next)
        {
        setCorresponding(gdf, xa, typeTable, typeCount);
        }
    gdfFreeGeneList(&gdfList);
    }
}

int countIx(int x)
/* Return where to lump in count */
{
int i;

for (i=ArraySize(minCounts)-1; i >= 0; i -= 1)
    {
    if (x >= minCounts[i])
        return i;
    }
return 0;
}

void unmanglePath(char **pOldPath)
/* Replace truncated oldPath with full version
 * MS-DOS truncated old path... */
{
char *new = NULL;
char dir[256], name[128], extension[64];
char newPath[512];

splitPath(*pOldPath, dir, name, extension);
if (sameString(dir, "ACTIN2~1/"))
    new = "actin2_cluster/";
else if (sameString(dir, "ACTIN5~1/"))
    new = "actin5_cluster/";
else if (sameString(dir, "ACTIN_~1/"))
    new = "actin_cluster/";
else if (sameString(dir, "CBC52E~1/"))
    new = "cbC52E4_6/";
else if (sameString(dir, "CBF22E~1/"))
    new = "cbF22E10_3/";
else if (sameString(dir, "CBT07A~1/"))
    new = "cbT07A9_6/";
else if (sameString(dir, "CBLBP_~1/"))
    new = "cblbp_cblbp_2/";
else
    {
    tolowers(dir);
    new = dir;
    }
sprintf(newPath, "%s%s%s", new, name, extension);
*pOldPath = cloneString(newPath);
}

void addCounts(struct xaAli *xa)
/* Add in alignment to statistics.  
 * This routine updates the counts based
 * on all xa's from a given C. briggsae cosmid.
 * Call it with xa = NULL to "close" off the
 * last cosmid. */
{
static char lastCosmidName[256] = "";
static struct aliClass *typeTable = NULL;
static int typeCount = 0;
static struct xaAli *xaList = NULL;
boolean newName = FALSE;

if (xa != NULL)
    {
    newName = differentWord(xa->query, lastCosmidName);
    }

/* Save data gathered from this C. briggsae cosmid. */
if (typeTable != NULL && (xa == NULL || newName))
    {
    int i;

    recordWhere(xaList, typeTable, typeCount);
    xaAliFreeList(&xaList);
    for (i=0; i<typeCount; ++i)
        {
        struct aliClass *tt = &typeTable[i];
        int repIx = countIx(tt->count);
        counts[repIx][tt->how][tt->where] += 1;
        }
    cbTotal += typeCount;
    }

if (xa == NULL)
    return;

if (newName)
    {
    char *cbSeqDir = "/d/bioData/cbriggsae/finish/";
    char path[512];
    struct dnaSeq *seq;

    strcpy(lastCosmidName, xa->query);
    unmanglePath(&xa->query);
    sprintf(path, "%s%s.seq", cbSeqDir, xa->query);
    seq = faReadDna(path);
    typeCount = seq->size;
    freeDnaSeq(&seq);
    freeMem(typeTable);
    typeTable = needMem(typeCount * sizeof(typeTable[0]));
    }

if (checkStitching(typeTable, typeCount, xa))
    {
    recordTypes(typeTable, typeCount, xa);
    slAddHead(&xaList, xa);
    }
}

int match[128];     /* Match count for each HMM symbol. */
int mismatch[128];   /* Mismatch count for each HMM symbol. */

void addMatches(struct xaAli *xa)
/* Add in match and mismatch counts for each symbol. */
{
int i;
int symCount = xa->symCount;
char *qSym = xa->qSym;
char *tSym = xa->tSym;
char *hSym = xa->hSym;
for (i=0; i<symCount; ++i)
    {
    if (qSym[i] == tSym[i])
        ++match[hSym[i]];
    else
        ++mismatch[hSym[i]];
    }
}

void printMatches()
/* Print out match/mismatch stats */
{
char *syms = "123HL";
char sym;

printf("\n");
while ((sym = *syms++) != 0)
    {
    printf("Sym  Match   MisMatch   Total\n");
    printf(" %c   %7d %7d %7d\n", sym, match[sym], mismatch[sym], match[sym]+mismatch[sym]);
    }
}

int insertStarts;
int insertTotal;
int atMatchTotal;
int matchTotal;
int mismatchTotal;
int blockCount;
int cosmidCount;
int aliSizeTotal;

void addCompStats(struct xaAli *xa, struct hash *qNameHash)
/* Add in stats for blast comparison. */
{
int i;
char lastH = 0, h;
int symCount = xa->symCount;
char *qSym = xa->qSym;
char *tSym = xa->tSym;
char *hSym = xa->hSym;

++blockCount;
if (!hashLookup(qNameHash, xa->query))
    {
    ++cosmidCount;
    hashAdd(qNameHash, xa->query, NULL);
    }
aliSizeTotal += xa->qEnd - xa->qStart;
for (i=0; i<symCount; ++i)
    {
    h = hSym[i];
    if (h != lastH)
	{
	if (h == 'Q' || h == 'T')
	    ++insertStarts;
	lastH = h;
	}
    if (h == 'Q' || h == 'T')
	++insertTotal;
    else if (qSym[i] == tSym[i])
	{
	++matchTotal;
	if (qSym[i] == 'a' || qSym[i] == 't')
	    ++atMatchTotal;
	}
    else
        ++mismatchTotal;
    }
}

void printCompStats(FILE *out)
/* Print stats for blast comparison. */
{
fprintf(out, "\nBlast Comparison Data - WABA side\n");
fprintf(out, "------------------------------------\n");
fprintf(out, "Cosmids w/ alignments %d\n", cosmidCount);
fprintf(out, "Total matches %d\n", matchTotal);
fprintf(out, "Total mismatches %d\n", mismatchTotal);
fprintf(out, "Number of inserts %d\n", insertStarts);
fprintf(out, "Total bases inserted %d\n", insertTotal);
fprintf(out, "Number of alignments %d\n", blockCount);
fprintf(out, "Average alignment size %f\n", (double)aliSizeTotal/blockCount);
fprintf(out, "AT%% of matches %f\n", 100.0 * atMatchTotal/matchTotal);
}



int main(int argc, char *argv[])
{
FILE *xaFile = openXa("cbriggsae");
struct xaAli *xa;
int count = 0;
long startTime = clock1000();
struct hash *qNameHash = newHash(0);

dnaUtilOpen();
out = mustOpen("aliStats.out", "w");
while ((xa = xaReadNext(xaFile, FALSE)) != NULL)
    {
    if (++count % 500 == 0)
        printf("Processing %d\n", count);
    addCounts(xa);
    addMatches(xa);
    addCompStats(xa, qNameHash);
    }
addCounts(NULL);
printCounts(out);
printMatches(out);
printCompStats(out);
printCompStats(stdout);
report(out, "Processing took %f seconds\n", (clock1000()-startTime)*0.001);
return 0;
}

