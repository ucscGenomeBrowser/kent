/* clusterGenes - Cluster genes from genePred tracks. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "verbose.h"
#include "dlist.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"
#include "binRange.h"
#include "bits.h"
#include "hdb.h"


/* Notes:
 *  strand is passed as '*' when -ignoreStrand is specified.
 */

/* Command line driven variables. */
char *clChrom = NULL;

#define ignoredStrand '*'

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterGenes - Cluster genes from genePred tracks\n"
  "usage:\n"
  "   clusterGenes [options] outputFile database table1 ... tableN\n"
  "   clusterGenes [options] -trackNames outputFile database track1 table1 ... trackN tableN\n"
  "\n"
  "Where outputFile is a tab-separated file describing the clustering,\n"
  "database is a genome database such as mm4 or hg16,\n"
  "and the table parameters are either tables in genePred format in that\n"
  "database or genePred tab seperated files. If the input is all from files, the argument\n"
  "can be `no'.\n"
  "options:\n"
  "   -verbose=N - Print copious debugging info. 0 for none, 3 for loads\n"
  "   -chrom=chrN - Just work this chromosome, maybe repeated.\n"
  "   -cds - cluster only on CDS exons, Non-coding genes are dropped.\n"
  "   -trackNames - If specified, input are pairs of track names and files.\n"
  "    This is useful when the file names don't reflact the desired track\n"
  "    names.\n"
  "   -ignoreStrand - cluster postive and negative strand together\n"
  "   -clusterBed=bed - output BED file for each cluster.  If -cds is specified,\n"
  "    this only contains bounds based on CDS\n"
  "   -clusterTxBed=bed - output BED file for each cluster.  If -cds is specified,\n"
  "    this contains bounds based on full transcript range of genes, not just CDS\n"
  "   -flatBed=bed - output BED file that contains the exons of all genes\n"
  "    flattned into a single record. If -cds is specified, this only contains\n"
  "    bounds based on CDS\n"
  "   -joinContained - join genes that are contained within a larger loci\n"
  "    into that loci. Intended as a way to handled fragments and exon-level\n"
  "    predictsions, as genes-in-introns on the same strand are very rare.\n"
  "   -conflicted - detect conflicted loci. Conflicted loci are loci that\n"
  "    contain genes that share no sequence.  This option greatly increases\n"
  "    size of output file.\n"
  "   -ignoreBases=N - ignore this many based to the start and end of each\n"
  "    transcript when determine overlap.  This prevents small amounts of overlap\n"
  "    from merging transcripts.  If -cds is specified, this amount of the CDS.\n"
  "    is ignored. The default is 0.\n"
  "\n"
  "The cdsConflicts and exonConflicts columns contains `y' if the cluster has\n"
  "conficts. A conflict is a cluster where all of the genes don't share exons. \n"
  "Conflicts maybe either internal to a table or between tables.\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING|OPTION_MULTI},
   {"cds", OPTION_BOOLEAN},
   {"trackNames", OPTION_BOOLEAN},
   {"clusterBed", OPTION_STRING},
   {"clusterTxBed", OPTION_STRING},
   {"flatBed", OPTION_STRING},
   {"cds", OPTION_BOOLEAN},
   {"joinContained", OPTION_BOOLEAN},
   {"conflicted", OPTION_BOOLEAN},
   {"ignoreStrand", OPTION_BOOLEAN},
   {"ignoreBases", OPTION_INT},
   {NULL, 0},
};

/* from command line  */
static boolean gUseCds;
static boolean gTrackNames;
static boolean gJoinContained = FALSE;
static boolean gDetectConflicted = FALSE;
static boolean gIgnoreStrand = FALSE;
static int gIgnoreBases = 0;

struct track
/*  Object representing a track. */
{
    struct track *next;
    char *name;             /* name to use */
    char *table;            /* table or file */
    boolean isDb;           /* is this a database table or file? */
    struct genePred *genes; /* genes read from a file, sorted by chrom and strand */
};

