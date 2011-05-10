/* regClusterTreeCells - Create a binary tree of cell types based on hierarchical clustering (Eisen style) of expression data.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "bedGraph.h"
#include "hmmstats.h"
#include "obscure.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regClusterTreeCells - Create a binary tree of cell types based on hierarchical clustering\n"
  "(Eisen style) of expression data.\n"
  "usage:\n"
  "   regClusterTreeCells inFiles.lst output.tree output.distances\n"
  "Where inFiles.lst is a list of bedGraph format files: <chrom><start><end><score>\n"
  "options:\n"
  "   -short - Abbreviate output by removing stuff shared by all.\n"
  "   -rename=twoCol.tab - Rename files according to file/name columns\n"
  );
}

struct hash *renameHash = NULL;

static struct optionSpec options[] = {
   {"short", OPTION_BOOLEAN},
   {"rename", OPTION_STRING},
   {NULL, 0},
};

struct treeNode
/* An element in a hierarchical binary tree.  */
    {
    struct treeNode *left, *right;	/* Two children */
    int vectorSize;		/* # of items in vector below. */
    float *vector;		/* Array of doubles, in this case gene expression values. */
    char *fileName;		/* File data came from. */
    boolean merged;		/* If true, has been merged. */
    };

struct bedGraph *readTabFileAsBedGraph(
	char *fileName, int chromIx, int startIx, int endIx, int dataIx, struct lm *lm)
/* Read in indicated columns from a tab-separated file and create a list of bedGraphs from them. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[128];
int maxRowSize = ArraySize(row);
int rowSize;
struct bedGraph *list = NULL, *el;

/* Figure out minimum number of columns for file. */
int maxIx = chromIx;
if (maxIx < startIx) maxIx = startIx;
if (maxIx < endIx) maxIx = endIx;
if (maxIx < dataIx) maxIx = dataIx;

while ((rowSize = lineFileChopNext(lf, row, maxRowSize)) != 0)
    {
    /* Check row is not too big, but not too small either. */
    if (rowSize == maxRowSize)
        errAbort("Too many words line %d of %s, can only handle %d\n", lf->lineIx, lf->fileName,
		maxRowSize-1);
    if (rowSize <= maxIx)
        errAbort("Need at least %d columns, got %d, line %d of %s\n", maxIx+1, rowSize,
		lf->lineIx, lf->fileName);

    /* Allocate and fill in bedGraph from local memory pool */
    lmAllocVar(lm, el);
    el->chrom = lmCloneString(lm, row[chromIx]);
    el->chromStart = lineFileNeedNum(lf, row, startIx);
    el->chromEnd = lineFileNeedNum(lf, row, endIx);
    el->dataValue = lineFileNeedDouble(lf, row, dataIx);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

static void notIdenticalRegions(struct lineFile *lf)
/* Complain about regions not being identical. */
{
errAbort("Line %d of %s, region not the same as corresponding line of first file",
	lf->lineIx, lf->fileName);
}

struct treeNode *readTabFileAsNode(
	char *fileName, int chromIx, int startIx, int endIx, int dataIx, 
	struct bedGraph *bedList, int vectorSize)
/* Read file into a treeNode.  Check while reading it that the chrom/start/end
 * are the same as bedList.  Effectively this makes sure that all of the files
 * are ordered the same way. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[128];
int maxRowSize = ArraySize(row);
int rowSize;

/* Figure out minimum number of columns for file. */
int maxIx = chromIx;
if (maxIx < startIx) maxIx = startIx;
if (maxIx < endIx) maxIx = endIx;
if (maxIx < dataIx) maxIx = dataIx;

/* Allocate tree node and it's vector. */
struct treeNode *treeNode;
AllocVar(treeNode);
treeNode->fileName = cloneString(fileName);
treeNode->vectorSize = vectorSize;
float *vector = AllocArray(treeNode->vector, vectorSize);

/* Loop through file. */
int count = 0;
struct bedGraph *el = bedList;
while ((rowSize = lineFileChopNext(lf, row, maxRowSize)) != 0)
    {
    /* Check row is not too big, but not too small either, and that there are not too
     * many of them. */
    if (rowSize == maxRowSize)
        errAbort("Too many words line %d of %s, can only handle %d\n", lf->lineIx, lf->fileName,
		maxRowSize-1);
    if (rowSize <= maxIx)
        errAbort("Need at least %d columns, got %d, line %d of %s\n", maxIx+1, rowSize,
		lf->lineIx, lf->fileName);
    if (count >= vectorSize)
        errAbort("Too many items %s.  First file only has %d\n", lf->fileName, vectorSize);

    /* Check we are covering same regions. */
    if (!sameString(row[chromIx], el->chrom))
	notIdenticalRegions(lf);
    if (el->chromStart != lineFileNeedNum(lf, row, startIx))
	notIdenticalRegions(lf);
    if (el->chromEnd != lineFileNeedNum(lf, row, endIx))
	notIdenticalRegions(lf);

    /* Store away the data. */
    vector[count] = lineFileNeedDouble(lf, row, dataIx);

    /* And advance to next one. */
    ++count;
    el = el->next;
    }
if (count != vectorSize)
    errAbort("Expecting %d items in %s, just got %d", vectorSize, lf->fileName, count);
lineFileClose(&lf);
return treeNode;
}

