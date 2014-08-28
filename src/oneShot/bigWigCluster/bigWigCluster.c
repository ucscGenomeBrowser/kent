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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigCluster - Cluster bigWigs using a hacTree\n"
  "usage:\n"
  "   bigWigCluster input.list chrom.sizes output.json\n"
  "options:\n"
  "   -precalc=precalc.tab - tab separated file with <file1> <file2> <distance>\n"
  "            columns.\n"
  "   -threads=N - number of threads to use, default %d\n"
  , gThreadCount
  );
}

char tmpPrefix[PATH_LEN] = "bwc_tmp_";  /* Prefix for temp file names */

/* Command line validation table. */
static struct optionSpec options[] = {
   {"precalc", OPTION_STRING},
   {"threads", OPTION_INT},
   {NULL, 0},
};

struct bigWig
    {
    struct bigWig *next;  //next item in series
    char *name;	//name of the bigWig filei
    struct rgbColor color; //for coloring
    };

struct bigWig *getBigWigs(char* input)
// get the bigWig files 
{
struct bigWig **list;
AllocVar(list);
char* line = NULL;
int i = 0;
struct lineFile *lf = lineFileOpen(input,TRUE);
while(lineFileNext(lf, &line, NULL))
    {
    ++i;
    struct bigWig *bw;
    AllocVar(bw);
    bw->name = line;
    slAddHead(&list,bw);
    }
slReverse(&list);
return *list;
}



static void rAddLeaf(struct hacTree *tree, struct slRef **pList)
/* Recursively add leaf to list */
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
/* Return list of references to bioExpVectors from leaf nodes
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
struct bigWig *bio = (struct bigWig *)tree->itemOrCluster;
char *tissue = bio->name;
struct rgbColor colors = bio->color;
int i;
for (i = 0;  i < level;  i++)
    fputc(' ', f); // correct spacing for .json format
if (tree->left == NULL && tree->right == NULL)
    {
    // Prints out the leaf objects
    fprintf(f, "{\"%s\"%s \"%s\"%s\"%s\"%s %f %s\"%s\"%s \"rgb(%i,%i,%i)\"}", "name", ":", tissue, ", ",
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

void printHierarchicalJson(FILE *f, struct hacTree *tree, int normConstant, int cgConstant)
/* Prints out the binary tree into .json format intended for d3
 * hierarchical layouts */
{
if (tree == NULL)
    {
    fputs("Empty tree.\n", f);
    return;
    }
double distance = 0;
rPrintHierarchicalJson(f, tree, 0, distance, normConstant, cgConstant);
fputc('\n', f);
}




double slBigWigDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Return the absolute difference between the two kids' values. 
 * Designed for HAC tree use*/
{
verbose(1,"Calculating Distance...\n");
const struct bigWig *kid1 = (const struct bigWig *)item1;
const struct bigWig *kid2 = (const struct bigWig *)item2;
char tmpName[PATH_LEN];
safef(tmpName, sizeof(tmpName), "tmp_%p_%p", kid1, kid2);
char cmd[1024];
safef(cmd, 1024, "bigWigCorrelate %s %s > %s", kid1->name, kid2->name, tmpName);
double diff = 0;
mustSystem(cmd);
struct lineFile *lf = lineFileOpen(tmpName,TRUE);
char* line = NULL;
if (!lineFileNext(lf, &line, NULL))
    errAbort("no difference output, check bigWigCorrelate");
diff = sqlDouble(line);
remove(tmpName);
return  1 - diff;
}


struct slList *slBigWigMerge(const struct slList *item1, const struct slList *item2,
				void *unusedExtraData)
/* Make a new slPair where the name is the children names concattenated and the 
 * value is the average of kids' values.
 * Designed for HAC tree use*/
{
verbose(1,"Merging...\n");
struct bigWig *result;
AllocVar(result);
const struct bigWig *kid1 = (const struct bigWig *)item1;
const struct bigWig *kid2 = (const struct bigWig *)item2;
char tmpName[PATH_LEN];
safef(tmpName, sizeof(tmpName), "%s%p_%p.bedGraph", tmpPrefix, kid1, kid2);
char cmd1[1024];
char cmd2[1024];
safef(cmd1, 1024, "bigWigMerge %s %s %s -verbose=0", kid1->name, kid2->name, tmpName);
char name[PATH_LEN];
safef(name, sizeof(name), "%s%p_%p.bw", tmpPrefix, kid1, kid2);
safef(cmd2, 1024, "bedGraphToBigWig %s chrom.sizes %s", tmpName, name);
mustSystem(cmd1);
mustSystem(cmd2);
remove(tmpName);
result->name = cloneString(name);
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
    hashAdd(itemHash, item->name, item);

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

void bigWigCluster(char *inputList, char* chromSizes, char* output)
/* bigWigCluster - Cluster bigWigs using a hactree. */
{
struct bigWig *list = getBigWigs(inputList);
FILE *f = mustOpen(output,"w");
struct lm *localMem = lmInit(0);
struct hash *distanceHash = hashNew(0);
char *precalcFile = optionVal("precalc", NULL);
if (precalcFile)
    preloadDistances(precalcFile, list, distanceHash);
struct hacTree *clusters;
if (gThreadCount > 1)
    {
    clusters = hacTreeMultiThread(gThreadCount, (struct slList *)list, localMem,
					    slBigWigDistance, slBigWigMerge, NULL, distanceHash);
    }
else
    {
    clusters = hacTreeFromItems((struct slList *)list, localMem,
					    slBigWigDistance, slBigWigMerge, NULL, NULL);
    }
struct slRef *orderedList = getOrderedLeafList(clusters);
colorLeaves(orderedList, distanceHash);
printHierarchicalJson(f, clusters, 20, 20);
// some cleanup
char cmd[1024];
safef(cmd, sizeof(cmd), "rm %s*", tmpPrefix);
mustSystem(cmd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
gThreadCount = optionInt("threads", gThreadCount);
bigWigCluster(argv[1], argv[2], argv[3]);
return 0;
}
