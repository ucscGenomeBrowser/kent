/* clusterGenes - Cluster genes from genePred tracks. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "dlist.h"
#include "jksql.h"
#include "genePred.h"
#include "binRange.h"
#include "hdb.h"

static char const rcsid[] = "$Id: clusterGenes.c,v 1.9 2004/02/02 01:46:14 markd Exp $";

/* Command line driven variables. */
int verbose = 0;
char *clChrom = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterGenes - Cluster genes from genePred tracks\n"
  "usage:\n"
  "   clusterGenes outputFile database table1 ... tableN\n"
  "Where outputFile is a tab-separated file describing the clustering,\n"
  "database is a genome database such as mm4 or hg16,\n"
  "and the table parameters are either tables in genePred format in that\n"
  "database or genePred tab seperated files.\n"
  "options:\n"
  "   -verbose=N - Print copious debugging info. 0 for none, 3 for loads\n"
  "   -chrom=chrN - Just work on one chromosome\n"
  "   -chromFile=file - Just work on chromosomes listed in this file\n"
  "   -cds - cluster only on CDS exons\n"
  "   -conflicts=file - Output information clusters where there are genes\n"
  "    that don't share exons.  This indicates case where clusters maybe.\n"
  "    incorrectly merged or split. Conflicts maybe either internal to \n"
  "    a table or between tables.\n"
  );
}

static struct optionSpec options[] = {
   {"verbose", OPTION_INT},
   {"chrom", OPTION_STRING},
   {"chromFile", OPTION_STRING},
   {"cds", OPTION_BOOLEAN},
   {"conflicts", OPTION_STRING},
   {NULL, 0},
};

/* from command line  */
boolean useCds;

struct track
/*  Object representing a track. Only one object is allocated for a track,
 *  pointers can be compared */
{
    struct track *next;
    char *name;
};

/* Global list of tracks (tables) in use. */
struct track* allTracks = NULL;

struct track* getTrack(char* name)
/* find the track object for name, creating if needed */
{
struct track* track;
for (track = allTracks; track != NULL; track = track->next)
    {
    if (sameString(track->name, name))
        return track;
    }
AllocVar(track);
track->name = cloneString(name);
slAddTail(&allTracks, track);
return track;
}

boolean gpGetExon(struct genePred* gp, int exonIx, int *exonStartRet, int *exonEndRet)
/* Get the start and end of an exon, adjusting if we are only examining CDS.
 * Return false if exon should not be used.  */
{
int exonStart = gp->exonStarts[exonIx];
int exonEnd = gp->exonEnds[exonIx];
if (useCds)
    {
    if (exonStart < gp->cdsStart)
        exonStart = gp->cdsStart;
    if (exonEnd > gp->cdsEnd)
        exonEnd = gp->cdsEnd;
    }
*exonStartRet = exonStart;
*exonEndRet = exonEnd;
return exonStart < exonEnd;
}

struct clusterGene
/* A gene in a cluster */
{
    struct clusterGene* next;
    struct track* track;       /* aka table for gene */
    struct genePred* gp;
};

struct clusterGene* clusterGeneNew(struct track* track, struct genePred* gp)
/* create a new clusterGene. */
{
struct clusterGene* cg;
AllocVar(cg);
cg->track = track;
cg->gp = gp;
return cg;
}

void clusterGeneFree(struct clusterGene** cgp)
/* Free a clusterGene. */
{
freez(cgp);
}

struct cluster
/* A cluster of overlapping genes. */
    {
    struct cluster *next;	/* Next in list. */
    int id;                     /* id assigned to cluster */
    struct clusterGene *genes;  /* Associated genes. */
    int start,end;		/* Range covered by cluster. */
    };

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
/* Compare to sort based on start. */
{
const struct cluster *a = *((struct cluster **)va);
const struct cluster *b = *((struct cluster **)vb);
return a->start - b->start;
}

struct clusterGene *clusterFindGene(struct cluster *cluster, struct track *track, struct genePred *gp)
/* search for a gene in a cluster.  */
{
struct clusterGene *cg;

for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    if ((cg->gp == gp) && (cg->track == track))
        return cg;
    }
return NULL;
}

void clusterAddExon(struct cluster *cluster,
	int start, int end, struct track *track, struct genePred *gp)