static int genePredStandCmp(const void *va, const void *vb)
/* Compare to sort based on chromosome, strand, txStart, txEnd, and name.  The
 * location and name are just included to help make tests consistent. */
{
const struct genePred *a = *((struct genePred **)va);
const struct genePred *b = *((struct genePred **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = strcmp(a->strand, b->strand);
if (dif == 0)
    dif = a->txStart - b->txStart;
if (dif == 0)
    dif = a->txEnd - b->txEnd;
if (dif == 0)
    dif = strcmp(a->name, b->name);
return dif;
}

static char *trackNameCreate(char* name,
                             char *table)
/* get name to use for a table */
{
if (name != NULL)
    return cloneString(name);
else
    {
    /* will strip directories and trailing extensions, if any */
    char trackName[256];
    splitPath(table, NULL, trackName, NULL);
    if (endsWith(table, ".gz") || endsWith(table, ".bz2") || endsWith(table, ".Z"))
        {
        char *ext2 = strrchr(trackName, '.');
        if (ext2 != NULL)
            *ext2 = '\0';
        }
    return cloneString(trackName);
    }
}

struct track* trackFileNew(char* name,
                           char *table)
/* construct a track from a file */
{
struct track* track;
AllocVar(track);
track->name = trackNameCreate(name, table);
track->isDb = FALSE;
track->genes = genePredReaderLoadFile(table, NULL);
slSort(&track->genes, genePredStandCmp);
return track;
}

struct genePred *trackFileGetGenes(struct track *track,
                                   char *chrom, char strand)
/* get genes from a file track for chrom and strand. Must be called in
 * assending chrom and then +,- order.  By passed chroms are deleted from
 * list. */
{
struct genePred *genes = NULL;

/* bypass and delete genes before this chrom/strand */
while ((track->genes != NULL)
       && (strcmp(track->genes->chrom, chrom) < 0)
       && ((track->genes->strand[0] < strand) || (strand == ignoredStrand)))
    {
    struct genePred *gp = slPopHead(&track->genes);
    genePredFree(&gp);
    }

/* add genes on same chrom/strand */
while ((track->genes != NULL)
       && sameString(track->genes->chrom, chrom)
       && ((track->genes->strand[0] == strand) || (strand == ignoredStrand)))
    {
    slSafeAddHead(&genes, slPopHead(&track->genes));
    }
slReverse(&genes);
return genes;
}

struct track* trackTableNew(char* name,
                            char *table)
/* construct a track from a file */
{
struct track* track;
AllocVar(track);
track->name = trackNameCreate(name, table);
track->table = cloneString(table);
track->isDb = TRUE;
return track;
}

struct genePred *trackTableGetGenes(struct track *track,
                                    struct sqlConnection *conn,
                                    char *chrom, char strand)
/* get genes from a table track for chrom and strand. */
{
char where[128];
if (strand == ignoredStrand)
    sqlSafef(where, sizeof(where), "chrom = '%s'", chrom);
else
    sqlSafef(where, sizeof(where), "chrom = '%s' and strand = '%c'", chrom, strand);
return genePredReaderLoadQuery(conn, track->table,  where);
}

struct track* trackNew(struct sqlConnection *conn,
                       char* name,
                       char *table)
/* create a new track, if name is NULL,  name is derived from table. */
{
/* determine if table or file, if name contains */
if (fileExists(table) || strchr(table, '.') || (conn == NULL))
    return trackFileNew(name, table);
else if (sqlTableExists(conn, table))
    return trackTableNew(name, table);
else
    errAbort("table %s.%s or file %s doesn't exist", sqlGetDatabase(conn), table, table);
return NULL;
}


struct track *buildTracks(struct sqlConnection *conn, int specCount, char *specs[])
/* build list of tracks, consisting of list of tables, files, or
 * pairs of trackNames and files */
{
struct track* tracks = NULL;
int i;
if (gTrackNames)
    {
    for (i = 0; i < specCount; i += 2)
        slSafeAddHead(&tracks, trackNew(conn, specs[i], specs[i+1]));
    }
else
    {
    for (i = 0; i < specCount; i++)
        slSafeAddHead(&tracks, trackNew(conn, NULL, specs[i]));
    }
slReverse(&tracks);
return tracks;
}

static void addTrackChroms(struct track *track, struct hash *chromSet)
/* add chroms for a track */
{
char *prevChrom = NULL;
struct genePred *gp;
for (gp = track->genes; gp != NULL; gp = gp->next)
    {
    if ((prevChrom == NULL) || !sameString(gp->chrom, prevChrom))
        {
        hashStoreName(chromSet, gp->chrom);
        prevChrom = gp->chrom;
        }
    }
}

struct slName *getFileTracksChroms(struct track* tracks)
/* get list of chromosome for all file tracks */
{
struct track* tr;
struct slName *chroms = NULL;
struct hash *chromSet = hashNew(20); /* in case of scaffolds */
struct hashCookie cookie;
struct hashEl* hel;

for (tr = tracks; tr != NULL; tr = tr->next)
    {
    if (!tr->isDb)
        addTrackChroms(tr, chromSet);
    }

cookie = hashFirst(chromSet);
while ((hel = hashNext(&cookie)) != NULL)
    slSafeAddHead(&chroms, slNameNew(hel->name));
hashFree(&chromSet);
return chroms;
}

struct slName *getChroms(struct sqlConnection *conn, struct track* tracks)
/* build list of possible chromosomes for all tracks, including command line
 * restrictions */
{
struct track* tr;
boolean anyDb = FALSE;
struct slName *chroms = NULL;

/* check for cmd line restriction first */
if (optionExists("chrom"))
    chroms = slNameCloneList(optionMultiVal("chrom", NULL));
else
    {
    /* if there are any tracks from the databases, include all chroms? */
    for (tr = tracks; tr != NULL; tr = tr->next)
        if (tr->isDb)
        anyDb = TRUE;
    if (anyDb)
        chroms = slNameCloneList(hAllChromNames(sqlGetDatabase(conn)));
    else
        chroms = getFileTracksChroms(tracks);
    }
slNameSort(&chroms);
return chroms;
}

struct clusterGene
/* A gene in a cluster */
{
    struct clusterGene* next;
    struct track* track;         /* aka table for gene */
    struct genePred* gp;
    struct cluster* cluster;  /* cluster containing gene */
    int effectiveStart;     /* start/end based on -cds and -ignoreBases */
    int effectiveEnd;
    struct slRef *exonConflicts; /* list of genes in cluster that don't share
                                  * exons with this gene */
    struct slRef *cdsConflicts; /* list of genes in cluster that don't share
                                 * CDS with this gene */
};

static int findStartExon(struct genePred* gp,
                         int minStart)
/* find start exon based on using cds or not */
{
int exonIx;
for (exonIx = 0; exonIx < gp->exonCount; exonIx++)
    {
    if (minStart >= gp->exonStarts[exonIx])
        return exonIx;
    }
errAbort("BUG: findStartExon failed: %s", gp->name);
return 0;
}

static int calcEffectiveStart(struct genePred* gp)
/* calculate the genomic start position to use for overlaps */
{
int minStart = gUseCds ? gp->cdsStart : gp->txStart;
int exonIx = findStartExon(gp, minStart);
for (; exonIx < gp->exonCount; exonIx++)
    {
    int possibleStart = minStart + gIgnoreBases;
    if (possibleStart >= gp->exonStarts[exonIx])
        return possibleStart;
    }
errAbort("BUG: calcEffectiveStart failed: %s", gp->name);
return 0;
}

static int findEndExon(struct genePred* gp,
                       int maxEnd)
/* find end exon based on using cds or not */
{
int exonIx;
for (exonIx = gp->exonCount-1; exonIx >= 0; exonIx--)
    {
    if (maxEnd <= gp->exonEnds[exonIx])
        return exonIx;
    }
errAbort("BUG: findEndExon failed: %s", gp->name);
return 0;
}

static int calcEffectiveEnd(struct genePred* gp)
/* calculate the genomic end position to use for overlaps */
{
int maxEnd = gUseCds ? gp->cdsEnd : gp->txEnd;
int exonIx = findEndExon(gp, maxEnd);
for (; exonIx >= 0; exonIx--)
    {
    int possibleEnd = maxEnd - gIgnoreBases;
    if (possibleEnd <= gp->exonEnds[exonIx])
        return possibleEnd;
    }
errAbort("BUG: calcEffectiveEnd failed: %s", gp->name);
return 0;
}


struct clusterGene* clusterGeneNew(struct track* track, struct genePred* gp)
/* create a new clusterGene. */
{
struct clusterGene* cg;
AllocVar(cg);
cg->track = track;
cg->gp = gp;
cg->effectiveStart = calcEffectiveStart(gp);
cg->effectiveEnd = calcEffectiveEnd(gp);
return cg;
}

void clusterGeneFree(struct clusterGene** cgp)
/* Free a clusterGene. */
{
struct clusterGene* cg = *cgp;
if (cg != NULL)
    {
    slFreeList(&cg->exonConflicts);
    slFreeList(&cg->cdsConflicts);
    freez(cgp);
    }
}

int clusterGeneRefCmp(const void *clr1, const void *clr2)
/* compare two slRef objects reference clusterGene objects */
{
struct clusterGene *cg1 = (*((struct slRef**)clr1))->val;
struct clusterGene *cg2 = (*((struct slRef**)clr2))->val;
int cmp = strcmp(cg1->track->name, cg2->track->name);
if (cmp == 0)
    cmp = strcmp(cg1->gp->name, cg2->gp->name);
return cmp;
}

struct cluster
/* A cluster of overlapping genes. */
    {
    struct cluster *next;	/* Next in list. */
    int id;                     /* id assigned to cluster */
    struct clusterGene *genes;  /* Associated genes. */
    char *chrom;                /* chrom, memory not owned */
    int start,end;		/* Range covered by cluster. */
    int txStart,txEnd;          /* txStart/txEnd range, diff from start/end if effective bounds differ */
    boolean hasExonConflicts;   /* does this cluster have conflicts? */
    boolean hasCdsConflicts;
    };

boolean clusterHaveTrack(struct cluster *cluster,
                         struct track *track)
/* check if the cluster has a track */
{
struct clusterGene *gene;
for (gene = cluster->genes; gene != NULL; gene = gene->next)
    if (gene->track == track)
        return TRUE;
return FALSE;
}

void clusterDump(struct cluster *cluster)
/* Dump contents of cluster to stderr. */
{
struct clusterGene *gene;
fprintf(stderr, "%d-%d", cluster->start, cluster->end);
for (gene = cluster->genes; gene != NULL; gene = gene->next)
    fprintf(stderr, " %s", gene->gp->name);
fprintf(stderr, "\n");
}

struct cluster *clusterNew()
/* Create new cluster. */
{
struct cluster *cluster;
AllocVar(cluster);
return cluster;
}

void clusterFree(struct cluster **pCluster)
/* Free up a cluster. */
{
struct cluster *cluster = *pCluster;
if (cluster != NULL)
    {
    while (cluster->genes != NULL)
        {
        struct clusterGene *cg = cluster->genes;
        cluster->genes = cluster->genes->next;
        clusterGeneFree(&cg);
        }
    freez(pCluster);
    }
}

void clusterFreeList(struct cluster **pList)
/* Free a list of dynamically allocated cluster's */
{
struct cluster *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    clusterFree(&el);
    }
*pList = NULL;
}

int clusterCmp(const void *va, const void *vb)
/* Compare to sort based on start and end. assumes they are on the chrom */
{
const struct cluster *a = *((struct cluster **)va);
const struct cluster *b = *((struct cluster **)vb);
int dif = a->start - b->start;
if (dif == 0)
    dif = a->end - b->end;
return dif;
}

static boolean gpGetExon(struct clusterGene *cg, int exonIx, boolean cdsOnly, 
                         int *exonStartRet, int *exonEndRet)
/* Get the start and end of an exon, adjusting if we are only examining CDS,
 * Return false if exon should not be used.  */
{
int exonStart = cg->gp->exonStarts[exonIx];
int exonEnd = cg->gp->exonEnds[exonIx];
if (exonStart < cg->effectiveStart)
    exonStart = cg->effectiveStart;
if (exonEnd > cg->effectiveEnd)
    exonEnd = cg->effectiveEnd;
// still need to check CDS for CDS conflict check
if (cdsOnly)
    {
    if (exonStart < cg->gp->cdsStart)
        exonStart = cg->gp->cdsStart;
    if (exonEnd > cg->gp->cdsEnd)
        exonEnd = cg->gp->cdsEnd;
    }
*exonStartRet = exonStart;
*exonEndRet = exonEnd;
return exonStart < exonEnd;
}

void clusterAddExon(struct cluster *cluster,
	int start, int end, struct clusterGene *cg)
/* Add exon to cluster. */
{
assert((cg->cluster == NULL) || (cg->cluster == cluster));
if (cg->cluster == NULL)
    {
    cg->cluster = cluster;
    slSafeAddHead(&cluster->genes, cg);
    }
if (cluster->start == cluster->end)
    {
    cluster->chrom = cg->gp->chrom;
    cluster->start = start;
    cluster->end = end;
    cluster->txStart = cg->gp->txStart;
    cluster->txEnd = cg->gp->txEnd;
    }
else
    {
    cluster->start = min(cluster->start, start);
    cluster->end = max(cluster->end, end);
    cluster->txStart = min(cluster->txStart, cg->gp->txStart);
    cluster->txEnd = max(cluster->txEnd, cg->gp->txEnd);
    }
}

void addExon(struct binKeeper *bk, struct dlNode *clusterNode,
             int start, int end, struct clusterGene *cg)
/* Add exon to cluster and binKeeper. */
{
clusterAddExon(clusterNode->val, start, end, cg);
binKeeperAdd(bk, start, end, clusterNode);
}

void mergeClusters(struct binKeeper *bk, struct binElement *bkRest,
	struct dlNode *aNode, struct dlNode *bNode)
/* Move bNode into aNode. */
{
struct cluster *aCluster = aNode->val;
struct cluster *bCluster = bNode->val;
struct binElement *bkEl;

if (verboseLevel() >= 3) 
    {
    fprintf(stderr, " a: ");
    clusterDump(aCluster);
    fprintf(stderr, " b: ");
    clusterDump(bCluster);
    }

/* First change references to bNode. */
binKeeperReplaceVal(bk, bCluster->start, bCluster->end, bNode, aNode);
for (bkEl = bkRest; bkEl != NULL; bkEl = bkEl->next)
    if (bkEl->val == bNode) 
        bkEl->val = aNode;

/* Add b's genes to a. */
struct clusterGene *cg = bCluster->genes;
while ((cg = slPopHead(&bCluster->genes)) != NULL)
    {
    cg->cluster = aCluster;
    slSafeAddHead(&aCluster->genes, cg);
    }

/* Adjust start/end. */
if (bCluster->start < aCluster->start) 
    aCluster->start = bCluster->start;
if (aCluster->end < bCluster->end)
    aCluster->end = bCluster->end;

/* Remove all traces of bNode. */
dlDelete(&bNode);
clusterFree(&bCluster);
if (verboseLevel() >= 3) 
    {
    fprintf(stderr, " ab: ");
    clusterDump(aCluster);
    }
}

boolean shareExons(struct clusterGene *cg1, struct clusterGene *cg2, boolean cdsOnly)
/* determine if two genes share exons or CDS exons */
{
int exonIx1, exonStart1, exonEnd1;
int exonIx2, exonStart2, exonEnd2;

for (exonIx1 = 0; exonIx1 < cg1->gp->exonCount; exonIx1++)
    {
    if (gpGetExon(cg1, exonIx1, cdsOnly, &exonStart1, &exonEnd1))
        {
        /* exonStart2 >= exon1End indicates there can't be overlap on this
         * exon */
        for (exonIx2 = 0, exonStart2 = 0;
             (exonIx2 < cg2->gp->exonCount) && (exonStart2 < exonEnd1);
             exonIx2++)
            {
            if (gpGetExon(cg2, exonIx2, cdsOnly, &exonStart2, &exonEnd2))
                {
                if ((exonStart2 < exonEnd1) && (exonEnd2 > exonStart1))
                    return TRUE; /* overlaps */
                }
            }
        }
    }
return FALSE;
}

struct slRef *getGeneConflicts(struct cluster *cluster, struct clusterGene *gene,
                               boolean cdsOnly)
/* get list of genes in this cluster that don't share exons/CDS */
{
struct slRef *conflicts = NULL;
struct clusterGene *cg;

/* check all other genes */
for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    if ((cg != gene) && !shareExons(cg, gene, cdsOnly))
        refAdd(&conflicts, cg);
    }
return conflicts;
}

