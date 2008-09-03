/* hgClusterGenes - Cluster overlapping gene predictions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "jksql.h"
#include "genePred.h"
#include "hdb.h"
#include "hgRelate.h"
#include "binRange.h"
#include "rbTree.h"

static char const rcsid[] = "$Id: hgClusterGenes.c,v 1.12 2008/09/03 19:20:41 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgClusterGenes - Cluster overlapping gene predictions\n"
  "usage:\n"
  "   hgClusterGenes database geneTable clusterTable cannonicalTable\n"
  "where clusterTable pairs clusterIds and geneIds, and cannonicalTable\n"
  "pairs the longest protein isoform with the clusterId\n"
  "options:\n"
  "   -chrom=chrN - Just work on one chromosome\n"
  "   -verbose=N - Print copious debugging info. 0 for none, 3 for loads\n"
  "   -noProt - Skip protein field\n"
  "   -sangerLinks - Use sangerLinks table for protein\n"
  "   -protAccQuery='query' - Use query to retrieve gene->protein map\n"
  );
}

boolean noProt = FALSE;
boolean sangerLinks = FALSE;
char *protAccQuery = NULL;

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"noProt", OPTION_BOOLEAN},
   {"sangerLinks", OPTION_BOOLEAN},
   {"protAccQuery", OPTION_STRING},
   {NULL, 0},
};

struct cluster
/* A cluster of overlapping genes. */
    {
    struct cluster *next;	/* Next in list. */
    struct hash *geneHash;	/* Associated genes. */
    int start,end;		/* Range covered by cluster. */
    };

void clusterDump(int verbosity, struct cluster *cluster)
/* Dump contents of cluster to log. */
{
struct hashEl *helList = hashElListHash(cluster->geneHash);
struct hashEl *hel;
verbose(verbosity, "%d-%d", cluster->start, cluster->end);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct genePred *gp = hel->val;
    verbose(verbosity, " %s", gp->name);
    }
verbose(verbosity, "\n");
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
	int start, int end, struct genePred *gp)
/* Add exon to cluster. */
{
if (!hashLookup(cluster->geneHash, gp->name))
    hashAdd(cluster->geneHash, gp->name, gp);
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
	int start, int end, struct genePred *gp)
/* Add exon to cluster and binKeeper. */
{
clusterAddExon(clusterNode->val, start, end, gp);
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

verbose(3, " a: ");
clusterDump(3, aCluster);
verbose(3, " b: ");
clusterDump(3, bCluster);

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
verbose(3, " ab: ");
clusterDump(3, aCluster);
}

int totalGeneCount = 0;
int totalClusterCount = 0;

struct cluster *makeCluster(struct genePred *geneList, int chromSize)
/* Create clusters out of overlapping genes. */
{
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct dlList *clusters = newDlList();
struct dlNode *node;
struct cluster *clusterList = NULL, *cluster;
struct genePred *gp;

/* Build up cluster list with aid of binKeeper.
 * For each exon look to see if it overlaps an
 * existing cluster.  If so put it into existing
 * cluster, otherwise make a new cluster.  The hard
 * case is where part of the gene is already in one
 * cluster and the exon overlaps with a new
 * cluster.  In this case merge the new cluster into
 * the old one. */
for (gp = geneList; gp != NULL; gp = gp->next)
    {
    int exonIx;
    struct dlNode *oldNode = NULL;
    verbose(2, "%s %d-%d\n", gp->name, gp->txStart, gp->txEnd);
    for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
        {
	int exonStart = gp->exonStarts[exonIx];
	int exonEnd = gp->exonEnds[exonIx];
	struct binElement *bEl, *bList = binKeeperFind(bk, exonStart, exonEnd);
	verbose(4, "  %d %d\n", exonIx, exonStart);
	if (bList == NULL)
	    {
	    if (oldNode == NULL)
		{
		struct cluster *cluster = clusterNew();
		oldNode = dlAddValTail(clusters, cluster);
		}
	    addExon(bk, oldNode, exonStart, exonEnd, gp);
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
			verbose(3, "Merging %p %p\n", oldNode, newNode);
			mergeClusters(bk, bEl->next, oldNode, newNode);
			}
		    }
		}
	    addExon(bk, oldNode, exonStart, exonEnd, gp);
	    slFreeList(&bList);
	    }
	}
    }

/* We build up the cluster list as a doubly-linked list
 * to make it faster to remove clusters that get merged
 * into another cluster.  At the end though we make
 * a singly-linked list out of it. */
for (node = clusters->tail; !dlStart(node); node=node->prev)
    {
    cluster = node->val;
    slAddHead(&clusterList, cluster);
    }
slSort(&clusterList, clusterCmp);

/* Clean up and go home. */
binKeeperFree(&bk);
dlListFree(&clusters);
return clusterList;
}

void createClusterTable(struct sqlConnection *conn, 
	char *tableName, int longestName)
