/* expData -  Takes in a relational database and outputs expression information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "expData.h"
#include "sqlList.h"
#include "hacTree.h"
#include "jsonWrite.h"

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

int target = 0;  // Used for the target value in rlinkJson.
float longest = 0;  // Used to normalize link distances in rlinkJson.

struct bioExpVector
/* Contains expression information for a biosample on many genes. */
    {
    struct bioExpVector *next;
    char *name;	    // name of biosample.
    int count;	    // Number of genes we have data for.
    double *vector;   //  An array allocated dynamically.
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
	// Detect first row.
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
/* Fill in name field from file. */
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

void rPrintNodes(FILE *f, struct hacTree *tree)
{
// Recursively prints out the nodes in a depth first order starting on the left
char *tissue = ((struct bioExpVector *)(tree->itemOrCluster))->name;
struct jsonWrite *jw = jsonWriteNew();
if (tree->childDistance != 0) 
    // If the current object is a node then we assign no name.
    {
    fprintf(f,"    %s\"%s\"%s\"%s\"%s", "{","name", ":", " ", ",");
    jsonWriteString(jw, "name", " ");
    jsonWriteNumber(jw, "y", tree->childDistance);
    fprintf(f,"\"%s\"%s%0.31f%s\n", "y", ":" , tree->childDistance, "},");
    }
else {
    // Otherwise the current object is a leaf, and the tissue name is printed. 
    jsonWriteString(jw, "name", tissue);
    jsonWriteNumber(jw, "y", tree->childDistance);
    fprintf(f,"    %s\"%s\"%s\"%s\"%s", "{","name", ":", tissue, ",");
    fprintf(f,"\"%s\"%s%0.31f%s\n", "y", ":" , tree->childDistance, "},");
    }
if (tree->left == NULL && tree->right == NULL)
    { 
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");
rPrintNodes(f, tree->left);
rPrintNodes(f, tree->right);
}


void rPrintLinks(FILE *f, struct hacTree *tree, int source)
{
// Recursively prints the links in json format. 
if (tree->childDistance > longest)
    // the first distance will be the longest, and is used for normalization
    longest = tree->childDistance;
/* if the current location is a leaf */
if (tree->left == NULL && tree->right == NULL)
    { 
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");
/* check for the end of the tree */

/* Left recursion; the source and target are always ofset by 1. */
++target;
fprintf(f,"    %s\"%s\"%s%d%s", "{","source", ":", target - 1, ",");
fprintf(f,"\"%s\"%s%d%s", "target", ":" , target, ",");  
fprintf(f,"\"%s\"%s%0.31f%s\n", "distance", ":" , 100*(tree->childDistance/longest) , "},");  
/* Prepares the source for the right recursion. */
source = target;
rPrintLinks(f, tree->left, source);
/* Right recursion. */
++target;
fprintf(f,"    %s\"%s\"%s%d%s", "{","source", ":", source - 1 , ",");
fprintf(f,"\"%s\"%s%d%s", "target", ":" , target, ",");  
fprintf(f,"\"%s\"%s%0.31f%s\n", "distance", ":" , 100*(tree->childDistance/longest) , "},");  
rPrintLinks(f, tree->right, ++source);
}

void printJson(FILE *f, struct hacTree *tree)
/* Prints the hacTree into a Json file format */
{
int source = 0;
// Basic json template for d3 visualizations
//struct jsonWrite *jw = jsonWriteNew();
fprintf(f,"%s\n", "{");
//jsonWriteTag(struct jsonWrite *jw, char *var);
fprintf(f,"  \"%s\"%s\n", "nodes", ":[" );
rPrintNodes(f, tree);
fprintf(f,  "%s\n", "],");
// Basic json template for d3 visualizations
fprintf(f,  "\"%s\"%s\n", "links", ":[" );
rPrintLinks(f,tree, source);
fprintf(f,"  %s\n", "]");

fprintf(f,"%s\n", "}");
}


static void rPrintSlBioExpVectorTree(FILE *f, struct hacTree *tree, int level,double distance)
/* Recursively print out cluster as nested-parens with {}'s around leaf nodes. */
{
char *tissue = ((struct bioExpVector *)(tree->itemOrCluster))->name;
int i;
for (i = 0;  i < level;  i++)
    fputc(' ', f);
if (tree->left == NULL && tree->right == NULL)
    {
    fprintf(f, "{%s}%0.31f", tissue, distance);
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");
fprintf(f, "(%s%f\n", "node", tree->childDistance);
distance += tree->childDistance;
rPrintSlBioExpVectorTree(f, tree->left, level+1, distance);
fputs(",\n", f);
rPrintSlBioExpVectorTree(f, tree->right, level+1, distance);
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
double distance = 0;
rPrintSlBioExpVectorTree(f, tree, 0, distance);
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
/* Return the absolute difference between the two kids' values. 
 * Designed for HAC tree use*/
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
/* Make a new slPair where the name is the children names concattenated and the 
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
printJson(f,clusters);
//printSlBioExpVectorTree(f,clusters);
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