void getClusterConflicts(struct cluster *cluster)
/* determine if the cluster has conflicts and fill in clusterGene
 * list of conflicts */
{
struct clusterGene *cg;

for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    cg->exonConflicts = getGeneConflicts(cluster, cg, FALSE);
    if (cg->exonConflicts != NULL)
        {
        slSort(&cg->exonConflicts, clusterGeneRefCmp);
        cluster->hasExonConflicts = TRUE;
        }
    cg->cdsConflicts = getGeneConflicts(cluster, cg, TRUE);
    if (cg->cdsConflicts != NULL)
        {
        slSort(&cg->cdsConflicts, clusterGeneRefCmp);
        cluster->hasCdsConflicts = TRUE;
        }
    }
}

void getConflicts(struct cluster *clusters)
/* search for conflicst in clusters and fill in the data structs */
{
struct cluster *cl;
for (cl = clusters; cl != NULL; cl = cl->next)
    getClusterConflicts(cl);
}

int totalGeneCount = 0;
int totalClusterCount = 0;

struct clusterMaker
/* Something that helps us make clusters. */
    {
    struct clusterMaker *next;	/* Next in list */
    struct dlList *clusters;	/* Doubly linked list of clusters. */
    struct cluster *orphans;    /* not added to clusters due to gIgnoreBases */
    struct binKeeper *bk;	/* Bin-keeper that tracks exons. */
    };

