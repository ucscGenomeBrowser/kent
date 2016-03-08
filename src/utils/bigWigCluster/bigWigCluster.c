/* bigWigCluster - Cluster bigWigs using a hactree. */
#include "sqlNum.h"
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hacTree.h"
#include "rainbow.h"
#include "portable.h"

int gThreadCount = 10;
char *gTmpDir = ".";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigCluster - Cluster bigWigs using a hacTree\n"
  "usage:\n"
  "   bigWigCluster input.list chrom.sizes output.json output.tab\n"
  "where: input.list is a list of bigWig file names\n"
  "       chrom.sizes is tab separated <chrom><size> for assembly for bigWigs\n"
  "       output.json is json formatted output suitable for graphing with D3\n"
  "       output.tab is tab-separated file of  of items ordered by tree with the fields\n"
  "           label - label from -labels option or from file name with no dir or extention\n"
  "           pos - number from 0-1 representing position according to tree and distance\n"
  "           red - number from 0-255 representing recommended red component of color\n"
  "           green - number from 0-255 representing recommended green component of color\n"
  "           blue - number from 0-255 representing recommended blue component of color\n"
  "           path - file name from input.list including directory and extension\n" 
  "options:\n"
  "   -labels=fileName - label files from tabSeparated file with fields\n"
  "           path - path to bigWig file\n"
  "           label - a string with no tabs\n"
  "   -precalc=precalc.tab - tab separated file with <file1> <file2> <distance>\n"
  "            columns.\n"
  "   -threads=N - number of threads to use, default %d\n"
  "   -tmpDir=/tmp/path - place to put temp files, default current dir\n"
  , gThreadCount
  );
}

char tmpPrefix[PATH_LEN] = "bwc_tmp_";  /* Prefix for temp file names */

/* Command line validation table. */
static struct optionSpec options[] = {
   {"precalc", OPTION_STRING},
   {"threads", OPTION_INT},
   {"labels", OPTION_STRING},
   {"tmpDir", OPTION_STRING},
   {NULL, 0},
   {"threads", OPTION_INT},
   {"hacTree", OPTION_STRING},
};


double longest = 0;
char* clHacTree = "fromItems";
int clThreads = 10; // The number of threads to run with the multiThreads option
int nameCount = 0;

int bigWigNextId;

struct bigWig
/* Information on a bigWig */
    {
    struct bigWig *next;  //next item in series
    int id;	// Unique numerical identifier
    char *fileName; // Associated file name
    char *label;    // Associated labels if any
    struct rgbColor color; //for coloring
    double pos;	    // Position between 0.0 and 1.0 when ordered by tree and distance
    };

struct bigWig *readBigWigList(char* input)
// get the bigWig files 
{
struct bigWig *list = NULL;
struct lineFile *lf = lineFileOpen(input,TRUE);
char *line = NULL;
while(lineFileNextReal(lf, &line))
    {
    /* Allocate new item and initialize id and fileName fields */
    struct bigWig *bw;
    AllocVar(bw);
    bw->id = ++bigWigNextId;
    bw->fileName = cloneString(trimSpaces(line));

    // Make up default label - same as file name without directory and extension
    char root[FILENAME_LEN];
    splitPath(line, NULL, root, NULL);
    bw->label = cloneString(root);

    /* Add to list */
    slAddHead(&list,bw);
    }

/* Clean up and go home */
lineFileClose(&lf);
slReverse(&list);
return list;
}


static void rAddLeaf(struct hacTree *tree, struct slRef **pList)
/* Recursively add itemOrCluster from leaf nodes of hacTree to list */
{
if (tree->left == NULL && tree->right == NULL)
    refAdd(pList, tree->itemOrCluster);
else
    {
    rAddLeaf(tree->left, pList);
    rAddLeaf(tree->right, pList);
    }
}

struct slRef *getOrderedLeafList(struct hacTree *tree)
/* Return list of references to bigWigs from leaf nodes
 * ordered by position in hacTree */
{
struct slRef *leafList = NULL;
rAddLeaf(tree, &leafList);
slReverse(&leafList);
return leafList;
}

static void rPrintHierarchicalJson(FILE *f, struct hacTree *tree, int level, double distance,
		int normConstant, int cgConstant)
