/* clusterRna - Make clusters of mRNA and ESTs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "binRange.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "estOrientInfo.h"
#include "ggMrnaAli.h"
#include "geneGraph.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterRna - Make clusters of mRNA and ESTs\n"
  "usage:\n"
  "   clusterRna database mrnaOut.bed estOut.bed\n"
  "options:\n"
  "   -MGC=mgc.out - output MGC ESTs to sequence fully\n"
  "   -chrom=chrN - work on chrN (default chr22)\n"
  );
}

struct psl *loadPsls(struct sqlConnection *conn, char *table, char *chromName)
/* Load all psls from chromosome in table. */
{
struct psl *pslList = NULL, *psl;
int rowOffset;
struct sqlResult *sr;
char **row;

sr = hChromQuery(conn, table, chromName, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
printf("Loaded %d from %s\n", slCount(pslList), table);
return pslList;
}

struct ggMrnaAli *maFromSomePsls(struct psl *pslList,
    char *chromName, struct dnaSeq *chrom, 
    struct hash *includeHash, boolean assumeForward,
    boolean isRefSeq)
/* Convert list of psl's to list of ma's.  
 * If includeHash is non-null, only convert those on hash. */
{
struct psl *psl;
struct ggMrnaAli *list = NULL, *el;
int intronDir;

for (psl = pslList; psl != NULL; psl = psl->next)
    {
    struct estOrientInfo *eoi = NULL;
    if (includeHash == NULL || (eoi = hashFindVal(includeHash, psl->qName)) != NULL)
	{
	el = pslToGgMrnaAli(psl, chromName, 0, chrom->size, chrom);
	ggMrnaAliMergeBlocks(el, 5);
	intronDir = pslWeightedIntronOrientation(psl, chrom, 0) * 2;
	if (assumeForward)
	    {
	    int bias = (psl->strand[0] == '-' ? -1 : 1);
	    if (bias * intronDir < 0) uglyf("%s %s %d\n", psl->qName, psl->strand, intronDir);
	    intronDir += bias;
	    }
	el->orientation = intronDir;
	if (isRefSeq)
	    el->orientation *= 2;
	slAddHead(&list, el);
	}
    }
slReverse(&list);
return list;
}

void orientEstMa(struct ggMrnaAli *maList, struct hash *eoiHash)
/* Orient alignments according to eoi. */
{
struct ggMrnaAli *ma;
struct estOrientInfo *eoi;

for (ma = maList; ma != NULL; ma = ma->next)
    {
    eoi = hashFindVal(eoiHash, ma->qName);
    ma->orientation += eoi->sizePolyA/6;
    ma->orientation -= eoi->revSizePolyA/6;
    if (eoi->signalPos > 0) ma->orientation += 1;
    if (eoi->revSignalPos > 0) ma->orientation -= 1;
    }
}


struct ggMrnaAli *maFromPslTable(struct sqlConnection *conn, char *table, 
	char *chromName, struct dnaSeq *chrom, boolean assumeForward,
	boolean isRefSeq)
/* Load all psl's from a given table on chromosome and conver to ma's. */
{
struct psl *pslList, *psl;
struct ggMrnaAli *maList;

pslList = loadPsls(conn, table, chromName);
maList = maFromSomePsls(pslList, chromName, chrom, NULL, assumeForward,
   isRefSeq);
pslFreeList(&pslList);
return maList;
}

struct estOrientInfo *loadEstOrientInfo(struct sqlConnection *conn, 
	char *chromName)
/* Return list EST infos. */
{
struct estOrientInfo *eoiList = NULL, *eoi;
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, "estOrientInfo", chromName, NULL, &rowOffset);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    eoi = estOrientInfoLoad(row + rowOffset);
    slAddHead(&eoiList, eoi);
    }
sqlFreeResult(&sr);
slReverse(&eoiList);
return eoiList;
}