double treeNodeDistance(struct treeNode *aNode, struct treeNode *bNode)
/* Return euclidean distance between vectors of two nodes. */
{
float *av = aNode->vector, *bv = bNode->vector;
int size = aNode->vectorSize;
double sumSquares = 0;
int i;
for (i=0; i<size; ++i)
    {
    double d = av[i] - bv[i];
    sumSquares += d*d;
    }
return sqrt(sumSquares);
}

void normalizeTreeNode(struct treeNode *treeNode)
/* Convert vector to z-scores - subtract mean, divide by standard deviation. */
{
/* Get local vars for stuff we use a bunch. */
float *vector = treeNode->vector;
int size = treeNode->vectorSize;

/* Calculate standard deviation and mean. */
double sum = 0, sumSquares = 0;
int i;
for (i=0; i<size; ++i)
    {
    double x = vector[i];
    sum += x;
    sumSquares += x*x;
    }
float mean = sum/size;
float std = calcStdFromSums(sum, sumSquares, size);
float invStd = 1.0/std;

/* Rescale vector. */
for (i=0; i<size; ++i)
   vector[i] = (vector[i] - mean)*invStd;
}

void findClosestPair(struct treeNode *array[], int nodesUsed, double **distanceMatrix, 
     struct treeNode **retA, struct treeNode **retB)
/* Find closest pair of nodes in array using distance matrix for distance. */
{
int i,j;
double closestDistance = BIGDOUBLE;
struct treeNode *a, *b, *closestA = NULL, *closestB = NULL;
for (i=0; i<nodesUsed; ++i)
    {
    a = array[i];
    if (!a->merged)
	{
	for (j=0; j<i; ++j)
	    {
	    b = array[j];
	    if (!b->merged)
	        {
		double distance = distanceMatrix[i][j];
		if (distance < closestDistance)
		    {
		    closestDistance = distance;
		    closestA = a;
		    closestB = b;
		    }
		}
	    }
	}
    }
*retA = closestA;
*retB = closestB;
}

float *averageVectors(float *a, float *b, int size)
/* Allocate and return a vector that is the average of a & b */
{
float *x = needLargeMem(size * sizeof(float));
int i;
for (i=0; i<size; ++i)
   x[i] = 0.5 * (a[i] + b[i]);
return x;
}

