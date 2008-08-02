/* clusterRna - Make clusters of mRNA and ESTs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "binRange.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "estOrientInfo.h"
#include "ggMrnaAli.h"
#include "geneGraph.h"

static char const rcsid[] = "$Id: clusterRna.c,v 1.15.116.2 2008/08/02 04:06:19 markd Exp $";

/* Global variables set for sorting alignments. */
char *clusterStrand = NULL;
int clusterStart =-1;
int clusterEnd   =-1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterRna - Make clusters of mRNA and ESTs\n"
  "usage:\n"
  "   clusterRna database rnaOut.bed estOut.bed\n"
  "options:\n"
  "   -MGC=mgc.out - output MGC ESTs to sequence fully\n"
  "   -mgcNumPicks=maxNumPicks - number of MGC clones to pick. Default 10.\n"
  "   -mgcMaxDist=max5'Dist - maximum distance from 5' end that MGC clone can be. Default 100.\n"
  "   -mrnaExclude=file.accessions - file of mRNA accessions to exclude from consideration.\n"
  "   -mRNAOrient=mrnaOrientTable - table for mRNA orientation and splicing. Default mrnaOrientInfo.\n"
  "   -chrom=chrN - work on chrN (default chr22)\n"
  "   -rna=rnaTable - table to use for mRNA\n"
  "   -est=estTable - table to use for ESTs\n"
  "   -noEst  - Don't do EST part of job\n"
  "   -noRefSeq  - Don't do refSeq part of job\n"
  "   -orient=orientTable - table to use for EST orientation\n"
  "   -group=group.out - produce list of mRNA/EST in cluster\n"
  );
}

static struct optionSpec options[] = {
   {"MGC", OPTION_STRING},
   {"mgcNumPicks", OPTION_INT},
   {"mgcMaxDist", OPTION_INT},
   {"mrnaExclude", OPTION_STRING},
   {"mRNAOrient", OPTION_STRING},
   {"chrom", OPTION_STRING},
   {"rna", OPTION_STRING},
   {"est", OPTION_STRING},
   {"noEst", OPTION_BOOLEAN},
   {"noRefSeq", OPTION_BOOLEAN},
   {"orient", OPTION_STRING},
   {"group", OPTION_STRING},
   {NULL, 0},
};


struct ggAiSorter
/* Singly linked list that holds a ggAliInfo, used for storing and sorting them. */
{
    struct ggAiSorter *next;  /* Next in list. */
    struct ggAliInfo *ai;          /* Aligment structure. */
};