/* Add exon to cluster. */
{
struct clusterGene *cg = clusterFindGene(cluster, track, gp);
if (cg == NULL)
    {
    cg = clusterGeneNew(track, gp);
    slSafeAddHead(&cluster->genes, cg);
    }
if (cluster->start == cluster->end)
    {
    cluster->start = start;
    cluster->end = end;
    }
else
    {
    if (start < cluster->start) cluster->start = start;
    if (cluster->end < end) cluster->end = end;
    }
}

void addExon(struct binKeeper *bk, struct dlNode *clusterNode,
	int start, int end, struct track *track, struct genePred *gp)
/* Add exon to cluster and binKeeper. */
{
clusterAddExon(clusterNode->val, start, end, track, gp);
binKeeperAdd(bk, start, end, clusterNode);
}

void mergeClusters(struct binKeeper *bk, struct binElement *bkRest,
	struct dlNode *aNode, struct dlNode *bNode)
/* Move bNode into aNode. */
{
struct cluster *aCluster = aNode->val;
struct cluster *bCluster = bNode->val;
struct binElement *bkEl;

if (verbose >= 3) 
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
while (bCluster->genes != NULL)
    {
    struct clusterGene *cg = bCluster->genes;
    bCluster->genes = bCluster->genes->next;
    cg->next = NULL;
    slSafeAddHead(&aCluster->genes, cg);
    }

/* Adjust start/end. */
if (bCluster->start < aCluster->start) 
    aCluster->start = bCluster->start;
if (aCluster->end < bCluster->end)
    aCluster->end = bCluster->end;

/* Remove all traces of bNode. */
dlRemove(bNode);
clusterFree(&bCluster);
if (verbose >= 3) 
    {
    fprintf(stderr, " ab: ");
    clusterDump(aCluster);
    }
}


boolean shareExons(struct genePred *gp1, struct genePred *gp2)
/* determine if two genes share exons (maybe restricted to CDS */
{
int exonIx1, exonStart1, exonEnd1;

for (exonIx1 = 0; exonIx1 < gp1->exonCount; exonIx1++)
    {
    if (gpGetExon(gp1, exonIx1, &exonStart1, &exonEnd1))
        {
        /* exonStart2 >= exon1End indicates there can't be overlap on this
         * exon */
        int exonIx2, exonStart2 = 0, exonEnd2;
        for (exonIx2 = 0; (exonIx2 < gp2->exonCount) && (exonStart2 < exonEnd1);
             exonIx2++)
            {
            if (gpGetExon(gp2, exonIx2, &exonStart2, &exonEnd2))
                {
                if ((exonStart2 < exonEnd1) && (exonEnd2 > exonStart1))
                    return TRUE; /* overlaps */
                }
            }
        }
    }
return FALSE;
}

struct conflict
/* object to store conflicting tracks in a cluster */
{
    struct conflict *next;
    struct track *track1;  /* conflicting pair of tracks */
    struct track *track2;
};


void addConflict(struct conflict **conflicts, struct track *track1,
                 struct track *track2)
/* add a pair of conflicting tracks to a list if not already there.
 * order of tracks is not important */
{
struct conflict *con;
for (con = *conflicts; con != NULL; con = con->next)
    {
    if (((con->track1 == track1) && (con->track2 == track2))
        || ((con->track2 == track1) && (con->track1 == track2)))
        break; /* got it */
    }
if (con == NULL)
    {
    AllocVar(con);
    con->track1 = track1;
    con->track2 = track2;
    slAddHead(conflicts, con);
    }
}

void findGeneConflicts(struct cluster *cluster, struct conflict **conflicts,
                       struct clusterGene *gene)
/* find conflicts for a gene in cluster */
{
struct clusterGene *cg;

/* check all other genes */
for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    if ((cg != gene) && !shareExons(cg->gp, gene->gp))
        addConflict(conflicts, gene->track, cg->track);
    }
}

struct conflict *findClusterConflicts(struct cluster *cluster)
/* Get a list of tracks that conflict for a cluster. free with slFreeList */
{
struct conflict *conflicts = NULL;
struct clusterGene *cg;

for (cg = cluster->genes; cg != NULL; cg = cg->next)
    findGeneConflicts(cluster, &conflicts, cg);

return conflicts;
}

int totalGeneCount = 0;
int totalClusterCount = 0;