/* Recursively prints out the elements of the hierarchical .json file. */
{
struct bigWig *bw = (struct bigWig *)tree->itemOrCluster;
char *label = bw->label;
struct rgbColor colors = bw->color;
int i;
for (i = 0;  i < level;  i++)
    fputc(' ', f); // correct spacing for .json format
if (tree->left == NULL && tree->right == NULL)
    {
    // Prints out the leaf objects
    fprintf(f, "{\"%s\"%s \"%s\"%s\"%s\"%s %f %s\"%s\"%s \"rgb(%i,%i,%i)\"}", "name", ":", label, ", ",
    		"similarity", ":", distance, "," , "colorGroup", ":", colors.r, colors.g, colors.b);
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");

// Prints out the node object and opens a new children block
fprintf(f, "{\"%s\"%s \"%s\"%s", "name", ":", " ", ",");
fprintf(f, "\"colorGroup\": \"rgb(%i,%i,%i)\",", colors.r, colors.g, colors.b );
fprintf(f, "\"%s\"%s \"%f\"%s\n", "distance", ":", 100*tree->childDistance, ",");
for (i = 0;  i < level + 1;  i++)
    fputc(' ', f);
fprintf(f, "\"%s\"%s\n", "children", ": [");
distance = tree->childDistance; 
rPrintHierarchicalJson(f, tree->left, level+1, distance, normConstant, cgConstant);
fputs(",\n", f);
rPrintHierarchicalJson(f, tree->right, level+1, distance, normConstant, cgConstant);
fputc('\n', f);
// Closes the children block for node objects
for (i=0;  i < level + 1;  i++)
    fputc(' ', f);
fputs("]\n", f);
for (i = 0;  i < level;  i++)
    fputc(' ', f);
fputs("}", f);
}

void printHierarchicalJson(char *fileName, struct hacTree *tree, int normConstant, int cgConstant)
/* Prints out the binary tree into .json format intended for d3
 * hierarchical layouts */
{
FILE *f = mustOpen(fileName, "w");
if (tree == NULL)
    {
    fputs("Empty tree.\n", f);
    return;
    }
rPrintHierarchicalJson(f, tree, 0, 0, normConstant, cgConstant);
fputc('\n', f);
carefulClose(&f);
}

void printOrderedTab(char *fileName, struct slRef *refList)
/* Print list of references to bigWigs */
{
FILE *f = mustOpen(fileName, "w");
struct slRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct bigWig *bw = ref->val;
    fprintf(f, "%s\t%g\t%d\t%d\t%d\t%s\n", bw->label, bw->pos,
	bw->color.r, bw->color.g, bw->color.b, bw->fileName);
    }
carefulClose(&f);
}

double slBigWigDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Return the absolute difference between the two kids' values. 
 * Designed for HAC tree use*/
{
verbose(1,"Calculating Distance...\n");
const struct bigWig *kid1 = (const struct bigWig *)item1;
const struct bigWig *kid2 = (const struct bigWig *)item2;
char tmpName[PATH_LEN];
safef(tmpName, sizeof(tmpName), "%s%d_%d.txt", tmpPrefix, kid1->id, kid2->id);
char cmd[1024];
safef(cmd, 1024, "bigWigCorrelate %s %s > %s", kid1->fileName, kid2->fileName, tmpName);
double diff = 0;
mustSystem(cmd);
struct lineFile *lf = lineFileOpen(tmpName,TRUE);
char *line = NULL;
if (!lineFileNext(lf, &line, NULL))
    errAbort("no difference output, check bigWigCorrelate");
lineFileClose(&lf);
diff = sqlDouble(line);
remove(tmpName);
return  1 - diff;
}


struct slList *slBigWigMerge(const struct slList *item1, const struct slList *item2,
				void *extraData)
/* Make a new slPair where the name is the children names concattenated and the 
 * value is the average of kids' values.
 * Designed for HAC tree use*/
{
char *chromSizesFile = extraData;
verbose(1,"Merging...\n");
struct bigWig *result;
AllocVar(result);
const struct bigWig *kid1 = (const struct bigWig *)item1;
const struct bigWig *kid2 = (const struct bigWig *)item2;
char tmpName[PATH_LEN];
safef(tmpName, sizeof(tmpName), "%s%d_%d.bedGraph", tmpPrefix, kid1->id, kid2->id);
char cmd1[1024];
char cmd2[1024];
safef(cmd1, 1024, "bigWigMerge %s %s %s -verbose=0", kid1->fileName, kid2->fileName, tmpName);
char fileName[PATH_LEN];
safef(fileName, sizeof(fileName), "%s%d_%d.bw", tmpPrefix, kid1->id, kid2->id);
safef(cmd2, 1024, "bedGraphToBigWig %s %s %s", tmpName, chromSizesFile, fileName);
mustSystem(cmd1);
mustSystem(cmd2);
remove(tmpName);
result->id = ++bigWigNextId;
result->fileName = cloneString(fileName);
result->label = " ";
return (struct slList *)result;
}

double findCachedDistance(struct hash *distanceHash, struct bigWig *bw1, struct bigWig *bw2)
/* Return cached distance - from hash if possible */
{
double distance;
double *cached = hacTreeDistanceHashLookup(distanceHash, bw1, bw2);
if (cached != NULL)
   distance = *cached;
else
   {
   distance = slBigWigDistance((struct slList *)bw1, (struct slList *)bw2, NULL);
   hacTreeDistanceHashAdd(distanceHash, bw1, bw2, distance);
   }
return distance;
}


