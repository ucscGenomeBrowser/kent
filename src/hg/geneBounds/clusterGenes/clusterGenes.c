/* clusterGenes - Cluster genes from genePred tracks. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "jksql.h"
#include "genePred.h"
#include "binRange.h"
#include "hdb.h"

static char const rcsid[] = "$Id: clusterGenes.c,v 1.3 2003/12/21 06:34:36 kent Exp $";

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
  "and the table parameters are tables in genePred format in that database\n"
  "options:\n"
  "   -verbose=N - Print copious debugging info. 0 for none, 3 for loads\n"
  "   -chrom=chrN - Just work on one chromosome\n"
  );
}

static struct optionSpec options[] = {
   {"verbose", OPTION_INT},
   {"chrom", OPTION_STRING},
   {NULL, 0},
};

struct cluster
/* A cluster of overlapping genes. */
    {
    struct cluster *next;	/* Next in list. */
    struct hash *geneHash;	/* Associated genes. */
    int start,end;		/* Range covered by cluster. */
    };

void clusterDump(struct cluster *cluster)
/* Dump contents of cluster to stdout. */
{
struct hashEl *helList = hashElListHash(cluster->geneHash);
struct hashEl *hel;
printf("%d-%d", cluster->start, cluster->end);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct genePred *gp = hel->val;
    printf(" %s", gp->name);
    }
printf("\n");
}

struct cluster *clusterNew()
/* Create new cluster. */
{
struct cluster *cluster;
AllocVar(cluster);
cluster->geneHash = hashNew(5);
return cluster;
}

void clusterFree(struct cluster **pCluster)
/* Free up a cluster. */
{
struct cluster *cluster = *pCluster;
if (cluster == NULL)
    return;
hashFree(&cluster->geneHash);
freez(pCluster);
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

void clusterAddExon(struct cluster *cluster,
	int start, int end, char *table, struct genePred *gp)
/* Add exon to cluster. */
{
char tableGeneName[256];
safef(tableGeneName, sizeof(tableGeneName), "%s\t%s", table, gp->name);
if (!hashLookup(cluster->geneHash, tableGeneName))
    hashAdd(cluster->geneHash, tableGeneName, gp);
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
	int start, int end, char *table, struct genePred *gp)
/* Add exon to cluster and binKeeper. */
{
clusterAddExon(clusterNode->val, start, end, table, gp);
binKeeperAdd(bk, start, end, clusterNode);
}

void mergeClusters(struct binKeeper *bk, struct binElement *bkRest,
	struct dlNode *aNode, struct dlNode *bNode)
/* Move bNode into aNode. */
{
struct cluster *aCluster = aNode->val;
struct cluster *bCluster = bNode->val;
struct binElement *bkEl;
struct hashEl *hEl, *hList;

if (verbose >= 3) 
    {
    printf(" a: ");
    clusterDump(aCluster);
    printf(" b: ");
    clusterDump(bCluster);
    }

/* First change references to bNode. */
binKeeperReplaceVal(bk, bCluster->start, bCluster->end, bNode, aNode);
for (bkEl = bkRest; bkEl != NULL; bkEl = bkEl->next)
    if (bkEl->val == bNode) 
        bkEl->val = aNode;

/* Add b's genes to a. */
hList = hashElListHash(bCluster->geneHash);
for (hEl = hList; hEl != NULL; hEl = hEl->next)
    hashAdd(aCluster->geneHash, hEl->name, hEl->val);

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
    printf(" ab: ");
    clusterDump(aCluster);
    }
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

/* Clean up and go home. */
binKeeperFree(&cm->bk);
dlListFree(&cm->clusters);
freez(pCm);
return clusterList;
}


void clusterMakerAdd(struct clusterMaker *cm, char *table, struct genePred *gp)
/* Add gene to clusterMaker. */
{
struct dlNode *node;
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
    printf("%s %d-%d\n", gp->name, gp->txStart, gp->txEnd);
for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    int exonStart = gp->exonStarts[exonIx];
    int exonEnd = gp->exonEnds[exonIx];
    struct binElement *bEl, *bList = binKeeperFind(cm->bk, exonStart, exonEnd);
    if (verbose >= 4)
	printf("  %d %d\n", exonIx, exonStart);
    if (bList == NULL)
	{
	if (oldNode == NULL)
	    {
	    struct cluster *cluster = clusterNew();
	    oldNode = dlAddValTail(cm->clusters, cluster);
	    }
	addExon(cm->bk, oldNode, exonStart, exonEnd, table, gp);
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
			printf("Merging %p %p\n", oldNode, newNode);
		    mergeClusters(cm->bk, bEl->next, oldNode, newNode);
		    }
		}
	    }
	addExon(cm->bk, oldNode, exonStart, exonEnd, table, gp);
	slFreeList(&bList);
	}
    }
}

int clusterId = 0;	/* Assign a unique id to each cluster. */

void clusterGenesOnStrand(struct sqlConnection *conn,
	int tableCount, char *tables[], char *chrom, char strand, 
	FILE *f)
/* Scan through genes on this strand, cluster, and write clusters to file. */
{
struct sqlResult *sr;
char **row;
int rowOffset;
struct genePred *gp, *gpList = NULL;
char extraWhere[64], query[256];
struct cluster *clusterList = NULL, *cluster;
int nameLen;
int tableIx;
struct clusterMaker *cm = clusterMakerStart(hChromSize(chrom));

for (tableIx = 0; tableIx < tableCount; ++tableIx)
    {
    char *table = tables[tableIx];
    if (!hTableExists(table))
        errAbort("Table %s doesn't exist in %s", table, hGetDb());
    if (verbose >= 1)
	printf("%s %s %c\n", table, chrom, strand);
    safef(extraWhere, sizeof(extraWhere), "strand = '%c'", strand);
    sr = hChromQuery(conn, table, chrom, extraWhere, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	gp = genePredLoad(row + rowOffset);
	slAddHead(&gpList, gp);
	clusterMakerAdd(cm, table, gp);
	++totalGeneCount;
	}
    sqlFreeResult(&sr);
    }
clusterList = clusterMakerFinish(&cm);
fprintf(f, "#");
fprintf(f, "cluster\t");
fprintf(f, "table\t");
fprintf(f, "gene\t");
fprintf(f, "chrom\t");
fprintf(f, "txStart\t");
fprintf(f, "txEnd\n");
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    struct hashEl *helList = hashElListHash(cluster->geneHash);
    struct hashEl *hel;
    char *protName;
    ++clusterId;
    for (hel = helList; hel != NULL; hel = hel->next)
	{
	struct genePred *gp = hel->val;
	fprintf(f, "%d\t%s\t%s\t%d\t%d\n", clusterId, hel->name, gp->chrom, gp->txStart, gp->txEnd);
	}
    ++totalClusterCount;
    }
genePredFreeList(&gp);
}


void clusterGenes(char *outFile, char *database, int tableCount, char *tables[])
/* clusterGenes - Cluster genes from genePred tracks. */
{
struct slName *chromList, *chrom;
struct sqlConnection *conn;
int tableIx;
FILE *f = mustOpen(outFile, "w");

hSetDb(database);
if (optionExists("chrom"))
    chromList = slNameNew(optionVal("chrom", NULL));
else
    chromList = hAllChromNames();
conn = hAllocConn();
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    clusterGenesOnStrand(conn, tableCount, tables, chrom->name, '+', f);
    clusterGenesOnStrand(conn, tableCount, tables, chrom->name, '-', f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
verbose = optionInt("verbose", verbose);
if (argc < 4)
    usage();
clusterGenes(argv[1], argv[2], argc-3, argv+3);
return 0;
}
