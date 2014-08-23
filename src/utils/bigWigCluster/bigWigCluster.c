/* bigWigCluster - Cluster bigWigs using a hactree. */
#include "sqlNum.h"
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hacTree.h"
#include "rainbow.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigCluster - Cluster bigWigs using a hactree\n"
  "usage:\n"
  "   bigWigCluster input.list chrom.sizes output.json\n"
  "options:\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};
double longest = 0;
int nameCount = 0;



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



static void rPrintHierarchicalJson(FILE *f, struct hacTree *tree, int level, double distance,
		int normConstant, int cgConstant)
/* Recursively prints out the elements of the hierarchical .json file. */
{
struct bigWig *bio = (struct bigWig *)tree->itemOrCluster;
char *tissue = bio->name;
struct rgbColor colors = bio->color;
if (tree->childDistance > longest)
    // the first distance will be the longest, and is used for normalization
    longest = tree->childDistance;
int i;
for (i = 0;  i < level;  i++)
    fputc(' ', f); // correct spacing for .json format
if (tree->left == NULL && tree->right == NULL)
    {
    // Prints out the leaf objects
 //   fprintf(f, "{\"name\": \"%s\",\"similarity\": %f,\"linkGroup\": \" \"", tissue, distance);
    fprintf(f, "{\"%s\"%s \"%s\"%s\"%s\"%s %f %s\"%s\"%s \"rgb(%i,%i,%i)\"}", "name", ":", tissue, ", ",
    		"similarity", ":", distance, "," , "colorGroup", ":", colors.r, colors.g, colors.b);
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");

// Prints out the node object and opens a new children block
fprintf(f, "{\"%s\"%s \"%s\"%s", "name", ":", " ", ",");
fprintf(f, "\"colorGroup\": \"rgb(%i,%i,%i)\",", colors.r, colors.g, colors.b );
fprintf(f, "\"%s\"%s \"%f\"%s\n", "distance", ":", normConstant * (tree->childDistance/longest), ",");
for (i = 0;  i < level + 1;  i++)
    fputc(' ', f);
fprintf(f, "\"%s\"%s\n", "children", ": [");
distance = tree->childDistance/longest; 
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
char cmd[1024];
safef(cmd, 1024, "bigWigCorrelate %s %s > output", kid1->name, kid2->name);
double diff = 0;
mustSystem(cmd);
struct lineFile *lf = lineFileOpen("output",TRUE);
char* line = NULL;
if (!lineFileNext(lf, &line, NULL))
    errAbort("no difference output, check bigWigCorrelate");
diff = sqlDouble(line);
remove("output");
return diff;
}


struct slList *slBigWigMerge(const struct slList *item1, const struct slList *item2,
				void *unusedExtraData)
/* Make a new slPair where the name is the children names concattenated and the 
 * value is the average of kids' values.
 * Designed for HAC tree use*/
{
verbose(1,"Merging...\n");
++nameCount;
struct bigWig *result;
AllocVar(result);
const struct bigWig *kid1 = (const struct bigWig *)item1;
const struct bigWig *kid2 = (const struct bigWig *)item2;
char cmd1[1024];
char cmd2[1024];
safef(cmd1, 1024, "bigWigMerge %s %s output -verbose=0", kid1->name, kid2->name);
char name[1024];
safef(name,1024, "%i", nameCount);
safef(cmd2, 1024, "bedGraphToBigWig output chrom.sizes %s", name);
mustSystem(cmd1);
mustSystem(cmd2);
result->name = name;
return (struct slList *)result;
}

void bigWigCluster(char *inputList, char* chromSizes, char* output)
/* bigWigCluster - Cluster bigWigs using a hactree. */
{
struct bigWig *list = getBigWigs(inputList);
FILE *f = mustOpen(output,"w");
struct lm *localMem = lmInit(0);
struct hacTree *clusters = hacTreeFromItems((struct slList *)list, localMem,
					    slBigWigDistance, slBigWigMerge, NULL, NULL);
printHierarchicalJson(f, clusters, 20, 20);

// some cleanup
int i;
for (i = 0 ; i <= nameCount; ++i)
   {
   char name[1024];
   safef(name,1024, "%i", i);
   remove(name);
   }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bigWigCluster(argv[1], argv[2], argv[3]);
return 0;
}