struct clusterMaker *clusterMakerStart(int chromSize)
/* Allocate a new clusterMaker */
{
struct clusterMaker *cm;
AllocVar(cm);
cm->bk = binKeeperNew(0, chromSize);
cm->clusters = dlListNew();
return cm;
}

int nextClusterId = 1;  /* next cluster id to assign */

struct cluster *clusterMakerFinish(struct clusterMaker **pCm)
/* Finish up cluster - free up and return singly-linked list of clusters. */
{
struct dlNode *node;
struct cluster *clusterList = NULL, *cluster;
struct clusterMaker *cm = *pCm;

if (cm == NULL)
    errAbort("Null cluster in clusterMakerFinish");

/* We build up the cluster list as a doubly-linked list
 * to make it faster to remove clusters that get merged
 * into another cluster.  At the end though we make
 * a singly-linked list out of it. */
for (node = cm->clusters->tail; !dlStart(node); node=node->prev)
    {
    cluster = node->val;
    slAddHead(&clusterList, cluster);
    }
clusterList = slCat(clusterList, cm->orphans);
slSort(&clusterList, clusterCmp);

if (gDetectConflicted)
    getConflicts(clusterList);

/* assign ids */
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    cluster->id = nextClusterId++;

/* Clean up and go home. */
binKeeperFree(&cm->bk);
dlListFree(&cm->clusters);
freez(pCm);
return clusterList;
}