void categorizeEsts(struct estOrientInfo *eoiList, 
	struct hash *splicedHash, struct hash *tailedHash, 
	struct hash *otherHash)
/* Lump ESTs into one of three categories and put in corresponding
 * hash */
{
struct estOrientInfo *eoi;
int splicedCount = 0, tailedCount = 0, otherCount = 0;
for (eoi = eoiList; eoi != NULL; eoi = eoi->next)
    {
    struct hash *hash = NULL;
    if (eoi->intronOrientation != 0)
        {
	hash = splicedHash;
	++splicedCount;
	}
    else 
	{
	int strand = 0;
	if (eoi->signalPos)
	     strand += 6;
	if (eoi->revSignalPos)
	     strand -= 6;
	strand += eoi->sizePolyA;
	strand -= eoi->revSizePolyA;
	if (strand >= 6 || strand <= -6)
	    {
	    hash = tailedHash;
	    ++tailedCount;
	    }
	else
	    {
	    hash = otherHash;
	    ++otherCount;
	    }
	}
    hashAdd(hash, eoi->name, eoi);
    }
printf("Got %d spliced, %d tailed, %d other ests\n", splicedCount, tailedCount, otherCount);
}


int exonOverlap(struct ggMrnaCluster *cluster, int eStart, int eEnd)
/* Return the approximate number of bases overlapped between
 * cluster and an exon. */
{
/* This approximates overlap by taking biggest overlap with
 * any mRNA in cluster. */
struct ggAliInfo *da;
int oneOverlap, overlap = 0;

for (da = cluster->mrnaList; da != NULL; da = da->next)
    {
    struct ggVertex *v = da->vertices;
    int vCount = da->vertexCount;
    int i;
    for (i=0; i<vCount; i+=2,v+=2)
        {
        int mStart = v[0].position;
        int mEnd = v[1].position;
        int s = max(eStart, mStart);
        int e = min(eEnd, mEnd);
	oneOverlap = e - s;
	if (oneOverlap > overlap)
	    overlap = oneOverlap;
        }
    }
return overlap;
}


int exonsOverlapCluster(struct ggMrnaCluster *cluster, struct ggMrnaCluster *ali)
/* Return the number of bases that overlap between cluster and alignment.
 * Alignment (ali) is a cluster too, but only with a single alignment in 
 * it. */
{
struct ggAliInfo *da = ali->mrnaList;
int i, vCount = da->vertexCount;
struct ggVertex *v = da->vertices;
int overlap = 0;

assert(da->next == NULL);
for (i=0; i<vCount; i+=2,v+=2)
    overlap += exonOverlap(cluster, v[0].position, v[1].position);
return overlap;
}

struct ggMrnaCluster *ggMrnaSoftClusterOfOne(struct ggMrnaAli *ma, 
	struct dnaSeq *genoSeq);
/* Make up a ggMrnaCluster with just one thing on it. */
/* Defined in ggCluster. */

struct ggMrnaCluster *ggMrnaSoftFilteredClusterOfOne(struct ggMrnaAli *ma,
        struct dnaSeq *genoSeq, int minExonSize, int minNonSpliceExon);
/* Make up a ggMrnaCluster with just one thing on it.
 * All edges here will be soft (not intended for alt-splicing use) */


void ggMrnaClusterMerge(struct ggMrnaCluster *aMc, struct ggMrnaCluster *bMc);
/* Merge bMc into aMc.  Afterwords aMc will be bigger and
 * bMc will be gone. */


void outputClusters(struct binKeeper *bins, char *chromName, int chromSize, 
	char *fileName)
