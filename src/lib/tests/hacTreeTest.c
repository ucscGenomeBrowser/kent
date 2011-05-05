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
  "hacTreeTest - Read in haplotypes from a file and print the resulting clusters.\n"
  "usage:\n"
  "   hacTreeTest in.txt out.txt\n"
  "All lines of in.txt must have the same length.\n"
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
					    slDoubleDistance, slDoubleMidpoint, NULL, NULL);
fputs("Clustering by numeric value:\n", f);
printSlDoubleTree(f, clusters);
}


//-------------------------------------------------------------------
// Center-weighted alpha clustering of haplotypes -- see Redmine #3711, #2823 note 7

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
fprintf(f, "(%s:%f:\n", contents, tree->childDistance);
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

struct cwaExtraData
/* Helper data for hacTree clustering of haplotypes by center-weighted alpha distance */
{
    int center;    // index from which each point's contribution to distance is to be weighted
    int len;       // total length of haplotype strings
    double alpha;  // weighting factor for distance from center
    struct lm *localMem;
};

static double cwaDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Center-weighted alpha sequence distance function for hacTree clustering of haplotype seqs */
// This is inner-loop so I am not doing defensive checks.  Caller must ensure:
// 1. kids's sequences' lengths are both equal to helper->len
// 2. 0 <= helper->center <= len
// 3. 0.0 < helper->alpha <= 1.0
{
const struct slName *kid1 = (const struct slName *)item1;
const struct slName *kid2 = (const struct slName *)item2;
const char *hap1 = kid1->name;
const char *hap2 = kid2->name;
struct cwaExtraData *helper = extraData;
double distance = 0;
double weight = 1; // start at center: alpha to the 0th power
int i;
for (i=helper->center;  i >= 0;  i--)
    {
    if (hap1[i] != hap2[i])
	distance += weight;
    weight *= helper->alpha;
    }
weight = helper->alpha; // start at center+1: alpha to the 1st power
for (i=helper->center+1;  i < helper->len;  i++)
    {
    if (hap1[i] != hap2[i])
	distance += weight;
    weight *= helper->alpha;
    }
return distance;
}

static struct slList *cwaMerge(const struct slList *item1, const struct slList *item2,
			       void *extraData)
/* Make a consensus haplotype from two input haplotypes, for hacTree clustering by
 * center-weighted alpha distance. */
// This is inner-loop so I am not doing defensive checks.  Caller must ensure that
// kids's sequences' lengths are both equal to helper->len.
{
const struct slName *kid1 = (const struct slName *)item1;
const struct slName *kid2 = (const struct slName *)item2;
const char *hap1 = kid1->name;
const char *hap2 = kid2->name;
struct cwaExtraData *helper = extraData;
struct slName *consensus = lmSlName(helper->localMem, (char *)hap1);
int i;
for (i=0; i < helper->len;  i++)
    if (hap1[i] != hap2[i])
	consensus->name[i] = 'N';
return (struct slList *)consensus;
}

static int cwaCmp(const struct slList *item1, const struct slList *item2, void *extraData)
/* Use strcmp on haplotype strings. */
{
const struct slName *kid1 = (const struct slName *)item1;
const struct slName *kid2 = (const struct slName *)item2;
return strcmp(kid1->name, kid2->name);
}

void hacTreeTestHaplos(char *inFileName, FILE *f, struct lm *localMem)
/* Read in haplotypes of same length from a file and print the resulting clusters. */
{
struct slName *sln, *slnList = NULL;
struct lineFile *lf = lineFileOpen(inFileName, TRUE);
int len = 0;
char *line;
int size;
while (lineFileNext(lf, &line, &size))
    {
    if (len == 0)
	len = size-1;
    else if (size-1 != len)
	lineFileAbort(lf, "All lines in input file must have same length (got %d vs. %d)",
		      size-1, len);
    sln = slNameNewN(line, size);
    slAddHead(&slnList, sln);
    }
lineFileClose(&lf);
slReverse(&slnList);
int center = len / 2;
double alpha = 0.5;
struct cwaExtraData helper = { center, len, alpha, localMem };
struct hacTree *clusters = hacTreeFromItems((struct slList *)slnList, localMem,
					    cwaDistance, cwaMerge, cwaCmp, &helper);
fputs("Clustering by haplotype similarity:\n", f);
printSlNameTree(f, clusters);
carefulClose(&f);
}

void hacTreeTest(char *inFileName, char *outFileName)
/* Read in items from a file and print the resulting clusters. */
{
FILE *f = mustOpen(outFileName, "w");
struct lm *localMem = lmInit(0);
hacTreeTestInts(f, localMem);
hacTreeTestHaplos(inFileName, f, localMem);
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