static struct dlNode *clusterMakerAddExonNoOverlap(struct clusterMaker *cm, struct clusterGene *cg,
                                                   int exonStart, int exonEnd, struct dlNode *currentNode)
/* add an exon to a cluster when it doesn't overlap any existing exons, return node */
{
if (currentNode == NULL)
    {
    struct cluster *cluster = clusterNew();
    currentNode = dlAddValTail(cm->clusters, cluster);
    }
addExon(cm->bk, currentNode, exonStart, exonEnd, cg);
return currentNode;
}

static struct dlNode *clusterMakerAddExonOverlap(struct clusterMaker *cm, struct clusterGene *cg,
                                                 int exonStart, int exonEnd, struct binElement *bList,
                                                 struct dlNode *currentNode)
/* add an exon that overlap exons in clusters, return node */
{
struct binElement *bEl;
for (bEl = bList; bEl != NULL; bEl = bEl->next)
    {
    struct dlNode *newNode = bEl->val;
    if (newNode != currentNode)
        {
        if (currentNode == NULL)
            {
            /* Add to existing cluster. */
            currentNode = newNode;
            }
        else
            {
            /* Merge new cluster into old one. */
            verbose(3, "Merging %p %p\n", currentNode, newNode);
            mergeClusters(cm->bk, bEl->next, currentNode, newNode);
            }
        }
    }
addExon(cm->bk, currentNode, exonStart, exonEnd, cg);
slFreeList(&bList);
return currentNode;
}