struct psl *loadPsls(struct sqlConnection *conn, char *table, char *chromName)
/* Load all psls from chromosome in table. */
{
struct psl *pslList = NULL, *psl;
int rowOffset;
struct sqlResult *sr;
char **row;
struct hTableInfo *hti = hFindTableInfo(sqlGetDatabase(conn), chromName, table);
if(hti == NULL)
    errAbort("clusterRna::loadPsl() - Table %s doesn't exist for chrom %s.", table, chromName);
sr = hChromQuery(conn, table, chromName, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
verbose(2, "Loaded %d from %s\n", slCount(pslList), table);
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
	    if (bias * intronDir < 0) verbose(1, "%s %s %d\n", psl->qName, psl->strand, intronDir);
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
struct psl *pslList;
struct ggMrnaAli *maList;

pslList = loadPsls(conn, table, chromName);
maList = maFromSomePsls(pslList, chromName, chrom, NULL, assumeForward,
   isRefSeq);
pslFreeList(&pslList);
return maList;
}

struct estOrientInfo *loadEstOrientInfo(struct sqlConnection *conn, 
	char *chromName, char *orientTable)
/* Return list EST infos. */
{
struct estOrientInfo *eoiList = NULL, *eoi;
int rowOffset;
struct sqlResult *sr;
char **row;

verbose(2, "LoadEstOrientInfo(%s)\n", chromName);
sr = hChromQuery(conn, orientTable, chromName, NULL, &rowOffset);
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
verbose(2, "Got %d spliced, %d tailed, %d other ests\n", splicedCount, tailedCount, otherCount);
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
	FILE *f)
/* Write out clusters to file. */
{
struct binElement *el, *list = binKeeperFindSorted(bins, 0, chromSize);
struct ggMrnaCluster *cluster;
int index = 0;
int score, start, end;
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
}

void addCluster(struct ggMrnaAli *maList, struct binKeeper *bins,
	struct hash *eoiHash,  struct dnaSeq *chrom)
/* Do basic clustering. */
{
struct ggMrnaAli *ma, *nextMa;
struct ggMrnaCluster *newCluster, *oldCluster;
struct binElement *binList = NULL, *binEl, *nextBinEl, *overlapList = NULL;

for (ma = maList; ma != NULL; ma = nextMa)
    {
    int overlap = 0, overlapSign = 0;
    char targetStrand = '?';
    nextMa = ma->next;

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
	nextBinEl = binEl->next;
	oldCluster = binEl->val;
	overlap = exonsOverlapCluster(oldCluster, newCluster);
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

	binKeeperRemove(bins, oldCluster->tStart, oldCluster->tEnd, oldCluster);
	ggMrnaClusterMerge(newCluster, oldCluster);
	if (oldStrand != '?')
	    newCluster->strand[0] = oldStrand;
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

verbose(2, "%d elements before weeding\n", slCount(list));
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
list = binKeeperFind(bins, 0, chromSize); 
verbose(1,"%d elements after weeding\n", slCount(list)); 
slFreeList(&list);
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
return daAliSize(da) > 0.4 * da->ma->baseCount; /* Only .4 as ESTs may be untrimmed (was .8 at one time). */
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
verbose(2, "%d match %s\n", count, query);
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

struct hash *hashMa(struct ggMrnaAli *maList)
/* Return hash of ma's keyed by rna accession. */
{
struct ggMrnaAli *ma;
struct hash *hash = newHash(18);
for (ma = maList; ma != NULL; ma = ma->next)
    hashAdd(hash, ma->qName, ma);
return hash;
}

void listMaInHash(FILE *f, struct maRef *list, struct hash *hash)
/* Output the count of elements in list that are also in hash, and
 * then a comma separated list of such elements. */
{
struct maRef *el;
struct ggMrnaAli *ma;
int count = 0;

for (el = list; el != NULL; el = el->next)
    {
    ma = el->ma;
    if (hashLookup(hash, ma->qName))
        ++count;
    }
fprintf(f, "\t%d\t", count);
for (el = list; el != NULL; el = el->next)
    {
    ma = el->ma;
    if (hashLookup(hash, ma->qName))
	fprintf(f, "%s,", ma->qName);
    }
}

void outputGroup(
	struct binKeeper *bins, char *chromName, int chromSize, 
    	struct ggMrnaAli *refSeqMa, struct ggMrnaAli *mrnaMa, 
	struct ggMrnaAli *estMa, FILE *f)
{
struct binElement *el, *list = binKeeperFindSorted(bins, 0, chromSize);
int clusterIx = 0;
struct hash *refHash = hashMa(refSeqMa);
struct hash *mrnaHash = hashMa(mrnaMa);
struct hash *estHash = hashMa(estMa);

for (el = list; el != NULL; el = el->next)
    {
    struct ggMrnaCluster *cluster = el->val;
    fprintf(f, "%s\t%d\t%d\t%s.%d\t1000\t%s", 
	chromName,
    	cluster->tStart, cluster->tEnd, 
	chromName, ++clusterIx, 
	cluster->strand);

    listMaInHash(f, cluster->refList, refHash);
    listMaInHash(f, cluster->refList, mrnaHash);
    listMaInHash(f, cluster->refList, estHash);
    fprintf(f, "\n");
    }
hashFree(&refHash);
hashFree(&mrnaHash);
hashFree(&estHash);
}

void outputBestAndGoodMgc(struct binElement *list, char *fileName, char *chromName,
			  struct hash *mgcEstHash, struct hash *mgcRnaHash,
			  struct hash *rnaHash, struct hash *refSeqHash)
/* Write out the "best" and "good" EST and mRNA MGC picks for each rnaCluster,
   include if available other mRNA's that are good and refSeq mRNAs. */
{
struct binElement *el = NULL;
FILE *f = mustOpen(fileName, "w");
int clusterIx =0;
verbose(2, "Printing %s\n", fileName);
fprintf(f, "#chrom\tstart    \tend      \tstrand\tMGC EST \t5' dist\tAli%%\tGood EST\t5' dist\tAli%%\tMGC RNA \t5' dist\tAli%%\tGood RNA\t5' dist\tAli%%\tOther RNA \t5' dist\tAli%%\tRefSeq  \t5' dist\tAli%%\tGroup Number\n");
for (el = list; el != NULL; el = el->next)
    {
    struct ggMrnaCluster *cluster = el->val;
    int start = el->start, end = el->end;
    fprintf(f, "%s\t%d\t%d\t%s", chromName, start, end, cluster->strand);
    printClosestInfo(f, start, end, cluster, mgcEstHash, TRUE);
    printClosestInfo(f, start, end, cluster, mgcRnaHash, TRUE);
    printClosestInfo(f, start, end, cluster, rnaHash, FALSE);
    printClosestInfo(f, start, end, cluster, refSeqHash, FALSE);
    fprintf(f, "\t%s.%d", chromName, ++clusterIx);
    fprintf(f, "\n");
    }
carefulClose(&f);
}

struct ggAiSorter *ggAiSorterByHash(struct ggAliInfo *list, struct hash *hash)
/* Construct a ggAiSorter list from a ggAliInfo list. Free with
   slFreeList. */
{
struct ggAiSorter *aiList = NULL, *ai=NULL;
struct ggAliInfo *ggAi = NULL;
for(ggAi = list; ggAi != NULL; ggAi = ggAi->next)
    {
    if(hashLookup(hash, ggAi->ma->qName) && goodDa(ggAi))
	{
	AllocVar(ai);
	ai->ai = ggAi;
	slAddHead(&aiList, ai);
	}
    }
slReverse(&aiList);
return aiList;
}

int fivePrimeDistCmp(const void *va, const void *vb)
/* Compare to sort on 5' distance, Make sure to set
   global variables clusterStrand, clusterStart, clusterEnd before
   using. */
{
int diff = 0;
int aDist = 0, bDist =0;
const struct ggAiSorter *a = *((struct ggAiSorter **)va);
const struct ggAiSorter *b = *((struct ggAiSorter **)vb);
if(clusterStart == -1 || clusterEnd == -1  || clusterStrand == NULL)
    errAbort("clusterRna::fivePrimeDistCmp() - Need to set clusterStart, clusterEnd, clusterStrand before sorting.");
aDist = findStrandDistance(a->ai, *clusterStrand, clusterStart, clusterEnd);
bDist = findStrandDistance(b->ai, *clusterStrand, clusterStart, clusterEnd);
diff = aDist - bDist;
return diff;
}

void writeOutAccession(FILE *f, struct ggAiSorter *ais, struct ggMrnaCluster *cluster,
		       char *chromName, int clusterNumber)
{
fprintf(f, "%s\t%d\t%d\t%s\t%-8s\t%d\t%4.2f%%\t%s.%d\n", 
	cluster->tName, cluster->tStart, cluster->tEnd, cluster->strand, 
	ais->ai->ma->qName, findStrandDistance(ais->ai, *cluster->strand,cluster->tStart, cluster->tEnd), 
	100.0*daAliSize(ais->ai)/ais->ai->ma->baseCount, chromName, clusterNumber);
}

void outputNGoodInHash(struct binElement *list, char *fileName, char *chromName,
		       struct hash *hash, struct hash *pslHash, int maxPicks, int maxDistance)
/* Ouput up to the "maxPicks" selections from each cluster in the "list" as 
   long as they are 1) in the hash, 2) less than "maxDistance" from the
   5' end of the cluster, and 3) "good" as defined by goodDa() */
{
struct binElement *el = NULL;
int clusterIx=0;
FILE *f = mustOpen(fileName, "w");
char *pslFile = addSuffix(fileName, ".psl");
FILE *pslOut = mustOpen(pslFile, "w");
for (el = list; el != NULL; el = el->next)
    {
    struct ggMrnaCluster *cluster = el->val;
    int start = el->start, end = el->end;
    struct ggAiSorter *ais = NULL, *aisList = ggAiSorterByHash(cluster->mrnaList, hash);
    int i;
    clusterIx++;
    clusterStrand = cluster->strand;
    clusterStart = start;
    clusterEnd = end;
    slSort(&aisList, fivePrimeDistCmp);
    for(ais=aisList, i=0; ais != NULL && findStrandDistance(ais->ai,cluster->strand[0], start, end) < maxDistance && i <maxPicks; ais = ais->next, i++)
	{
	struct psl *psl = hashFindVal(pslHash, ais->ai->ma->qName);
	writeOutAccession(f, ais, cluster, chromName, clusterIx);
	if(psl == NULL)
	    fprintf(pslOut, "clusterRna::outputNGoodInHash() - Couldn't find psl in hash with accession %s.\n",ais->ai->ma->qName);
	else
	    pslTabOut(psl, pslOut);
	}
    clusterStrand = NULL;
    clusterStart = -1;
    clusterEnd = -1;
    slFreeList(&aisList);
    }
freez(&pslFile);
carefulClose(&pslOut);
carefulClose(&f);
}

void outputMgc(struct hash *pslHash, struct sqlConnection *conn, struct binKeeper *bins, 
	       char *chromName, int chromSize, char *fileName)
/* Output info about clusters relevant to MGC */
{
struct hash *refSeqHash = hashRefSeq(conn);
struct hash *mgcEstHash = newHash(20);
struct hash *mgcRnaHash = newHash(0);
struct hash *rnaHash = newHash(0);
struct hash *estAuthorHash = wildHash(conn, "author", "NIH-MGC%");
struct hash *rnaAuthorHash = wildHash(conn, "author", "Strausberg,R.");
struct sqlResult *sr = NULL;
char **row=NULL;
char *mgcOutFile = NULL;
int maxPicks = optionInt("mgcNumPicks", 10);     /* How many picks to make. */
int maxDistance = optionInt("mgcMaxDist", 100);  /* Max distance allowed from 5' end to allow. */
struct binElement *list = binKeeperFindSorted(bins, 0, chromSize);

/* Read mRNA table and split into hashes. */
verbose(2, "Scanning mrna table\n");
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
outputBestAndGoodMgc(list, fileName, chromName, mgcEstHash,
		     mgcRnaHash, rnaHash, refSeqHash);
mgcOutFile = addSuffix(fileName, ".ests");
outputNGoodInHash(list, mgcOutFile, chromName, mgcEstHash, pslHash, maxPicks, maxDistance);
freez(&mgcOutFile);
mgcOutFile = addSuffix(fileName, ".rnas");
outputNGoodInHash(list, mgcOutFile, chromName, mgcRnaHash, pslHash, maxPicks, maxDistance);
freez(&mgcOutFile);
hashFree(&mgcEstHash);
hashFree(&mgcRnaHash);
slFreeList(&list);
}

void fillInRnaExcludeHash(struct hash *excludeHash, char *excludeFile)
/* Put entries for the accessions in the exclude file into the exclude hash.
   Basically if there is an entry in the hash, it shouldn't be used. */
{
struct lineFile *lf = lineFileOpen(excludeFile, TRUE);
char *row[1];
char *line = NULL;
while(lineFileRow(lf, row))
    {
    line = cloneString(row[0]);    
    hashAddUnique(excludeHash, line, line);
    }
lineFileClose(&lf);
}

struct psl *filterByExcludeHash(struct psl *pslList,  struct hash *excludeHash)
/* Iterate through the pslList and throw away those that have entries
   in the excludeHash */
{
struct psl *psl=NULL, *pslNext=NULL, *pslRet=NULL;
char *entry = NULL;
for(psl = pslList; psl != NULL; psl=pslNext)
    {
    pslNext = psl->next;
    entry = hashFindVal(excludeHash, psl->qName);
    if(entry == NULL)
	slSafeAddHead(&pslRet, psl);
    else
	pslFree(&psl);
    }
slReverse(&pslRet);
return pslRet;
}
    
void hashAddSomePsls(struct hash *pslHash, struct psl *pslList)
/* Add psls into hash using qName as key. */
{
struct psl *psl = NULL;
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    hashAdd(pslHash, psl->qName, psl);
    }
}

void clusterChromRna(struct sqlConnection *conn, char *chromName,
	FILE *rnaOut, FILE *estOut, FILE *groupOut)
/* Cluster RNA on one chromosome. */
{
struct ggMrnaAli *refSeqMaList = NULL, *mrnaMaList = NULL, 
	*splicedEstMaList = NULL, *tailedEstMaList = NULL, 
	*otherEstMaList = NULL, *splicedRnaMaList=NULL;
struct dnaSeq *chrom = NULL;
struct hash *splicedHash = newHash(20), *tailedHash = newHash(20),
            *otherHash = newHash(20);
struct hash *splicedRnaHash = newHash(20), *tailedRnaHash = newHash(20),
            *otherRnaHash = newHash(20), *excludeRnaHash = newHash(20);
struct hash *pslHash = newHash(20);
struct estOrientInfo *eoiList = NULL;
struct estOrientInfo *mRnaOiList = NULL;
struct psl *estPslList = NULL, *mrnaPslList = NULL;
struct binKeeper *bins = NULL;
char *mgcOut = optionVal("MGC", NULL);
char *rnaTable = optionVal("rna", "mrna");
char *estTable = optionVal("est", "est");
boolean doEsts = !optionExists("noEst");
boolean doRefSeq = !optionExists("noRefSeq");
char *orientTable = optionVal("orient", "estOrientInfo");
char *mRnaOrientTable = optionVal("mRNAOrient", "mrnaOrientInfo");
char *mRnaExclude = optionVal("mrnaExclude", NULL);

chrom = hLoadChrom(sqlGetDatabase(conn), chromName);
verbose(1, "Loaded %d bases in %s\n", chrom->size, chromName);

/* If we got an exclusion list of accessions to avoid, filter them out. */
if(mRnaExclude != NULL)
    fillInRnaExcludeHash(excludeRnaHash, mRnaExclude);
    
/* Load mRNA info and use it to categorize mRNAs, mainly with Intron or not. */
mRnaOiList = loadEstOrientInfo(conn, chromName, mRnaOrientTable);
verbose(2, "Loaded %d mRnaOi's\n", slCount(mRnaOiList));
categorizeEsts(mRnaOiList, splicedRnaHash, tailedRnaHash, otherRnaHash);

/* Load mRNA psls, then do any necessary filtering. */
mrnaPslList = loadPsls(conn, rnaTable, chromName);
mrnaPslList = filterByExcludeHash(mrnaPslList, excludeRnaHash);

/* Create clusters from mRNAs that are spliced and clean up. */
splicedRnaMaList = maFromSomePsls(mrnaPslList, chromName,
				  chrom, splicedRnaHash, FALSE, FALSE); 
hashAddSomePsls(pslHash, mrnaPslList);

/* Load RefGenes. */
if (doRefSeq)
    refSeqMaList = maFromPslTable(conn, "refSeqAli", chromName, 
	    chrom, TRUE, TRUE);

if (doEsts)
    {
    /* Load EST info and use it to categorize ESTs. */
    eoiList = loadEstOrientInfo(conn, chromName, orientTable);
    verbose(2, "Loaded %d eoi's\n", slCount(eoiList));
    categorizeEsts(eoiList, splicedHash, tailedHash, otherHash);

    /* Load ESTs into three separate lists. */
    estPslList = loadPsls(conn, estTable, chromName);
    estPslList = filterByExcludeHash(estPslList, excludeRnaHash);
    hashAddSomePsls(pslHash, estPslList);
    freeHashAndVals(&excludeRnaHash);

    splicedEstMaList = maFromSomePsls(estPslList, chromName, 
	    chrom, splicedHash, FALSE, FALSE);
    tailedEstMaList = maFromSomePsls(estPslList, chromName, 
	    chrom, tailedHash, FALSE, FALSE);
    otherEstMaList = maFromSomePsls(estPslList, chromName, 
	    chrom, otherHash, FALSE, FALSE);

    estOrientInfoFreeList(&eoiList);
    }
/* Create bin to put clusters in, and cluster
 * refSeq and mRNAs. */
bins = binKeeperNew(0, chrom->size);
addCluster(refSeqMaList, bins, NULL, chrom);
verbose(2, "Added refSeqMrna to bins ok.\n");
addCluster(splicedRnaMaList, bins, NULL, chrom);
verbose(2, "Added tightMrna to bins ok.\n");
weedWeakClusters(bins, chrom->size);
outputClusters(bins, chromName, chrom->size, rnaOut);

/* Cluster ESTs. */
if (doEsts)
    {
    addCluster(splicedEstMaList, bins, splicedHash, chrom);
    // addCluster(tailedEstMaList, bins, tailedHash, chrom);
    // addCluster(otherEstMaList, bins, otherHash, chrom);
    // orientEstMa(tailedEstMaList, tailedHash);
    weedWeakClusters(bins, chrom->size);
    outputClusters(bins, chromName, chrom->size, estOut);
    }


/* TODO: Merge clusters that share 3'/5' ESTs. */

if (mgcOut != NULL)
    outputMgc(pslHash, conn, bins, chromName, chrom->size, mgcOut);
if (groupOut != NULL)
    outputGroup(bins, chromName, chrom->size, 
    	refSeqMaList, splicedRnaMaList, splicedEstMaList, groupOut);

/* Cleanup. */
freeDnaSeq(&chrom);
hashFree(&splicedHash);
hashFree(&tailedHash);
hashFree(&otherHash);
hashFree(&splicedRnaHash);
hashFree(&tailedRnaHash);
hashFree(&otherRnaHash);
hashFree(&excludeRnaHash);
estOrientInfoFreeList(&eoiList);
estOrientInfoFreeList(&mRnaOiList);
ggMrnaAliFreeList(&refSeqMaList);
ggMrnaAliFreeList(&mrnaMaList);
ggMrnaAliFreeList(&splicedEstMaList);
ggMrnaAliFreeList(&tailedEstMaList);
ggMrnaAliFreeList(&otherEstMaList);
ggMrnaAliFreeList(&splicedRnaMaList);
pslFreeList(&estPslList);
pslFreeList(&mrnaPslList);
}

void clusterRna(char *database, char *rnaOut, char *estOut)
/* clusterRna - Make clusters of mRNA and ESTs. */
{
struct sqlConnection *conn;
struct slName *chromList = NULL, *chrom;
FILE *rnaOutFile = mustOpen(rnaOut, "w");
FILE *estOutFile = mustOpen(estOut, "w");
FILE *groupOutFile = NULL;
char *group = optionVal("group", NULL);

if (optionExists("chrom"))
    chromList = slNameNew(optionVal("chrom", NULL));
else
    {
    chromList = hAllChromNames(database);
    if (optionExists("MGC"))
        errAbort("Currently MGC can't be used except with chrom option");
    }
conn = hAllocConn(database);
if (group != NULL)
    groupOutFile = mustOpen(group, "w");
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    clusterChromRna(conn, chrom->name, rnaOutFile, estOutFile, groupOutFile);
hFreeConn(&conn);
carefulClose(&groupOutFile);
carefulClose(&rnaOutFile);
carefulClose(&estOutFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clusterRna(argv[1], argv[2], argv[3]);
return 0;
}