/* Write out clusters to file. */
{
struct binElement *el, *list = binKeeperFindSorted(bins, 0, chromSize);
struct ggMrnaCluster *cluster;
int index = 0;
int score, start, end;
FILE *f = mustOpen(fileName, "w");
for (el = list; el != NULL; el = el->next)
    {
    cluster = el->val;
    score = 300 * slCount(cluster->refList);
    if (score > 1000) score = 1000;
    start = cluster->tStart;
    end = cluster->tEnd;
    fprintf(f, "%s\t%d\t%d\t%s.%d\t%d\t%s\t%d\t%d\t0\t2\t1,1,\t%d,%d\n",
    	chromName, start, end, chromName, ++index, 
	score, cluster->strand, start, end,
	0, end - start - 1);
    }
slFreeList(&list);
carefulClose(&f);
}

void addCluster(struct ggMrnaAli *maList, struct binKeeper *bins,
	struct hash *eoiHash,  struct dnaSeq *chrom)
/* Do basic clustering. */
{
struct ggMrnaAli *ma, *nextMa;
struct ggMrnaCluster *newCluster, *oldCluster;
struct estOrientInfo *eoi;
struct binElement *binList = NULL, *binEl, *nextBinEl, *overlapList = NULL;

for (ma = maList; ma != NULL; ma = nextMa)
    {
    int overlap = 0, overlapSign = 0;
    char targetStrand = '?';
    nextMa = ma->next;

    //uglyf("Processing %s: %d-%d, orient %d\n", ma->qName, ma->tStart, ma->tEnd, ma->orientation);

    /* Make up cluster for this RNA and merge with overlapping clusters
     * if any. */
    newCluster = ggMrnaSoftFilteredClusterOfOne(ma, chrom, 10, 130);
    if (newCluster == NULL)
        {
	continue;
	}

    /* Get list of overlapping clusters on either strand. */
    binList = binKeeperFind(bins, newCluster->tStart, newCluster->tEnd);

    /* Weed out clusters that don't overlap at exon level as well. 
     * While we're at this calculate predominant strand of overlap. */
    overlapList = NULL;
    for (binEl = binList; binEl != NULL; binEl = nextBinEl)
	{
	boolean gotOverlap = FALSE;
	nextBinEl = binEl->next;
	oldCluster = binEl->val;
	overlap = exonsOverlapCluster(oldCluster, newCluster);
	// uglyf(" oldCluster %d,%d %s exonOverlap %d strand %s\n", binEl->start, binEl->end, oldCluster->refList->ma->qName, overlap, oldCluster->strand);
	if (overlap > 0)
	    {
	    slAddHead(&overlapList, binEl);
	    if (oldCluster->strand[0] == '+')
		overlapSign += overlap;
	    else if (oldCluster->strand[0] == '-')
		overlapSign -= overlap;
	    }
	else
	    {
	    freez(&binEl);
	    }
	}
    binList = NULL;

    /* Figure out strand to cluster on. */
    if (overlapSign != 0 && ma->orientation == 0)
        {
	targetStrand = (overlapSign < 0 ? '-' : '+');
	}
    else
        {
	if (ma->orientation < 0)
	    targetStrand = '-';
	else if (ma->orientation > 0)
	    targetStrand = '+';
	else
	    targetStrand = '?';
	}


    /* Weed out clusters that are on the wrong strand. */
    binList = overlapList;
    overlapList = NULL;
    for (binEl = binList; binEl != NULL; binEl = nextBinEl)
        {
	nextBinEl = binEl->next;
	oldCluster = binEl->val;
	if (oldCluster->strand[0] == '?' || oldCluster->strand[0] == targetStrand)
	    {
	    slAddHead(&overlapList, binEl);
	    }
	else
	    {
	    freez(&binEl);
	    }
	}
    binList = NULL;


    /* Merge new  cluster for this RNA and merge with overlapping 
     * old clusters if any. */
    for (binEl = overlapList; binEl != NULL; binEl = binEl->next)
        {
	struct ggMrnaCluster *oldCluster = binEl->val;
	char oldStrand = oldCluster->strand[0];

	// uglyf("  merging with old cluster %d-%d\n", oldCluster->tStart, oldCluster->tEnd);
	binKeeperRemove(bins, oldCluster->tStart, oldCluster->tEnd, oldCluster);
	ggMrnaClusterMerge(newCluster, oldCluster);
	if (oldStrand != '?')
	    newCluster->strand[0] = oldStrand;
	// uglyf("  afterMerge - cluster goes %d-%d\n", newCluster->tStart, newCluster->tEnd);
	}
    
    /* Add new cluster to bin and clean up. */
    binKeeperAdd(bins, newCluster->tStart, newCluster->tEnd, newCluster);
    slFreeList(&overlapList);
    }
}