static void clusterMakerAddExon(struct clusterMaker *cm, struct clusterGene *cg,
                                int exonStart, int exonEnd, struct dlNode **currentNodePtr)
/* Add a gene exon to clusterMaker. */
{
verbose(4, "  exon %d-%d\n", exonStart, exonEnd);
struct binElement *bList = binKeeperFind(cm->bk, exonStart, exonEnd);
if (bList == NULL)
    *currentNodePtr = clusterMakerAddExonNoOverlap(cm, cg, exonStart, exonEnd, *currentNodePtr);
else
    *currentNodePtr = clusterMakerAddExonOverlap(cm, cg, exonStart, exonEnd, bList, *currentNodePtr);
}

bool clusterMakerAdd(struct clusterMaker *cm, struct clusterGene *cg)
/* Add gene to clusterMaker, return false if not added due to gIgnoreBases */
{
int exonIx;
struct dlNode *currentNode = NULL;

/* Build up cluster list with aid of binKeeper.  For each exon look to see if
 * it overlaps an existing cluster.  If so put it into existing cluster,
 * otherwise make a new cluster.  The hard case is where part of the gene is
 * already in one cluster and the exon overlaps with a new cluster.  In this
 * case merge the new cluster into the old one.  If we are joining contained
 * genes, the only gene range is added as if it was a single exon. */

verbose(2, "add gene: [%s] %s %d-%d\n", cg->track->name, cg->gp->name, cg->gp->txStart, cg->gp->txEnd);
bool added = FALSE;
if (gJoinContained)
    {
    if (cg->effectiveStart < cg->effectiveEnd)
        {
        clusterMakerAddExon(cm, cg, cg->effectiveStart, cg->effectiveEnd, &currentNode);
        added = TRUE;
        }
    }
else
    {
    for (exonIx = 0; exonIx < cg->gp->exonCount; ++exonIx)
        {
        int exonStart, exonEnd;
        if (gpGetExon(cg, exonIx, FALSE, &exonStart, &exonEnd))
            {
            clusterMakerAddExon(cm, cg, exonStart, exonEnd, &currentNode);
            added = TRUE;
            }
        }
    }
return added;
}

void addOrphan(struct clusterMaker *cm, struct clusterGene* cg)
/* create an orphan cluster and added it to the orphans */
{
struct cluster *cluster = clusterNew();
cg->cluster = cluster;
slSafeAddHead(&cluster->genes, cg);
cluster->chrom = cg->gp->chrom;
cluster->start = cg->gp->txStart;
cluster->end = cg->gp->txEnd;
cluster->txStart = cg->gp->txStart;
cluster->txEnd = cg->gp->txEnd;
slSafeAddHead(&cm->orphans, cluster);
}