/* Create cluster table. */
{
struct dyString *dy = newDyString(1024);
dyStringPrintf(dy,
    "CREATE TABLE %s (\n"
    " clusterId int unsigned not null,\n"
    " transcript varchar(255) not null,\n"
    " INDEX(transcript(%d)),\n"
    " INDEX(clusterId))\n"
    , tableName, longestName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

void createCannonicalTable(struct sqlConnection *conn, 
	char *tableName, int longestName)
/* Create cannonical representative of cluster table. */
{
struct dyString *dy = newDyString(1024);
dyStringPrintf(dy,
    "CREATE TABLE %s (\n"
    " chrom varchar(255) not null,\n"
    " chromStart int not null,\n"
    " chromEnd int not null,\n"
    " clusterId int unsigned not null,\n"
    " transcript varchar(255) not null,\n"
    " protein varchar(255) not null,\n"
    " UNIQUE(clusterId),\n"
    " INDEX(transcript(%d)),\n"
    " INDEX(protein(8)))\n"
    , tableName, longestName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

int clusterId = 0;	/* Assign a unique id to each cluster. */
int longestName = 1;	/* Longest gene name. */

void clusterGenesOnStrand(char *database, struct sqlConnection *conn,
	char *geneTable, char *chrom, char strand, 
	FILE *clusterFile, FILE *canFile)
/* Scan through genes on this strand, cluster, and write clusters to file. */
{
struct sqlResult *sr;
char **row;
int rowOffset;
struct genePred *gp, *gpList = NULL;
char extraWhere[64], query[256];
struct cluster *clusterList = NULL, *cluster;
int nameLen;
struct hash *protHash = NULL;

verbose(1, "%s %c\n", chrom, strand);
safef(extraWhere, sizeof(extraWhere), "strand = '%c'", strand);
sr = hChromQuery(conn, geneTable, chrom, extraWhere, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row + rowOffset);
    nameLen = strlen(gp->name);
    slAddHead(&gpList, gp);
    if (nameLen > longestName)
        longestName = nameLen;
    ++totalGeneCount;
    }
sqlFreeResult(&sr);

/* Build hash to map between transcript names and protein IDs. */
if (!noProt)
    {
    protHash = newHash(16);
    if (sangerLinks)
	safef(query, sizeof(query), "select orfName,protName from sangerLinks");
    else if (isNotEmpty(protAccQuery))
	safef(query, sizeof(query), protAccQuery);
    else
	safef(query, sizeof(query), "select name, proteinId from %s", geneTable);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	hashAdd(protHash, row[0], cloneString(row[1]));
    sqlFreeResult(&sr);
    }

slReverse(&gpList);
clusterList = makeCluster(gpList, hChromSize(database, chrom));
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    struct hashEl *helList = hashElListHash(cluster->geneHash);
    struct hashEl *hel;
    struct genePred *cannonical = NULL;
    char *protName;
    int cannonicalSize = -1, size = 0;
    ++clusterId;
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	struct genePred *gp = hel->val;
	fprintf(clusterFile, "%d\t%s\n", clusterId, gp->name);
	size = genePredCodingBases(gp);
	if (size > cannonicalSize)
	    {
	    cannonical = gp;
	    cannonicalSize = size;
	    }
	}
    protName = cannonical->name;
    if (protHash != NULL)
	{
	char *newVal = hashFindVal(protHash, protName);
	if (newVal != NULL)
	    protName = newVal;
	}
    fprintf(canFile, "%s\t%d\t%d\t%d\t%s\t%s\n", 
    	chrom, cannonical->txStart, cannonical->txEnd, clusterId, cannonical->name,
	protName);
    ++totalClusterCount;
    }
genePredFreeList(&gpList);
freeHashAndVals(&protHash);
}

void hgClusterGenes(char *database, char *geneTable, char *clusterTable,
	char *cannonicalTable)
/* hgClusterGenes - Cluster overlapping gene predictions. */
{
struct slName *chromList, *chrom;
FILE *clusterFile = hgCreateTabFile(".", clusterTable);
FILE *canFile = hgCreateTabFile(".", cannonicalTable);
struct sqlConnection *conn;

if (optionExists("chrom"))
    chromList = slNameNew(optionVal("chrom", NULL));
else
    chromList = hAllChromNames(database);
conn = hAllocConn(database);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    clusterGenesOnStrand(database, conn, geneTable, chrom->name, '+', 
    	clusterFile, canFile);
    clusterGenesOnStrand(database, conn, geneTable, chrom->name, '-', 
    	clusterFile, canFile);
    }
createClusterTable(conn, clusterTable, longestName);
createCannonicalTable(conn, cannonicalTable, longestName);
verbose(1, "Loading database\n");
hgLoadTabFile(conn, ".", clusterTable, &clusterFile);
hgLoadTabFile(conn, ".", cannonicalTable, &canFile);
verbose(1, "Got %d clusters, from %d genes in %d chromosomes\n",
	totalClusterCount, totalGeneCount, slCount(chromList));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
noProt = optionExists("noProt");
sangerLinks = optionExists("sangerLinks");
protAccQuery = optionVal("protAccQuery", protAccQuery);
if (argc != 5)
    usage();
hgClusterGenes(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