int clusterScore(struct ggMrnaCluster *cluster)
/* Return score of cluster - based on combination of
 * how orientable it is and number of elements in it. */
{
struct maRef *ref;
struct ggMrnaAli *ma;   
int orientation = 0, count = 0;

for (ref = cluster->refList; ref != NULL; ref = ref->next)
    {
    ma = ref->ma;
    orientation += ma->orientation;
    count += 1;
    }
if (orientation < 0)
    orientation = -orientation;
return count + orientation;
}

void weedWeakClusters(struct binKeeper *bins, int chromSize)
/* Remove clusters that are small and poorly oriented. */
{
struct binElement *el, *list = binKeeperFind(bins, 0, chromSize);

uglyf("%d elements before weeding\n", slCount(list));
for (el = list; el != NULL; el = el->next)
    {
    int score = clusterScore(el->val);
    if (score < 3)
	{
	struct ggMrnaCluster *cluster = el->val;
        binKeeperRemove(bins, el->start, el->end, cluster);
	}
    }
slFreeList(&list);
list = binKeeperFind(bins, 0, chromSize); uglyf("%d elements after weeding\n", slCount(list)); slFreeList(&list);
}

int ggAliInfoEndPos(struct ggAliInfo *da)
/* Return end of ggAliInfo in chromosome */
{
return da->vertices[da->vertexCount-1].position;
}

int ggAliInfoStartPos(struct ggAliInfo *da)
/* Return start of ggAliInfo in chromosome */
{
return da->vertices[0].position;
}

int daAliSize(struct ggAliInfo *da)
/* Return size of bases in query side of alignment. */
{
struct ggVertex *v = da->vertices;
int i, size = 0, vCount = da->vertexCount;
for (i=0; i<vCount; i += 2, v += 2)
    size += v[1].position - v[0].position;
return size;
}

boolean goodDa(struct ggAliInfo *da)
/* Return TRUE if da looks good. */
{
return daAliSize(da) > 0.8 * da->ma->baseCount;
}

int findStrandDistance(struct ggAliInfo *da, char strand, int start, int end)
/* Find distance from da to 5' end of cluster. */
{
if (strand == '-')
    return end - ggAliInfoEndPos(da);
else 
    return ggAliInfoStartPos(da) - start;
}

void findBestFivePrime(struct ggMrnaCluster *cluster, struct hash *hash,
	struct ggAliInfo **retClosest,  struct ggAliInfo **retClosestGood)
/* Return closest to 5' end element in cluster that is also in hash.
 * Also return closest element without apparent problems. */
{
struct ggAliInfo *da, *closest = NULL, *closestGood = NULL;
int minDistance = BIGNUM, minGoodDistance = BIGNUM, distance;
char strand = cluster->strand[0];
int start = cluster->tStart, end = cluster->tEnd;

assert(strand == '+' || strand == '-');
for (da = cluster->mrnaList; da != NULL; da = da->next)
    {
    char *acc = da->ma->qName;
    if (hashLookup(hash, acc))
	{
	distance = findStrandDistance(da, strand, start, end);
	if (distance < minDistance)
	    {
	    minDistance = distance;
	    closest = da;
	    }
	if (distance < minGoodDistance && goodDa(da))
	    {
	    minGoodDistance = distance;
	    closestGood = da;
	    }
	}
    }
*retClosest = closest;
*retClosestGood = closestGood;
}