void loadGenes(struct clusterMaker *cm, struct sqlConnection *conn,
               struct track* track, char *chrom, char strand)
/* load genes into cluster from a table or file */
{
struct genePred *genes, *gp;
verbose(2, "loadGenes: [%s] %s %c\n", track->name, chrom, strand);

if (track->isDb)
    genes = trackTableGetGenes(track, conn, chrom, strand);
else
    genes = trackFileGetGenes(track, chrom, strand);

/* add to cluster and deletion list */
while ((gp = slPopHead(&genes)) != NULL)
    {
    if ((!gUseCds) || (gp->cdsStart < gp->cdsEnd))
        {
        struct clusterGene* cg = clusterGeneNew(track, gp);
        if (!clusterMakerAdd(cm, cg))
            addOrphan(cm, cg);
        ++totalGeneCount;
        }
    else
        genePredFree(&gp);
    }
}

void prConflicts(FILE *f, struct slRef* conflicts)
/* print list of conflicts as comma-seperated list */
{
struct slRef* cl;
fprintf(f, "\t");
for (cl = conflicts; cl != NULL; cl = cl->next)
    {
    struct clusterGene *cg = cl->val;
    fprintf(f, "%s:%s,", cg->track->name, cg->gp->name);
    }
}

void prGene(FILE *f, struct cluster *cluster, struct clusterGene *cg)
/* output info on one gene */
{
fprintf(f, "%d\t%s\t%s\t%s\t%d\t%d\t%s\t%d\t%d", cluster->id, cg->track->name, cg->gp->name, cg->gp->chrom, cg->gp->txStart, cg->gp->txEnd,
        cg->gp->strand, genePredBases(cg->gp), genePredCodingBases(cg->gp));
if (gDetectConflicted)
    {
    fprintf(f, "\t%c\t%c", (cluster->hasExonConflicts ? 'y' : 'n'),
            (cluster->hasCdsConflicts ? 'y' : 'n'));
    prConflicts(f, cg->exonConflicts);
    prConflicts(f, cg->cdsConflicts);
    }
fprintf(f, "\n");
}

Bits* mkClusterMap(struct cluster *cluster)
/* make a bit map of the exons in a cluster */
{
int len = (cluster->end - cluster->start);
Bits *map = bitAlloc(len);
struct clusterGene *cg;
int exonStart, exonEnd, exonIx;

for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    for (exonIx = 0; exonIx < cg->gp->exonCount; exonIx++)
        if (gpGetExon(cg, exonIx, FALSE, &exonStart, &exonEnd))
            bitSetRange(map, (exonStart-cluster->start), (exonEnd - exonStart));
    }
return map;
}

void outputFlatBed(struct cluster *cluster, char strand, FILE *flatBedFh)
/* output a clusters as a single bed record, with blocks */
{
static struct bed bed;  /* bed buffer */
static int capacity = 0;

int len = (cluster->end - cluster->start);
Bits *map = mkClusterMap(cluster); 
int startIdx = 0;
char nameBuf[64];

/* setup bed */
if (capacity == 0)
    {
    ZeroVar(&bed);
    capacity = 16;
    bed.blockSizes = needMem(capacity*sizeof(int));
    bed.chromStarts = needMem(capacity*sizeof(int));
    }
bed.chrom = cluster->chrom;
bed.chromStart = cluster->start;
bed.chromEnd = cluster->end;
bed.blockCount = 0;
safef(nameBuf, sizeof(nameBuf), "%d", cluster->id);
bed.name = nameBuf;
bed.strand[0] = ((strand == ignoredStrand) ? '+' : strand);
bed.thickStart = cluster->start;
bed.thickEnd = cluster->end;

/* add blocks */
while ((startIdx = bitFindSet(map, startIdx, len)) < len)
    {
    int endIdx = bitFindClear(map, startIdx, len);
    if (bed.blockCount == capacity)
        {
        /* grouw memory in bed buffer */
        int oldSize = capacity*sizeof(int);
        int newSize = capacity*capacity*sizeof(int);
        bed.blockSizes = needMoreMem(bed.blockSizes, oldSize, newSize);
        bed.chromStarts = needMoreMem(bed.chromStarts, oldSize, newSize);
        capacity *= capacity;
        }
    bed.blockSizes[bed.blockCount] = endIdx-startIdx;
    bed.chromStarts[bed.blockCount] = startIdx; 
    bed.blockCount++;
    startIdx = endIdx;
    }
bedTabOutN(&bed, 12, flatBedFh);

bitFree(&map);
}

void outputBed(struct cluster *cluster, int start, int end, char strand,
               FILE *bedFh)
