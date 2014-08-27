/* expData -  Takes in GNF expression data, organizes it using a hierarchical agglomerative clustering algorithm. The output defaults to a hierarchichal .json format, with two additional options. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "expData.h"
#include "sqlList.h"
#include "hacTree.h"
#include "rainbow.h" 

/* Visually the nodes will be assigned a color corresponding to a range of sizes. 
 * This constant dictates how many colors will be displayed (1-20).  */
int clCgConstant = 20;
/* A normalizing constant for the distances, this corresponds to 
 * the size of the nodes in the standard output and the link distance in the forceLayout output*/
int clNormConstant = 20; 
boolean clForceLayout = FALSE; // Prints the data in .json format for d3 force layout visualizations
int target = 0;  // Used for the target value in rlinkJson.
float longest = 0;  // Used to normalize link distances in rlinkJson.

void usage()
/* Explain usage and exit. */
{
errAbort(
  "expData -  Takes in a relational database and outputs expression information.\n"
  "Standard output is .json format intended for d3 hierarchical displays. \n"
  "usage:\n"
  "   expData matrix biosamples output\n"
  "options:\n"
  "   -forceLayout = bool Prints the output in .json format for d3 forceLayouts\n"
  "   -normConstant = int Normalizing constant for d3, default is 20. For forceLayout 100 is reccomended \n"
  "   -cgConstant = int Defines the number of possible colors for nodes, 1 - 20 \n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"normConstant", OPTION_INT},
   {"cgConstant", OPTION_INT},
   {"structuredOutput", OPTION_BOOLEAN},
   {"forceLayout", OPTION_BOOLEAN},
   {NULL, 0},
};

struct jsonNodeLine
/* Stores the information for a single json node line */
    {
    struct jsonNodeLine *next;
    char* name;		// the source for a given link
    double distance;	// the distance for a given link
    };

struct jsonLinkLine
/* Stores the information for a single json link line */
    {
    struct jsonLinkLine *next;
    int source;		// the source for a given link
    int target;		// the target for a given link
    double distance;	// the distance for a given link
    };

struct bioExpVector
/* Contains expression information for a biosample on many genes. */
    {
    struct bioExpVector *next;
    char *name;	    // name of biosample.
    int count;	    // Number of genes we have data for.
    double *vector;   //  An array allocated dynamically.
    struct rgbColor color;  // Color for this one
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
//    assert(el->count == vectorSize);
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

void printJsonNodeLine(FILE *f, struct jsonNodeLine *node)
{
fprintf(f,"    %s\"%s\"%s\"%s\"%s", "{","name", ":", node->name, ",");
fprintf(f,"\"%s\"%s%0.31f%s\n", "y", ":" , node->distance, "},");
}

void printEndJsonNodeLine(FILE *f, struct jsonNodeLine *node)
{
fprintf(f,"    %s\"%s\"%s\"%s\"%s", "{","name", ":", node->name, ",");
fprintf(f,"\"%s\"%s%0.31f%s\n", "y", ":" , node->distance, "}");
}

void printJsonLinkLine(FILE *f, struct jsonLinkLine *links)
/* Prints out a single json link line */
{
fprintf(f,"    %s\"%s\"%s%d%s", "{","source", ":", links->source, ",");
fprintf(f,"\"%s\"%s%d%s", "target", ":" , links->target, ",");  
fprintf(f,"\"%s\"%s%0.31f%s\n", "distance", ":" , links->distance , "},");  
}

void printEndJsonLinkLine(FILE *f, struct jsonLinkLine *links)
/* Prints out a single json link line */
{
fprintf(f,"    %s\"%s\"%s%d%s", "{","source", ":", links->source, ",");
fprintf(f,"\"%s\"%s%d%s", "target", ":" , links->target, ",");  
fprintf(f,"\"%s\"%s%0.31f%s\n", "distance", ":" , links->distance , "}");  
}

void rPrintNodes(FILE *f, struct hacTree *tree, struct jsonNodeLine *nodes)
// Recursively adds the node information to a linked list
{
char *tissue = ((struct bioExpVector *)(tree->itemOrCluster))->name;
if (tree->childDistance != 0) 
    // If the current object is a node then we assign no name.
    {
    struct jsonNodeLine *nNode;
    AllocVar(nNode);
    nNode->name = " ";
    nNode->distance = tree->childDistance;
    slAddHead(nodes, nNode); // add the node
    }
else {
    // Otherwise the current object is a leaf, and the tissue name is printed. 
    struct jsonNodeLine *lNode;
    AllocVar(lNode);
    lNode->name = tissue;
    lNode->distance = tree->childDistance;
    slAddHead(nodes, lNode); // add the node
    }

if (tree->left == NULL && tree->right == NULL)
    // Stop at the last element
    { 
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");
rPrintNodes(f, tree->left, nodes);
rPrintNodes(f, tree->right, nodes);
}


void rPrintLinks(int normConstant, FILE *f, struct hacTree *tree, int source, struct jsonLinkLine *links)
// Recursively loads the link information into a linked list
{
if (tree->childDistance > longest)
    // the first distance will be the longest, and is used for normalization
    longest = tree->childDistance;
if (tree->left == NULL && tree->right == NULL)
    // Stop at the last element
    { 
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");

// Left recursion.
struct jsonLinkLine *lLink;
AllocVar(lLink);
++target;
lLink->source = target - 1;	 // The source and target are always ofset by 1. 
lLink->target = target;
lLink->distance = normConstant*(tree->childDistance/longest);	//Calculates the link distance
source = target;		// Prepares the source for the right recursion. 
slAddHead(links, lLink); 	// Add the link
rPrintLinks(normConstant,f, tree->left, source, links);

// Right recursion.
struct jsonLinkLine *rLink;
AllocVar(rLink);
++target;
rLink->source = source - 1;	// The source is dependent on the last target of the left recursion
rLink->target = target;	
rLink->distance = normConstant*(tree->childDistance/longest);
slAddHead(links, rLink);		// Add the link
rPrintLinks(normConstant,f, tree->right, ++source, links);
}

void printForceLayoutJson(int normConstant, FILE *f, struct hacTree *tree)
// Prints the hacTree into a .json file format for d3 forceLayout visualizations.
{
// Basic json template for d3 visualizations
fprintf(f,"%s\n", "{");
fprintf(f,"  \"%s\"%s\n", "nodes", ":[" );

// Print the nodes
struct jsonNodeLine *nodes;
AllocVar(nodes);
rPrintNodes(f, tree, nodes);
slReverse(&nodes);
int nodeCount = slCount(nodes);
int j;
for (j = 0; j < nodeCount -1; ++j)
   // iterate through the linked nodelist printing nodes
   {
   if (j == nodeCount - 2)
       printEndJsonNodeLine(f, nodes);
   else
       {
       printJsonNodeLine(f, nodes);
       nodes = nodes -> next;
       }
   }
fprintf(f,  "%s\n", "],");
fprintf(f,  "\"%s\"%s\n", "links", ":[" );

// Print the links
struct jsonLinkLine *links;
AllocVar(links);
rPrintLinks(normConstant,f,tree, 0, links);
slReverse(&links);
int linkCount = slCount(links);
int i;
for (i = 0; i< linkCount -1; ++i)
   // iterate through the linked linklist printing links
   {
   if (i == linkCount - 2)
       printEndJsonLinkLine(f, links);
   else
       {
       printJsonLinkLine(f, links);
       links = links -> next;
       }
   }
fprintf(f,"  %s\n", "]");
fprintf(f,"%s\n", "}");
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
struct bioExpVector *bio = (struct bioExpVector *)tree->itemOrCluster;
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


double slBioExpVectorDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Return the absolute difference between the two kids' values. 
 * Designed for HAC tree use*/
{
verbose(1,"Calculating Distance...\n");
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
verbose(1,"Merging...\n");
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

void colorLeaves(struct slRef *leafList)
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
   struct bioExpVector *bio1 = el->val;
   struct bioExpVector *bio2 = nextEl->val;
   double distance = slBioExpVectorDistance((struct slList *)bio1, (struct slList *)bio2, NULL);
   total += distance;
   }

/* Loop through list a second time to generate actual colors. */
double soFar = 0;
for (el = leafList; el != NULL; el = nextEl)
   {
   nextEl = el->next;
   if (nextEl == NULL)
       break;
   struct bioExpVector *bio1 = el->val;
   struct bioExpVector *bio2 = nextEl->val;
   double distance = slBioExpVectorDistance((struct slList *)bio1, (struct slList *)bio2, NULL);
   soFar += distance;
   double normalized = soFar/total;
   bio2->color = saturatedRainbowAtPos(normalized * purplePos);
   }

/* Set first color to correspond to 0, since not set in above loop */
struct bioExpVector *bio = leafList->val;
bio->color = saturatedRainbowAtPos(0);
}

void expData(char *matrixFile, char *nameFile, char *outFile, bool forceLayout, int normConstant, int cgConstant)
/* Read matrix and names into a list of bioExpVectors, run hacTree to
 * associate them, and write output. */
{
struct bioExpVector *list = bioExpVectorListFromFile(matrixFile);
FILE *f = mustOpen(outFile,"w");
struct lm *localMem = lmInit(0);
fillInNames(list, nameFile);
//struct hacTree *clusters = hacTreeForCostlyMerges((struct slList *)list, localMem,
//					    slBioExpVectorDistance, slBioExpVectorMerge, NULL);
struct hacTree *clusters = hacTreeFromItems((struct slList *)list, localMem,
					    slBioExpVectorDistance, slBioExpVectorMerge, NULL, NULL);
struct slRef *orderedList = getOrderedLeafList(clusters);
colorLeaves(orderedList);
if (forceLayout)
    printForceLayoutJson(normConstant,f,clusters);
if (!clForceLayout)
    printHierarchicalJson(f, clusters, normConstant, cgConstant);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clForceLayout = optionExists("forceLayout");
clNormConstant = optionInt("normConstant", clNormConstant);
clCgConstant = optionInt("cgConstant", clCgConstant);
if (argc != 4)
    usage();
expData(argv[1], argv[2], argv[3], clForceLayout, clNormConstant, clCgConstant);
return 0;
}
