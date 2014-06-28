/* expData -  Takes in a relational database and outputs expression information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "expData.h"
#include "sqlList.h"
#include "hacTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "expData -  Takes in a relational database and outputs expression information\n"
  "usage:\n"
  "   expData biosamples matrix output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


struct bioExpVector
/* Contains expression information for a biosample on many genes */
    {
    struct bioExpVector *next;
    char *name;	    // name of biosample
    int count;	    // Number of genes we have data for
    double *vector;   //  An array allocated dynamically
    };

struct bioExpVector *bioExpVectorListFromFile(char *matrixFile)
// Read a tab-delimited file and return list of bioExpVector.

{
int vectorSize = 0;
struct lineFile *lf = lineFileOpen(matrixFile, TRUE);
char *line, **row = NULL;
struct bioExpVector *list = NULL, *el;
while (lineFileNextReal(lf, &line))
    {
    if (vectorSize == 0)
        {
	// Detect first row
	vectorSize = chopByWhite(line, NULL, 0);  // counting up
	AllocArray(row, vectorSize);
	continue;
	}
    AllocVar(el);
    AllocArray(el->vector, vectorSize);
    el->count = chopByWhite(line, row, vectorSize);
    assert(el->count == vectorSize);
    int i;
    for (i = 0; i < el->count; ++i)
	el->vector[i] = sqlDouble(row[i]);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void fillInNames(struct bioExpVector *list, char *nameFile)
/* Fill in name field from file */
{
struct lineFile *lf = lineFileOpen(nameFile, TRUE);
char *line;
struct bioExpVector *el = list;
while (lineFileNextReal(lf, &line))
     {
     if (el == NULL)
	 {
         warn("More names than items in list");
	 break;
	 }
     el->name = cloneString(line);
     el = el->next;
     }
lineFileClose(&lf);
}


static void rPrintSlBioExpVectorTree(FILE *f, struct hacTree *tree, int level)
/* Recursively print out cluster as nested-parens with {}'s around leaf nodes. */
{
char *tissue = ((struct bioExpVector *)(tree->itemOrCluster))->name;
int i;
for (i = 0;  i < level;  i++)
    fputc(' ', f);
if (tree->left == NULL && tree->right == NULL)
    {
    fprintf(f, "{%s}", tissue);
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");
fprintf(f, "(%s\n", tissue);
rPrintSlBioExpVectorTree(f, tree->left, level+1);
fputs(",\n", f);
rPrintSlBioExpVectorTree(f, tree->right, level+1);
fputc('\n', f);
for (i=0;  i < level;  i++)
    fputc(' ', f);
fputs(")", f);
}

void printSlBioExpVectorTree(FILE *f, struct hacTree *tree)
/* Print out cluster as nested-parens with {}'s around leaf nodes. */
{
if (tree == NULL)
    {
    fputs("Empty tree.\n", f);
    return;
    }
rPrintSlBioExpVectorTree(f, tree, 0);
fputc('\n', f);
}

char* floatToString(float input)
{
char* result = needMem(sizeof(result));
sprintf(result,"%f", input);
return result;
freez(result);
}

double slBioExpVectorDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Return the absolute difference between the two kids' values. */
{
const struct bioExpVector *kid1 = (const struct bioExpVector *)item1;
const struct bioExpVector *kid2 = (const struct bioExpVector *)item2;
int j;
double diff = 0, sum = 0;
for (j = 0; j < kid1->count; ++j)
    {
    diff = kid1->vector[j] - kid2->vector[j];
    sum += (diff * diff);
    }
return sqrt(sum);
}


struct slList *slBioExpVectorMerge(const struct slList *item1, const struct slList *item2,
				void *unusedExtraData)
/* Make a new slPair where the name is the both kids names and the 
 * value is the average of kids' values.
 * Designed for HAC tree use*/
{
const struct bioExpVector *kid1 = (const struct bioExpVector *)item1;
const struct bioExpVector *kid2 = (const struct bioExpVector *)item2;
struct bioExpVector *el;
AllocVar(el);
AllocArray(el->vector, kid1->count);
el->count = kid1->count; 
el->name = catTwoStrings(kid1->name, kid2->name);
int i;
for (i = 0; i < el->count; ++i)
    {
    el->vector[i] = (kid1->vector[i] + kid2->vector[i])/2;
    }
return (struct slList *)(el);
}

void expData(char *matrixFile, char *nameFile, char *outFile)
/* Read matrix and names into a list of bioExpVectors, run hacTree to
 * associate them, and write output. */
{
struct bioExpVector *list = bioExpVectorListFromFile(matrixFile);
FILE *f = mustOpen(outFile,"w");
struct lm *localMem = lmInit(0);
fillInNames(list, nameFile);
struct hacTree *clusters = hacTreeFromItems((struct slList *)list, localMem,
					    slBioExpVectorDistance, slBioExpVectorMerge, NULL, NULL);
printSlBioExpVectorTree(f,clusters);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
expData(argv[1], argv[2], argv[3]);
return 0;
}