void colorLeaves(struct slRef *leafList, struct hash *distanceHash)
/* Assign colors of rainbow to leaves. */
{
/* Loop through list once to figure out total, since we need to
 * normalize */
double total = 0;
double purplePos = 0.80;
struct slRef *el, *nextEl;
for (el = leafList; el != NULL; el = nextEl)
   {
   nextEl = el->next;
   if (nextEl == NULL)
       break;
   double distance = findCachedDistance(distanceHash, el->val, nextEl->val);
   total += distance;
   }

/* Loop through list a second time to generate actual colors. */
double soFar = 0;
for (el = leafList; el != NULL; el = nextEl)
   {
   nextEl = el->next;
   if (nextEl == NULL)
       break;
   struct bigWig *bw = nextEl->val;
   double distance = findCachedDistance(distanceHash, el->val, bw);
   soFar += distance;
   double normalized = soFar/total;
   bw->pos = normalized;
   bw->color = saturatedRainbowAtPos(normalized * purplePos);
   }

/* Set first color to correspond to 0, since not set in above loop */
struct bigWig *bw = leafList->val;
bw->color = saturatedRainbowAtPos(0);
}

void preloadDistances(char *distanceFile, struct bigWig *itemList, struct hash *distanceHash)
/* Load tab-separated distanceFile into a hash, making sure all items
 * in distanceFile correspond to file in list */
{
/* Create hash of items in list keyed by file name */
struct hash *itemHash = hashNew(0);
struct bigWig *item;
for (item = itemList; item != NULL; item = item->next)
    hashAdd(itemHash, item->fileName, item);

/* Loop through distance file's three columns building up distance hash. */
struct lineFile *lf = lineFileOpen(distanceFile, TRUE);
char *row[3];
while (lineFileRow(lf, row))
    {
    char *a = row[0], *b = row[1];
    double correlation = sqlDouble(row[2]);
    double distance = 1.0 - correlation;
    struct bigWig *aItem = hashMustFindVal(itemHash, a);
    struct bigWig *bItem = hashMustFindVal(itemHash, b);
    hacTreeDistanceHashAdd(distanceHash, aItem, bItem, distance);
    hacTreeDistanceHashAdd(distanceHash, bItem, aItem, distance);
    }

/* Report, clean up and go home. */
verbose(1, "Preloaded %d distances on %d items from %s\n", distanceHash->elCount, itemHash->elCount,
    distanceFile);
lineFileClose(&lf);
hashFree(&itemHash);
}

void addLabels(char *labelFile, struct bigWig *list)
/* Update label field of bigWig list according to labels in tab-separated file of format
 *    <fileName> <labels> */
{
/* Build up hash of labels keyed by file name */
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(labelFile, TRUE);
char *row[2];
while (lineFileRowTab(lf, row))
     hashAdd(hash, row[0], cloneString(row[1]));
lineFileClose(&lf);

/* Loop through list looking up labels in hash */
struct bigWig *bw;
for (bw = list; bw != NULL; bw = bw->next)
     {
     bw->label = hashFindVal(hash, bw->fileName);
     if (bw->label == NULL)
         errAbort("No label for %s\n", bw->fileName);
     }

/* Clean up and go home */
freeHash(&hash);
}

void bigWigCluster(char *inputList, char *chromSizes, char *outputJson, char *outputTab)
/* bigWigCluster - Cluster bigWigs using a hactree. */
{
/* Read in input hash */
struct bigWig *list = readBigWigList(inputList);

/* Set up distance cache, preloading it if possible from file */
struct hash *distanceHash = hashNew(0);
char *precalcFile = optionVal("precalc", NULL);
if (precalcFile)
    preloadDistances(precalcFile, list, distanceHash);

/* Supply better labels if they exist */
char *labelFile = optionVal("labels", NULL);
if (labelFile)
    addLabels(labelFile, list);

struct hacTree *clusters;
struct lm *localMem = lmInit(0);
if (gThreadCount > 1)
    {
    clusters = hacTreeMultiThread(gThreadCount, (struct slList *)list, localMem,
					    slBigWigDistance, slBigWigMerge, NULL, chromSizes, distanceHash);
    }
else
    {
    /* Use older code for single threaded case, just to be able to compare results to
     * parallelized version */
    clusters = hacTreeFromItems((struct slList *)list, localMem,
					    slBigWigDistance, slBigWigMerge, NULL, chromSizes);
    }

/* Convert tree to ordered list, do coloring, and make outputs */
struct slRef *orderedList = getOrderedLeafList(clusters);
colorLeaves(orderedList, distanceHash);
printHierarchicalJson(outputJson, clusters, 20, 20);
printOrderedTab(outputTab, orderedList);

// Clean up remaining temp files 
char cmd[1024];
safef(cmd, sizeof(cmd), "rm %s*", tmpPrefix);
mustSystem(cmd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clThreads = optionInt("threads", clThreads);
clHacTree = optionVal("hacTree", clHacTree);
if (argc != 5)
    usage();
gThreadCount = optionInt("threads", gThreadCount);
gTmpDir = optionVal("tmpDir", gTmpDir);
safef(tmpPrefix, sizeof(tmpPrefix), "%s/bwc_tmp_", gTmpDir);
bigWigCluster(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