/* output bed should bounds of cluser */
{
fprintf(bedFh, "%s\t%d\t%d\t%d", cluster->chrom, start, end,
        cluster->id);
if (strand != ignoredStrand)
    fprintf(bedFh, "\t0\t%c", strand);
fputc('\n', bedFh);
}

void outputClusters(struct cluster *clusterList, char strand, FILE *outFh,
                    FILE *clBedFh, FILE *clTxBedFh, FILE *flatBedFh)
/* output clusters */
{
struct cluster *cluster;
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    if (cluster->id >= 0)
        {
        struct clusterGene *cg;
        for (cg = cluster->genes; cg != NULL; cg = cg->next)
            prGene(outFh, cluster, cg);
        ++totalClusterCount;
        if (clBedFh != NULL)
            outputBed(cluster, cluster->start, cluster->end, strand, clBedFh);
        if (clTxBedFh != NULL)
            outputBed(cluster, cluster->txStart, cluster->txEnd, strand, clTxBedFh);
        if (flatBedFh != NULL)
            outputFlatBed(cluster, strand, flatBedFh);
        }
    }
}

void clusterGenesOnStrand(struct sqlConnection *conn, struct track* tracks,
                          char *chrom, char strand, FILE *outFh,
                          FILE *clBedFh, FILE *clTxBedFh, FILE *flatBedFh)
/* Scan through genes on this strand, cluster, and write clusters to file. */
{
struct cluster *clusterList = NULL;
struct track *tr;
int chromSize = (conn != NULL) ? hChromSize(sqlGetDatabase(conn), chrom) : 1000000000;
struct clusterMaker *cm = clusterMakerStart(chromSize);

for (tr = tracks; tr != NULL; tr = tr->next)
    loadGenes(cm, conn, tr, chrom, strand);

clusterList = clusterMakerFinish(&cm);
outputClusters(clusterList, strand, outFh, clBedFh, clTxBedFh, flatBedFh);

clusterFreeList(&clusterList);
}

FILE *openOutput(char *outFile)
/* open the output file and write the header */
{
FILE *f = mustOpen(outFile, "w");
fputs("#"
      "cluster\t"
      "table\t"
      "gene\t"
      "chrom\t"
      "txStart\t"
      "txEnd\t"
      "strand\t"
      "rnaLength\t"
      "cdsLength", f);
if (gDetectConflicted)
    fputs("\thasExonConflicts\t"
          "hasCdsConflicts\t"
          "exonConflicts\t"
          "cdsConflicts", f);
fputs("\n", f);
return f;
}

void clusterGenes(char *outFile, char *database, int specCount, char *specs[])
/* clusterGenes - Cluster genes from genePred tracks. */
{
struct slName *chroms, *chrom;
struct sqlConnection *conn = NULL;
struct track *tracks;
FILE *outFh = NULL;
FILE *clBedFh = NULL;
FILE *clTxBedFh = NULL;
FILE *flatBedFh = NULL;

if (!sameString(database, "no"))
    {
    conn = hAllocConn(database);
    }

tracks  = buildTracks(conn, specCount, specs);
chroms = getChroms(conn, tracks);

outFh = openOutput(outFile);
if (optionExists("clusterBed"))
    clBedFh = mustOpen(optionVal("clusterBed", NULL), "w");
if (optionExists("clusterTxBed"))
    clTxBedFh = mustOpen(optionVal("clusterTxBed", NULL), "w");
if (optionExists("flatBed"))
    flatBedFh = mustOpen(optionVal("flatBed", NULL), "w");

for (chrom = chroms; chrom != NULL; chrom = chrom->next)
    {
    if (gIgnoreStrand)
        clusterGenesOnStrand(conn, tracks, chrom->name, ignoredStrand, outFh, clBedFh, clTxBedFh, flatBedFh);
    else 
        {
        clusterGenesOnStrand(conn, tracks, chrom->name, '+', outFh, clBedFh, clTxBedFh, flatBedFh);
        clusterGenesOnStrand(conn, tracks, chrom->name, '-', outFh, clBedFh, clTxBedFh, flatBedFh);
        }
    }
carefulClose(&clBedFh);
carefulClose(&clTxBedFh);
carefulClose(&flatBedFh);
carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
gUseCds = optionExists("cds");
gTrackNames = optionExists("trackNames");
gJoinContained = optionExists("joinContained");
gDetectConflicted = optionExists("conflicted");
gIgnoreStrand = optionExists("ignoreStrand");
gIgnoreBases = optionInt("ignoreBases", gIgnoreBases);
if (!gTrackNames)
    {
    if (argc < 4)
        usage();
    }
else
    {
    if ((argc < 5) || !(argc & 1))
        usage();
    }
clusterGenes(argv[1], argv[2], argc-3, argv+3);
return 0;
}