void printOneDaInfo(FILE *f, char strand, int start, int end, struct ggAliInfo *da)
/* Print a couple of columns of info */
{
if (da == NULL)
    fprintf(f, "\tn/a     \tn/a\tn/a");
else
    fprintf(f, "\t%-8s\t%d\t%4.2f%%", da->ma->qName, 
    	findStrandDistance(da, strand, start, end), 
	100.0*daAliSize(da)/da->ma->baseCount);
}

void printClosestInfo(FILE *f, int start, int end, struct ggMrnaCluster *cluster, 
	struct hash *hash, boolean both)
/* Calculate and print a couple of columns of info about elements of cluster
 * that are also in hash. */
{
char strand = cluster->strand[0];
struct ggAliInfo *closest, *good;

findBestFivePrime(cluster, hash, &closest, &good);
printOneDaInfo(f, strand, start, end, closest);
if (both)
    printOneDaInfo(f, strand, start, end, good);
}

struct hash *hashRow0(struct sqlConnection *conn, char *query)
/* Return hash of row0 of return from query. */
{
struct hash *hash = newHash(0);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int count = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[0], NULL);
    ++count;
    }
sqlFreeResult(&sr);
uglyf("%d match %s\n", count, query);
return hash;
}


struct hash *hashRefSeq(struct sqlConnection *conn)
/* Get list of all RefSeq mRNAs */
{
return hashRow0(conn, "select mrnaAcc from refLink");
}

struct hash *wildHash(struct sqlConnection *conn, char *table, char *pattern)
/* Read an id/name table, and return hash keyed by id
 * for all members that match pattern. */
{
char query[256];
sprintf(query, "select id from %s where name like '%s'", table, pattern);
return hashRow0(conn, query);
}

void outputMgc(struct sqlConnection *conn, 
	struct binKeeper *bins, char *chromName,
	int chromSize, char *fileName)
/* Output info about clusters relevant to MGC */
{
struct hash *refSeqHash = hashRefSeq(conn);
struct hash *mgcEstHash = newHash(20);
struct hash *mgcRnaHash = newHash(0);
struct hash *rnaHash = newHash(0);
struct hash *estAuthorHash = wildHash(conn, "author", "NIH-MGC%");
struct hash *rnaAuthorHash = wildHash(conn, "author", "Strausberg,R.");
struct sqlResult *sr = NULL;
char **row;
FILE *f = mustOpen(fileName, "w");
struct binElement *el, *list = binKeeperFindSorted(bins, 0, chromSize);

/* Read mRNA table and split into hashes. */
uglyf("Scanning mrna table");
sr = sqlGetResult(conn, "select acc,type,author from mrna");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *acc = row[0], *type = row[1], *author = row[2];
    if (sameString(type, "EST"))
        {
	if (hashLookup(estAuthorHash, author))
	    hashAdd(mgcEstHash, acc, NULL);
	}
    else
        {
	if (hashLookup(rnaAuthorHash, author))
	    hashAdd(mgcRnaHash, acc, NULL);
	else
	    hashAdd(rnaHash, acc, NULL);
	}
    }
sqlFreeResult(&sr);

uglyf("Printing %s\n", fileName);
fprintf(f, "#chrom\tstart    \tend      \tstrand\tMGC EST \t5' dist\tAli%%\tGood EST\t5' dist\tAli%%\tMGC RNA \t5' dist\tAli%%\tGood RNA\t5' dist\tAli%%\tOther RNA \t5' dist\tAli%%\tRefSeq  \t5' dist\tAli%%\n");
for (el = list; el != NULL; el = el->next)
    {
    struct ggMrnaCluster *cluster = el->val;
    int start = el->start, end = el->end;
    fprintf(f, "%s\t%d\t%d\t%s", chromName, start, end, cluster->strand);
    printClosestInfo(f, start, end, cluster, mgcEstHash, TRUE);
    printClosestInfo(f, start, end, cluster, mgcRnaHash, TRUE);
    printClosestInfo(f, start, end, cluster, rnaHash, FALSE);
    printClosestInfo(f, start, end, cluster, refSeqHash, FALSE);
    fprintf(f, "\n");
    }