struct clusterMaker
/* Something that helps us make clusters. */
    {
    struct clusterMaker *next;	/* Next in list */
    struct dlList *clusters;	/* Doubly linked list of clusters. */
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
slSort(&clusterList, clusterCmp);

/* assign ids */
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    cluster->id = nextClusterId++;

/* Clean up and go home. */
binKeeperFree(&cm->bk);
dlListFree(&cm->clusters);
freez(pCm);
return clusterList;
}

void clusterMakerAddExon(struct clusterMaker *cm, struct track *track, struct genePred *gp,
                         int exonStart, int exonEnd, struct dlNode **oldNodePtr)
/* Add a gene exon to clusterMaker. */
{
struct dlNode *oldNode = *oldNodePtr;
struct binElement *bEl, *bList = binKeeperFind(cm->bk, exonStart, exonEnd);
if (bList == NULL)
    {
    if (oldNode == NULL)
        {
        struct cluster *cluster = clusterNew();
        oldNode = dlAddValTail(cm->clusters, cluster);
        }
    addExon(cm->bk, oldNode, exonStart, exonEnd, track, gp);
    }
else
    {
    for (bEl = bList; bEl != NULL; bEl = bEl->next)
        {
        struct dlNode *newNode = bEl->val;
        if (newNode != oldNode)
            {
            if (oldNode == NULL)
                {
                /* Add to existing cluster. */
                oldNode = newNode;
                }
            else
                {
                /* Merge new cluster into old one. */
                if (verbose >= 3)
                    fprintf(stderr, "Merging %p %p\n", oldNode, newNode);
                mergeClusters(cm->bk, bEl->next, oldNode, newNode);
                }
            }
        }
    addExon(cm->bk, oldNode, exonStart, exonEnd, track, gp);
    slFreeList(&bList);
    }
*oldNodePtr = oldNode;
}

void clusterMakerAdd(struct clusterMaker *cm, struct track *track, struct genePred *gp)
/* Add gene to clusterMaker. */
{
int exonIx;
struct dlNode *oldNode = NULL;

/* Build up cluster list with aid of binKeeper.
 * For each exon look to see if it overlaps an
 * existing cluster.  If so put it into existing
 * cluster, otherwise make a new cluster.  The hard
 * case is where part of the gene is already in one
 * cluster and the exon overlaps with a new
 * cluster.  In this case merge the new cluster into
 * the old one. */

if (verbose >= 2)
    fprintf(stderr, "%s %s %d-%d\n", track->name, gp->name, gp->txStart, gp->txEnd);

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    int exonStart, exonEnd;
    if (gpGetExon(gp, exonIx, &exonStart, &exonEnd))
        {
        if (verbose >= 4)
            fprintf(stderr, "  %s %d %d\n", track->name, exonIx, exonStart);
        clusterMakerAddExon(cm, track, gp, exonStart, exonEnd, &oldNode);
        }
    }
}

void loadGenesFromTable(struct clusterMaker *cm, struct sqlConnection *conn,
                        char *table, char *chrom, char strand,
                        struct genePred **gpList)
/* load genes into cluster from a table */
{
char extraWhere[64];
struct sqlResult *sr;
char **row;
int rowOffset;
struct track *track = getTrack(table);

safef(extraWhere, sizeof(extraWhere), "strand = '%c'", strand);
sr = hChromQuery(conn, table, chrom, extraWhere, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(row + rowOffset);
    slAddHead(gpList, gp);
    clusterMakerAdd(cm, track, gp);
    ++totalGeneCount;
    }
sqlFreeResult(&sr);
}

void loadGenesFromFile(struct clusterMaker *cm, char *gpFile, char *chrom,
                       char strand, struct genePred **gpList)
/* load genes into cluster from a table */
{
struct lineFile *lf = lineFileOpen(gpFile, TRUE);
char *row[GENEPRED_NUM_COLS];
char trackName[PATH_LEN];
struct track *track;
splitPath(gpFile, NULL, trackName, NULL);
track = getTrack(trackName);


while (lineFileNextRowTab(lf, row, GENEPRED_NUM_COLS))
    {
    struct genePred *gp = genePredLoad(row);
    if ((gp->strand[0] == strand) && sameString(gp->chrom, chrom))
        {
        slAddHead(gpList, gp);
        clusterMakerAdd(cm, track, gp);
        ++totalGeneCount;
        }
    else
        {
        genePredFree(&gp);
        }
    }
lineFileClose(&lf); 
}

