/* hacTreeTest - Read in items from a file and print the resulting clusters. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "sqlNum.h"
#include "hacTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hacTreeTest - Read in lines from a file and print the resulting clusters by length.\n"
  "usage:\n"
  "   hacTreeTest in.txt out.txt\n"
  "Actually, before clustering in.txt, it clusters some numbers because that is easier.\n"
  "\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

//------------------- numeric example ------------------

static void rPrintSlDoubleTree(FILE *f, struct hacTree *tree, int level)
/* Recursively print out cluster as nested-parens with {}'s around leaf nodes. */
{
double val = ((struct slDouble *)(tree->itemOrCluster))->val;
int i;
for (i=0;  i < level;  i++)
    fputc(' ', f);
if (tree->left == NULL && tree->right == NULL)
    {
    fprintf(f, "{%0.3lf}", val);
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");
fprintf(f, "(%0.3lf:%0.3lf:\n", val, tree->childDistance);
rPrintSlDoubleTree(f, tree->left, level+1);
fputs(",\n", f);
rPrintSlDoubleTree(f, tree->right, level+1);
fputc('\n', f);
for (i=0;  i < level;  i++)
    fputc(' ', f);
fputs(")", f);
}

void printSlDoubleTree(FILE *f, struct hacTree *tree)
/* Print out cluster as nested-parens with {}'s around leaf nodes. */
{
if (tree == NULL)
    {
    fputs("Empty tree.\n", f);
    return;
    }
rPrintSlDoubleTree(f, tree, 0);
fputc('\n', f);
}

double slDoubleDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Return the absolute difference between the two kids' values. */
{
const struct slDouble *kid1 = (const struct slDouble *)item1;
const struct slDouble *kid2 = (const struct slDouble *)item2;
double diff = kid1->val - kid2->val;
if (diff < 0)
    diff = -diff;
return diff;
}

struct slList *slDoubleMidpoint(const struct slList *item1, const struct slList *item2,
				void *unusedExtraData)
/* Make a new slDouble that is the midpoint/average of kids' values. */
{
const struct slDouble *kid1 = (const struct slDouble *)item1;
const struct slDouble *kid2 = (const struct slDouble *)item2;
double midpoint = (kid1->val + kid2->val) / 2.0;
return (struct slList *)(slDoubleNew(midpoint));
}

void hacTreeTestInts(FILE *f, struct lm *localMem)
/* Cluster a list of integers (use doubles to get more precise cluster points). */
{
struct slDouble *sldList = NULL;
slAddHead(&sldList, slDoubleNew(0));
slAddHead(&sldList, slDoubleNew(5));
slAddHead(&sldList, slDoubleNew(7));
slAddHead(&sldList, slDoubleNew(1));
slAddHead(&sldList, slDoubleNew(-8));
slAddHead(&sldList, slDoubleNew(12));
slAddHead(&sldList, slDoubleNew(6));
slAddHead(&sldList, slDoubleNew(-6));
slAddHead(&sldList, slDoubleNew(-3));
slAddHead(&sldList, slDoubleNew(8));
struct hacTree *clusters = hacTreeFromItems((struct slList *)sldList, localMem,
					    slDoubleDistance, slDoubleMidpoint, NULL);
fputs("Clustering by numeric value:\n", f);
printSlDoubleTree(f, clusters);
}


//------------------- strlen(line) example ------------------

static void rPrintSlNameTree(FILE *f, struct hacTree *tree, int level)
/* Recursively print out cluster as nested-parens with {}'s around leaf nodes. */
{
char *contents = ((struct slName *)(tree->itemOrCluster))->name;
int i;
for (i=0;  i < level;  i++)
    fputc(' ', f);
if (tree->left == NULL && tree->right == NULL)
    {
    fprintf(f, "{%s}", contents);
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");
fprintf(f, "(%s:%d:\n", contents, (int)(tree->childDistance));
rPrintSlNameTree(f, tree->left, level+1);
fputs(",\n", f);
rPrintSlNameTree(f, tree->right, level+1);
fputc('\n', f);
for (i=0;  i < level;  i++)
    fputc(' ', f);
fputs(")", f);
}

void printSlNameTree(FILE *f, struct hacTree *tree)
/* Print out cluster as nested-parens with {}'s around leaf nodes. */
{
if (tree == NULL)
    {
    fputs("Empty tree.\n", f);
    return;
    }
rPrintSlNameTree(f, tree, 0);
fputc('\n', f);
}

double strlenDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Return the absolute difference in length between the two kids. */
{
const struct slName *kid1 = (const struct slName *)item1;
const struct slName *kid2 = (const struct slName *)item2;
int diff = strlen(kid1->name) - strlen(kid2->name);
if (diff < 0)
    diff = -diff;
return (double)diff;
}

struct slList *mergeLines(const struct slList *item1, const struct slList *item2,
			  void *unusedExtraData)
/* Make a string that is the average length of the kids. */
{
const struct slName *kid1 = (const struct slName *)item1;
const struct slName *kid2 = (const struct slName *)item2;
int len1 = strlen(kid1->name);
int len2 = strlen(kid2->name);
int avgLen = (len1 + len2 + 1) / 2;
const char *longerName = (len1 >= len2) ? kid1->name : kid2->name;
return (struct slList *)slNameNewN((char *)longerName, avgLen);
}

void hacTreeTestStrlens(char *inFileName, FILE *f, struct lm *localMem)
/* Read in items from a file and print the resulting clusters. */
{
struct slName *sln, *slnList = NULL;
struct lineFile *lf = lineFileOpen(inFileName, TRUE);
char *line;
int size;
while (lineFileNext(lf, &line, &size))
    {
    sln = slNameNewN(line, size);
    slAddHead(&slnList, sln);
    }
lineFileClose(&lf);
slReverse(&slnList);
struct hacTree *clusters = hacTreeFromItems((struct slList *)slnList, localMem,
					    strlenDistance, mergeLines, NULL);
fputs("Clustering by string length:\n", f);
printSlNameTree(f, clusters);
carefulClose(&f);
}

void hacTreeTest(char *inFileName, char *outFileName)
/* Read in items from a file and print the resulting clusters. */
{
FILE *f = mustOpen(outFileName, "w");
struct lm *localMem = lmInit(0);
hacTreeTestInts(f, localMem);
hacTreeTestStrlens(inFileName, f, localMem);
lmCleanup(&localMem);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hacTreeTest(argv[1], argv[2]);
return 0;
}