carefulClose(&f);
hashFree(&mgcEstHash);
hashFree(&mgcRnaHash);
slFreeList(&list);
}

void clusterChromRna(struct sqlConnection *conn, char *chromName,
	char *rnaOut, char *estOut)
/* Cluster RNA on one chromosome. */
{
struct ggMrnaAli *refSeqMaList = NULL, *mrnaMaList = NULL, 
	*splicedEstMaList = NULL, *tailedEstMaList = NULL, 
	*otherEstMaList = NULL;
struct dnaSeq *chrom = NULL;
struct hash *splicedHash = newHash(20), *tailedHash = newHash(20),
            *otherHash = newHash(20);
struct estOrientInfo *eoiList = NULL;
struct psl *estPslList = NULL;
struct ggMrnaCluster *clusterList = NULL, *cluster;
struct binKeeper *bins = NULL;
char *mgcOut = cgiOptionalString("MGC");

chrom = hLoadChrom(chromName);
printf("Loaded %d bases in %s\n", chrom->size, chromName);

/* Load EST info and use it to categorize ESTs. */
eoiList = loadEstOrientInfo(conn, chromName);
uglyf("Loaded %d eoi's\n", slCount(eoiList));
categorizeEsts(eoiList, splicedHash, tailedHash, otherHash);

/* Load mRNA. */
refSeqMaList = maFromPslTable(conn, "refSeqAli", chromName, 
	chrom, TRUE, TRUE);
mrnaMaList = maFromPslTable(conn, "mrna", chromName, 
	chrom, FALSE, FALSE);

/* Load ESTs into three separate lists. */
estPslList = loadPsls(conn, "est", chromName);
splicedEstMaList = maFromSomePsls(estPslList, chromName, 
	chrom, splicedHash, FALSE, FALSE);
tailedEstMaList = maFromSomePsls(estPslList, chromName, 
	chrom, tailedHash, FALSE, FALSE);
otherEstMaList = maFromSomePsls(estPslList, chromName, 
	chrom, otherHash, FALSE, FALSE);

/* Create bin to put clusters in, and cluster
 * refSeq and mRNAs. */
bins = binKeeperNew(0, chrom->size);
addCluster(refSeqMaList, bins, NULL, chrom);
uglyf("Added refSeqMrna to bins ok.\n");
addCluster(mrnaMaList, bins, NULL, chrom);
uglyf("Added tightMrna to bins ok.\n");
weedWeakClusters(bins, chrom->size);
outputClusters(bins, chromName, chrom->size, rnaOut);

/* Cluster ESTs. */
addCluster(splicedEstMaList, bins, splicedHash, chrom);
// addCluster(tailedEstMaList, bins, tailedHash, chrom);
// addCluster(otherEstMaList, bins, otherHash, chrom);
// orientEstMa(tailedEstMaList, tailedHash);
weedWeakClusters(bins, chrom->size);
outputClusters(bins, chromName, chrom->size, estOut);

/* TODO: Merge clusters that share 3'/5' ESTs. */

if (mgcOut != NULL)
    outputMgc(conn, bins, chromName, chrom->size, mgcOut);
}

void clusterRna(char *database, char *rnaOut, char *estOut)
/* clusterRna - Make clusters of mRNA and ESTs. */
{
struct sqlConnection *conn;
hSetDb(database);
conn = hAllocConn();
clusterChromRna(conn, cgiUsualString("chrom", "chr22"), rnaOut, estOut);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
clusterRna(argv[1], argv[2], argv[3]);
return 0;
}