void rOutputTree(FILE *f, struct treeNode *parent, struct treeNode *node, int level, int prefixSize, int suffixSize)
/* Recursively output tree. */
{
if (node->fileName)
    {
    if (renameHash)
        {
	char *s = hashMustFindVal(renameHash, node->fileName);
	mustWrite(f, s, strlen(s));
	}
    else
	{
	char *s = node->fileName + prefixSize;
	int len = strlen(s) - suffixSize;
	mustWrite(f, s, len);
	}
    }
else
    {
    fprintf(f, "(");
    rOutputTree(f, node, node->left, level+1, prefixSize, suffixSize);
    fprintf(f, ",");
    rOutputTree(f, node, node->right, level+1, prefixSize, suffixSize);
    fprintf(f, ")");
    }
if (parent != NULL)
    fprintf(f, ":%g", treeNodeDistance(parent, node));
fprintf(f, " ");
}

char *findCommonPrefix(struct slName *list)
/* Find common prefix to list of names. */
{
/* Deal with short special cases. */
if (list == NULL)
   errAbort("Can't findCommonPrefix on empty list");
if (list->next == NULL)
   return cloneString("");

int prefixSize = BIGNUM;
struct slName *el, *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (next == NULL)
        break;
    int same = countSame(el->name, next->name);
    if (same < prefixSize)
        prefixSize = same;
    }
return cloneStringZ(list->name, prefixSize);
}

int countSameAtEnd(char *a, char *b)
/* Count number of chars at end of string that are the same between a and b. */
{
int aLen = strlen(a), bLen = strlen(b);
int minLen = min(aLen, bLen);
int sameCount = 0;
a += aLen-1;
b += bLen-1;
int i;
for (i=0; i<minLen; ++i)
    {
    if (a[-i] == b[-i])
        ++sameCount;
    else
        break;
    }
return sameCount;
}

char *findCommonSuffix(struct slName *list)
/* Find common suffix to list of names. */
{
/* Deal with short special cases. */
if (list == NULL)
   errAbort("Can't findCommonPrefix on empty list");
if (list->next == NULL)
   return cloneString("");

int suffixSize = BIGNUM;
struct slName *el, *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (next == NULL)
        break;
    int same = countSameAtEnd(el->name, next->name);
    if (same < suffixSize)
        suffixSize = same;
    }
return cloneStringZ(list->name + strlen(list->name) - suffixSize, suffixSize);
}

void outputTree(char *fileName, struct slName *inFileList, struct treeNode *root)
/* Output tree to file. */
{
FILE *f = mustOpen(fileName, "w");
char *commonPrefix = "", *commonSuffix = "";
if (optionExists("short"))
    {
    commonPrefix = findCommonPrefix(inFileList);
    commonSuffix = findCommonSuffix(inFileList);
    }
rOutputTree(f, NULL, root, 0, strlen(commonPrefix), strlen(commonSuffix));
fputc('\n', f);
carefulClose(&f);
}

struct slRef *rRefList = NULL;

void rMakeRefList(struct treeNode *node)
/* Recursively add references to leaf nodes to rRefList */
{
if (node->fileName)
    {
    refAdd(&rRefList, node);
    }
else
    {
    rMakeRefList(node->left);
    rMakeRefList(node->right);
    }
}

struct slRef *makeRefsToLeaves(struct treeNode *root)
/* Make a refList of treeNodes that are leaves ordered according to tree. */
{
rRefList = NULL;
rMakeRefList(root);
slReverse(&rRefList);
return rRefList;
}

void outputDistancesOfNeighbors(char *fileName, struct treeNode *root)
/* Output three column file of format <cellA> <cellB> <distanceA-B>
 * where lines in file or ordered by tree. */
{
struct slRef *ref, *refList = makeRefsToLeaves(root);
FILE *f = mustOpen(fileName, "w");
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct slRef *next = ref->next;
    if (next == NULL)
        next = refList;  // wrap at end
    struct treeNode *a = ref->val;
    struct treeNode *b = next->val;
    char *aString = a->fileName;
    char *bString = b->fileName;
    if (renameHash)
        {
	aString = hashMustFindVal(renameHash, aString);
	bString = hashMustFindVal(renameHash, bString);
	}
    fprintf(f, "%s\t%s\t%g\n", aString, bString, treeNodeDistance(a, b));
    }