void loadGenes(struct clusterMaker *cm, struct sqlConnection *conn,
               char *table, char *chrom, char strand,
               struct genePred **gpList)
/* load genes into cluster from a table or file */
{
if (verbose >= 1)
    fprintf(stderr, "%s %s %c\n", table, chrom, strand);
if (fileExists(table))
    loadGenesFromFile(cm, table, chrom, strand, gpList);
else if (hTableExists(table))
    loadGenesFromTable(cm, conn, table, chrom, strand, gpList);
else
    errAbort("Table or file %s doesn't exist in %s", table, hGetDb());
}

void reportConflicts(FILE* conflictFh, char* chrom, struct cluster* cluster)
/* look for conflicts in an cluster and report any found. */
{
struct conflict *conflicts = findClusterConflicts(cluster);
struct conflict *con;
for (con = conflicts; con != NULL; con = con->next)
    fprintf(conflictFh, "%d\t%s\t%d\t%d\t%s\t%s\n", cluster->id,
            chrom, cluster->start, cluster->end,
            con->track1->name, con->track2->name);

slFreeList(&conflicts);
}

void clusterGenesOnStrand(struct sqlConnection *conn,
	int tableCount, char *tables[], char *chrom, char strand, 
	FILE *f, FILE* conflictFh)
/* Scan through genes on this strand, cluster, and write clusters to file. */
{
struct genePred *gpList = NULL;
struct cluster *clusterList = NULL, *cluster;
int tableIx;
struct clusterMaker *cm = clusterMakerStart(hChromSize(chrom));

for (tableIx = 0; tableIx < tableCount; ++tableIx)
    loadGenes(cm, conn, tables[tableIx], chrom, strand, &gpList);

clusterList = clusterMakerFinish(&cm);
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    struct clusterGene *cg;
    for (cg = cluster->genes; cg != NULL; cg = cg->next)
	fprintf(f, "%d\t%s\t%s\t%s\t%d\t%d\n", cluster->id, cg->track->name, cg->gp->name, cg->gp->chrom, cg->gp->txStart, cg->gp->txEnd);
    ++totalClusterCount;
    }

if (conflictFh != NULL)
    {
    for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
        reportConflicts(conflictFh, chrom, cluster);
    }

genePredFreeList(&gpList);
}

struct slName* readChromFile(char* chromFile)
/* read chromosomes from a file */
{
struct lineFile *lf = lineFileOpen(chromFile, TRUE);
char *row[1];
struct slName* chroms = NULL;

while (lineFileNextRow(lf, row, ArraySize(row)))
    {
    slSafeAddHead(&chroms, slNameNew(row[0]));
    }

lineFileClose(&lf);
slReverse(&chroms);
return chroms;
}

void clusterGenes(char *outFile, char *database, int tableCount, char *tables[])
/* clusterGenes - Cluster genes from genePred tracks. */
{
struct slName *chromList, *chrom;
struct sqlConnection *conn;
FILE *f = mustOpen(outFile, "w");
char* conflictFile = optionVal("conflicts", NULL);
FILE* conflictFh = NULL;
fprintf(f, "#");
fprintf(f, "cluster\t");
fprintf(f, "table\t");
fprintf(f, "gene\t");
fprintf(f, "chrom\t");
fprintf(f, "txStart\t");
fprintf(f, "txEnd\n");

if (conflictFile != NULL)
    {
    conflictFh = mustOpen(conflictFile, "w");
    fprintf(conflictFh, "#cluster\tchrom\tstart\tend\ttrack1\ttrack2\n");
    }
    

hSetDb(database);
if (optionExists("chrom"))
    chromList = slNameNew(optionVal("chrom", NULL));
else if (optionExists("chromFile"))
    chromList = readChromFile(optionVal("chromFile", NULL));
else
    chromList = hAllChromNames();
conn = hAllocConn();
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    clusterGenesOnStrand(conn, tableCount, tables, chrom->name, '+', f, conflictFh);
    clusterGenesOnStrand(conn, tableCount, tables, chrom->name, '-', f, conflictFh);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
verbose = optionInt("verbose", verbose);
if (argc < 4)
    usage();
useCds = optionExists("cds");
clusterGenes(argv[1], argv[2], argc-3, argv+3);
return 0;
}