carefulClose(&f);
}

void regClusterTreeCells(char *inFiles, char *outTree, char *outDistances)
/* regClusterTreeCells - Create a binary tree of cell types based on hierarchical clustering 
 * (Eisen style) of expression data.. */
{
struct slName *inFileList = readAllLines(inFiles);
int inFileCount = slCount(inFileList);
verbose(1, "Got %d files in %s\n", inFileCount, inFiles);
if (inFileCount < 2)
    errAbort("Can't do this with less than 2 input files");

/* Read in first file as a bedGraph */
char *firstFileName = inFileList->name;
struct lm *lm = lmInit(0);
struct bedGraph *bedList = readTabFileAsBedGraph(firstFileName, 0, 1, 2, 6, lm);
int itemCount = slCount(bedList);
verbose(1, "Got %d items in %s\n", itemCount, firstFileName);

/* Create some arrays big enough for all nodes.  We'll be merging nodes a pair at a time,
 * but creating a new node while doing this. */
int treeNodeCount = inFileCount + inFileCount - 1;
struct treeNode **array;
AllocArray(array, treeNodeCount);

/* Read in initial treeNodes from file. */
/* Create a doubly-linked list of treeNodes, one for each file. */
int nodesUsed = 0;
struct slName *inFile;
for (inFile = inFileList; inFile != NULL; inFile = inFile->next)
    {
    struct treeNode *treeNode = readTabFileAsNode(inFile->name, 0, 1, 2, 6, bedList, itemCount);
    normalizeTreeNode(treeNode);
    array[nodesUsed++] = treeNode;
    verboseDot();
    }
verbose(1, "\nRead %d x %d = %ld data values total\n", inFileCount, itemCount, 
	(long)itemCount * (long)inFileCount);

/* Create look up array for pairwise distances.  This needs to be big enough to hold all nodes,
 * the initial nodes we just read in, and also the ones we create by joining a pair of existing
 * nodes. */
double **distanceMatrix;
AllocArray(distanceMatrix, treeNodeCount);
int i,j;
for (i=0; i<treeNodeCount; ++i)
    {
    distanceMatrix[i] = needLargeZeroedMem((i+1) * sizeof(double));
    }

/* Populate distance array with initial values. */
for (i=0; i<nodesUsed; ++i)
    for (j=0; j<i; ++j)
        distanceMatrix[i][j] = treeNodeDistance(array[i], array[j]);
verbose(1, "done initial distance matrix calcs\n");

/* Main loop - find two nearest,  make new node that is an average of the two,
 * and replace the two with that node until only one left. */
struct treeNode *join = NULL;
for ( ;nodesUsed <treeNodeCount; ++nodesUsed)
    {
    /* Find closest two and build new merged node around it. */
    struct treeNode *aNode, *bNode;
    findClosestPair(array, nodesUsed, distanceMatrix, &aNode, &bNode);
    AllocVar(join);
    join->vectorSize = itemCount;
    join->vector = averageVectors(aNode->vector, bNode->vector, join->vectorSize);
    join->left = aNode;
    join->right = bNode;
    aNode->merged = bNode->merged = TRUE;

    /* Put vector in array, and in distanceMatrix */
    array[nodesUsed] = join;
    distanceMatrix[nodesUsed] = needLargeZeroedMem((nodesUsed+1) * sizeof(double));
    for (j=0; j<nodesUsed; ++j)
        distanceMatrix[nodesUsed][j] = treeNodeDistance(join, array[j]);
    verboseDot();
    }
verbose(1, "\ndone main loop\n");

outputTree(outTree, inFileList, join);
outputDistancesOfNeighbors(outDistances, join);

lmCleanup(&lm);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
char *renameFile = optionVal("rename", NULL);
if (renameFile != NULL)
    renameHash = hashTwoColumnFile(renameFile);
regClusterTreeCells(argv[1], argv[2], argv[3]);
return 0;
}
