/* expData -  Takes in an expression matrix and clusters it using a hierarchical agglomerative clustering algorithm. The output defaults to a hierarchichal .json format, with two additional options. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "memalloc.h"
#include "jksql.h"
#include "expData.h"
#include "jsonWrite.h"
#include "sqlList.h"
#include "hacTree.h"
#include "rainbow.h" 

boolean clCSV = FALSE; // Converts the comma separated matrix into a tab based file. 
int clThreads = 10; // The number of threads to run with the multiThreads option
int clMemLim = 4; // The amount of memeory the program can use, read in Gigabytes. 
float clLongest = 0;  // Used to normalize link distances in rlinkJson.
char* clDescFile = NULL; // The user can provide a description file 
char* clAttributeTable = NULL; // The user can provide an attributes table... this may get removed soon.  
int nodeCount; //The number of nodes. 
int internalNodes = 0; 
void usage()
/* Explain usage and exit. */
{
errAbort(
    "expMatrixToJson -  Takes in an expression matrix and outputs a binary tree clustering the data.\n"
    "			The tree is output as a .json file,  a .html file is generated to view the \n"
    "			tree.  The files are named using the output argument (ex output.json, output.html).\n"
    "usage:\n"
    "   expMatrixToJson [options] matrix output\n"
    "options:\n"
    "    -CSV               The input matrix is in .csv format. \n"
    "    -threads=int       Sets the thread count for the multiThreads option, default is 10 \n"
    "    -memLim=int        Sets the amount of memeory the program can use before aborting. The default is 4G. \n"
    "    -verbose=2         Show basic run stats. \n"
    "    -verbose=3         Show all run stats. Very ugly, avoid at all costs. \n" 
    "    -descFile=string   The user is providing a description file. The description file must provide a \n"
    "                       description for each cell line in the expression matrix. There should be one description per \n" 
    "                       line, starting on the left side of the expression matrix. The description will appear over a \n" 
    "                       leaf node when hovered over.\n"
    );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"CSV", OPTION_BOOLEAN},
    {"threads", OPTION_INT},
    {"memLim", OPTION_INT},
    {"descFile", OPTION_STRING},
    {"attributeTable", OPTION_STRING},
    {NULL, 0},
    };

struct slDoubleInt
/* Used to keep track of the top ten genes contributed */
    {
    struct slDoubleInt *next; 
    double val; 
    int index; 
    };

struct bioExpVector
/* Contains expression information for a biosample on many genes. */
    {
    struct bioExpVector *next;
    char *name;	    // name of biosample.
    char *desc;	    // description of biosample. 
    int count;	    // Number of genes we have data for.
    double *vector;   //  An array allocated dynamically.
    struct rgbColor color;  // Color for this one
    int children;   // Number of bioExpVectors used to build the current 
    struct slDoubleInt *topGeneIndeces; // The indeces for the top 10 genes that drove the clustering up to this point 
    int contGenes; //The number of contributing genes
    int mergeCount; //Number in the merge chain. 
    };


struct slDoubleInt *slDoubleIntNew(double x, int y)
/* Return a new doubleInt */
    {
    struct slDoubleInt *a;
    AllocVar(a);
    a->val = x;
    a->index = y; 
    return a;
    }

int slDoubleIntCmp(const void *va, const void *vb)
/* Compare two doubleInts */
{
    const struct slDoubleInt *a = *((struct slDoubleInt **)va);
    const struct slDoubleInt *b = *((struct slDoubleInt **)vb);
    double diff = a->val - b->val;
    if (diff < 0)
	return -1;
    else if (diff > 0)
	return 1;
    else
	return 0;
    }

double stringToDouble(char *s)
/* Convert string to a double.  Assumes all of string is number
 * and aborts on an error. Errors on 'nan'*/
    {
    char* end;
    double val = strtod(s, &end);

    if (val != val) errAbort("A value of %f was encountered. Please change this value then re run the program.", val); 
    if ((end == s) || (*end != '\0'))
	errAbort("invalid double: %s", s);
    return val;
    }

struct bioExpVector *bioExpVectorListFromFile(char *matrixFile)
/* Read a tab-delimited file and return list of bioExpVectors */
    {
    int vectorSize = 0;
    struct lineFile *lf = lineFileOpen(matrixFile, TRUE);
    char *line, **row = NULL;
    struct bioExpVector *list = NULL, *el;
    while (lineFileNextReal(lf, &line))
	{
	++nodeCount; 
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
	    el->vector[i] = stringToDouble(row[i]);
	el->children = 1;
	el->contGenes = 0;
	slAddHead(&list, el);
	}
    lineFileClose(&lf);
    slReverse(&list);
    return list;
    }

int fillInNames(struct bioExpVector *list, char *nameFile)
/* Fill in name field from file. */
    {
    struct lineFile *lf = lineFileOpen(nameFile, TRUE);
    char *line;
    struct bioExpVector *el = list;
    int maxSize = 0 ; 

    while (lineFileNextReal(lf, &line))
	{
	if (el == NULL)
	    {
	    warn("More names than items in list");
	    break;
	    }
	char *fields[2];
	if (strlen(line) > maxSize) maxSize = strlen(line); 
	int fieldCount = chopTabs(line, fields);
	if (fieldCount >= 1)
	   {
	   el->name = cloneString(fields[0]);
	   if (fieldCount >= 2)
	       el->desc = cloneString(fields[1]);
	   else
	       el->desc = cloneString("0");
	   }
	el = el->next;
	}
    if (el != NULL)
	errAbort("More items in matrix file than %s", nameFile);
    lineFileClose(&lf);
    return maxSize; 
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


static void printJsonNode(FILE *f, struct hacTree *tree, int level, double distance, struct slName *geneNames) 
/* Prints out a json node, NOTE this is not strict .json the downstream script
 * addMetaDataToJson uses the line breaks to ease the .json additions (this lets it be done line by line, rather
 * than unpacking the full .json). */ 
    {
    int i; 
    struct bioExpVector *bio = (struct bioExpVector *)tree->itemOrCluster;
    struct rgbColor colors = bio->color;
    for (i = 0;  i < level + 1;  i++)
	fputc(' ', f);
    distance = tree->childDistance/clLongest;
    double length = 0; 
    if (tree->parent != NULL)
	{
	length = tree->parent->childDistance;
	}
    ++internalNodes; 
    fprintf(f, "{\"name\":\" \", \"number\":\"%i\", \"kids\":\"%i\", \"tpmDistance\":"
			"\"%f\", \"length\": \"%f\",  \"colorGroup\": \"rgb(%i,""%i,%i)\","
			"\"contGenes\":\"%i\",\"geneList\": {",internalNodes,  bio->children,
			tree->childDistance, length, colors.r,colors.g,colors.b, bio->contGenes);
    
    struct slDoubleInt *j;
    for (j = bio->topGeneIndeces; j != NULL; j = j->next)
	{
	struct slName *geneName = slElementFromIx(geneNames, j->index); 
	fprintf(f,"\"%s\":\"%f\"", geneName->name, j->val);  
	if (j->next != NULL) fprintf(f, ", "); 
	
	}
    fprintf(f , "},"); 
    if (distance != distance) distance = 0;
    struct rgbColor wTB; 
    struct rgbColor wTBsqrt; 
    struct rgbColor wTBquad; 
    if (distance == 0) {
	wTB = greyScaleRainbowAtPos(0);
	wTBsqrt = greyScaleRainbowAtPos(0);
	wTBquad = greyScaleRainbowAtPos(0);
	}
    else  {
	wTB = greyScaleRainbowAtPos(distance*.95);
	wTBsqrt = greyScaleRainbowAtPos(sqrt(distance*.95));
	wTBquad = greyScaleRainbowAtPos(sqrt(sqrt(distance*.95)));
	}
    fprintf(f, "\"normalizedDistance\": \"%f\", \"greyScale\":\"rgb(%i,%i,%i)\", \"whiteTo", 
			distance, wTB.r, wTB.g, wTB.b); 
    fprintf(f, "BlackSqrt\":\"rgb(%i,%i,%i)\", \"greyScaleQuad\":\"rgb(%i,%i,%i)\", \"children\":[\n",
			wTBsqrt.r, wTBsqrt.g, wTBsqrt.b, wTBquad.r, wTBquad.g, wTBquad.b);
    }

static void rPrintHierarchicalJson(FILE *f, struct hacTree *tree, int level, double distance, struct slName *geneNames)
/* Recursively prints out the elements of the hierarchical .json file. */
    {
    struct bioExpVector *bio = (struct bioExpVector *)tree->itemOrCluster;
    char *tissue = bio->name;
    struct rgbColor colors = bio->color;
    if (tree->childDistance > clLongest)
	/* In practice the first distance will be the longest, and is used for normalization. */ 
	clLongest = tree->childDistance;
    
    int i;
    for (i = 0;  i < level;  i++)
	fputc(' ', f); // correct spacing for .json format
    
    if (tree->left == NULL && tree->right == NULL) // Check if the current node is a leaf
	{
	fprintf(f, "{\"name\":\"%s\",\"kids\":\"0\",\"length\":\"%f\",\"colorGroup\":\"rgb(%i,%i,%i)\"}",
			    tissue, tree->parent->childDistance, colors.r, colors.g, colors.b); 
	return; 
	}
    // Sanity check leaf nodes, they should not have any kids, definitely should not have a single kid. 
    else if (tree->left == NULL || tree->right == NULL)
	errAbort("\nHow did we get a node with one NULL kid??");
    
    printJsonNode(f, tree, level,  distance, geneNames); 
    
    rPrintHierarchicalJson(f, tree->left, level+1, distance, geneNames);
    fputs(",\n", f);
    rPrintHierarchicalJson(f, tree->right, level+1, distance, geneNames);
    fputc('\n', f);
    // Closes the children block for node objects
    for (i=0;  i < level + 1;  i++)
	fputc(' ', f);
    fputs("]\n", f);
    for (i = 0;  i < level;  i++)
	fputc(' ', f);
    fputs("}", f);
    }

void printHierarchicalJson(FILE *f, struct hacTree *tree, char *geneNamesFile)
/* Prints out the binary tree into .json format intended for d3
 * hierarchical layouts */
    {
    if (tree == NULL)
	{
	fputs("Empty tree.\n", f);
	return;
	}
    double distance = 0;
    struct lineFile *lf = lineFileOpen(geneNamesFile, TRUE);
    char *line;
    struct slName *geneNames;
    AllocVar(geneNames);
    while (lineFileNextReal(lf, &line))
	{
	struct slName *geneName = newSlName(cloneString(line));
	slAddTail(&geneNames, geneName);
	}
    lineFileClose(&lf);
    rPrintHierarchicalJson(f, tree, 0, distance, geneNames);
    }

double slBioExpVectorDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Return the absolute difference between the two kids' values. Weight based on how many nodes have been merged
 * to create the current node.  Designed for HAC tree use*/
    {
    verbose(3,"Calculating Distance...\n");
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

int mergeCount = 0; 

struct slList *slBioExpVectorMerge(const struct slList *item1, const struct slList *item2,
				void *unusedExtraData)
/* Make a new slPair where the name is the children names concattenated and the 
 * value is the average of kids' values.
 * Designed for HAC tree use*/
    {
    verbose(3,"Merging...\n");
    const struct bioExpVector *kid1 = (const struct bioExpVector *)item1;
    const struct bioExpVector *kid2 = (const struct bioExpVector *)item2;
    float kid1Weight = kid1->children / (float)(kid1->children + kid2->children); //Weight based on number of children.
    float kid2Weight = kid2->children / (float)(kid1->children + kid2->children);
    struct bioExpVector *el;
    AllocVar(el);
    AllocArray(el->vector, kid1->count);
    assert(kid1->count == kid2->count);
    el->count = kid1->count; 
    el->name = catTwoStrings(kid1->name, kid2->name);
    int i;
    mergeCount++; 
    int gCount = 0;
    for (i = 0; i < el->count; ++i)
	{
	if (kid1->vector[i] == kid2->vector[i]) // We are doing thousands of merges, lets cut out useless compute where we can. 
	    {
	    el->vector[i] = kid1->vector[i];  
	    continue;    
	    }
	el->vector[i] = (kid1Weight*kid1->vector[i] + kid2Weight*kid2->vector[i]);  
	float diff; 
	if (((float)(kid1->vector[i])) > ((float)(kid2->vector[i]))) diff = ((float)(kid1->vector[i])) - ((float)(kid2->vector[i]));
	else diff = ((float)(kid2->vector[i])) - ((float)(kid1->vector[i]));
	if (diff != 0.0) // Only consider diffs that are non zero
	    {
	    ++el->contGenes; // This gene is non zero so it is contributing 
	    ++gCount;
	    int index = i + 1; 
	    if (gCount <= 10){
		struct slDoubleInt *newGene = slDoubleIntNew(diff, index); 
		slAddHead(&el->topGeneIndeces, newGene); 
		slSort(&el->topGeneIndeces, slDoubleIntCmp); 
		}
	    else{
		if (el->vector[i] > el->topGeneIndeces->val){
		    slPopHead(el->topGeneIndeces); 
		    struct slDoubleInt *newGene = slDoubleIntNew(diff, index); 
		    slAddHead(&el->topGeneIndeces, newGene); 
		    slSort(&el->topGeneIndeces, slDoubleIntCmp); 
		    }
		}
	    }
	}
    slReverse(&el->topGeneIndeces); 
    el->mergeCount = mergeCount; 
    el->children = kid1->children + kid2->children;
    return (struct slList *)(el);
    }


bool slBioExpVectorCmp(const struct slList *item1, const struct slList *item2,
				void *extraData)
/* Compare two bioExpVectors to decide which one becomes the left child and which becomes the right. 
 * Return TRUE if item 1 is left child and item 2 is right child.  */ 
    {
    const struct slList *lastLeaf = extraData;  
    const struct bioExpVector *kid1 = (const struct bioExpVector *)item1;
    const struct bioExpVector *kid2 = (const struct bioExpVector *)item2;
    if (kid1->children !=1 && kid2->children ==1) lastLeaf = item2; 
    if (kid1->children ==1 && kid2->children !=1) lastLeaf = item1;
    
    if (kid1->children > kid2->children)
	{
	return TRUE; // Whoever has more kids gets to be first born 
	}
    else if (kid1->children < kid2->children)return FALSE;
    else{
	if (kid1->mergeCount < kid2->mergeCount) return TRUE;
	else if (kid1->mergeCount > kid2->mergeCount)  return FALSE;
	else{
	    // These are two leaf siblings, things get a bit tricky here... 
	    // First we find the closest neighbor (who should be a firstborn)
	    if (lastLeaf == NULL) 
		{
		// This is the very first merge, not certain how to handle it so lets just return True for now
		lastLeaf = item1; 
		return TRUE; 
		}
	    else{
		double score1 = slBioExpVectorDistance(item1, lastLeaf, NULL);
		double score2 = slBioExpVectorDistance(item2, lastLeaf, NULL);
		if (score1 < score2) 
		    {
		    lastLeaf = item2; 
		    return TRUE; 
		    }
		else{
		    lastLeaf = item1; 
		    return FALSE;
		    }
		}
	    }
	}
    }

void colorLeaves(struct slRef *leafList)
/* Assign colors to leaves. I am very unhappy with the inconsistencies here.  Namely the when you have two leaf siblings
 * they are essentially assigned at random, which makes this metric fairly useless?  Im less than stoked*/
    {
    float total = 0.0;
    struct slRef *el, *nextEl;

    /* Loop through list once to figure out the longest distance since we need to normalize */
    for (el = leafList; el != NULL; el = nextEl)
	{
	nextEl = el->next;
	if (nextEl == NULL)
	    break;
	struct bioExpVector *bio1 = el->val;
	struct bioExpVector *bio2 = nextEl->val;
	double distance = slBioExpVectorDistance((struct slList *)bio1, (struct slList *)bio2, NULL);
	if (distance > total) total = distance; 
	}

    /* Loop through list a second time to generate actual colors. */
    bool firstLine = TRUE; 
    for (el = leafList; el != NULL; el = nextEl)
	{
	nextEl = el->next;
	if (nextEl == NULL)
	    break;
	struct bioExpVector *bio1 = el->val;
	struct bioExpVector *bio2 = nextEl->val;
	double distance = slBioExpVectorDistance((struct slList *)bio1, (struct slList *)bio2, NULL);
	if (firstLine) // Handle the first two nodes, color them both the same 
	    {
	    double normalized = distance/total; 
	    struct rgbColor col = greyScaleRainbowAtPos(normalized); 
	    bio1->color = col; 
	    bio2->color = col; 
	    firstLine = FALSE;
	    continue;
	    }
	double normalized = distance/total; 
	// Color the next node based on distance from previous leaf node
	if (normalized >= .95) bio2->color = greyScaleRainbowAtPos(.95);
	else bio2->color = greyScaleRainbowAtPos(normalized); 
	}
    }

void convertInput(char *expMatrix, char *descFile, bool csv)
/* Takes in a expression matrix and makes the inputs that this program will use. 
 * Namely a transposed table with the first column removed.  Makes use of system calls
 * to use cut, sed, kent utility rowsToCols, and paste (for descFile option). */
    {
    char cmd[1024],cmd1[1024], cmd2[1024];
    if (csv)
	/* A sed one liner will convert comma separated values into a tab separated values*/ 
	{
	char cmd3[1024]; 
	safef(cmd3, 1024, "sed -i 's/,/\\t/g' %s ",expMatrix);  
	verbose(2,"%s\n", cmd3);
	mustSystem(cmd3); 
	}
    safef(cmd, 1024, "cut -f 1 %s | sed \'1d\' > %s.genes", expMatrix, expMatrix); 
    mustSystem(cmd); 
    safef(cmd1, 1024, "cat %s | sed '1d' | rowsToCols stdin %s.transposedMatrix", expMatrix, expMatrix); 
    /* Exp matrices are X axis of cell lines and Y axis of transcripts. This causes long Y axis and short
     * X axis, which are not handled well in C.  The matrix is transposed to get around this issue. */ 
    verbose(2,"%s\n", cmd1);
    mustSystem(cmd1);
    /* Pull out the cell names, and store them in a separate file. This allows the actual data matrix to 
     * have the first row identify the transcript, then all following rows contain only expression values.
     * By removing the name before hand the computation was made faster and easier. */ 
    if (descFile) 
	{
	char cmd3[1024]; 
	safef(cmd2, 1024, "rowsToCols %s stdout | cut -f1 | sed \'1d\' > %s.cellNamesTemp", expMatrix, expMatrix); 
	safef(cmd3, 1024, "paste %s.cellNamesTemp %s > %s.cellNames", expMatrix, descFile, expMatrix);
	verbose(2,"%s\n", cmd2); 
	mustSystem(cmd2);
	verbose(2,"%s\n", cmd3); 
	mustSystem(cmd3);
	}
    else
	{
	safef(cmd2, 1024, "rowsToCols %s stdout | cut -f1 | sed \'1d\' > %s.cellNames", expMatrix, expMatrix);  
	verbose(2,"%s\n", cmd2); 
	mustSystem(cmd2);
	}
    }

void generateHtml(FILE *outputFile, int nameSize, char* jsonFile)
// Generates a new .html file for the dendrogram. Will do some size calculations as well. 
    {
    char *pageName = cloneString(jsonFile);
    chopSuffix(pageName);
    int textSize = 12 - log(nodeCount);  
    int labelLength = 10+nameSize*(15-textSize);
    if (labelLength > 100) labelLength = 100;

    fprintf(outputFile,"<!DOCTYPE html>\n"); 
    fprintf(outputFile,"<head>\n"); 
    fprintf(outputFile,"<title>%s</title>\n", pageName); 
    fprintf(outputFile,"<link rel=\"stylesheet\" href=\"http://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap.min.css\">\n"); 
    fprintf(outputFile,"<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js\"></script>\n"); 
    fprintf(outputFile,"<script src=\"http://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/js/bootstrap.min.js\"></script>\n"); 
    fprintf(outputFile,"<script src=\"http://d3js.org/d3.v3.min.js\" type=\"text/javascript\"></script>\n"); 
    fprintf(outputFile,"<script src=\"/js/d3.dendrograms.js\" type=\"text/javascript\"></script>\n"); 
    fprintf(outputFile,"<div class = \"dropdown\">\n"); 
    fprintf(outputFile,"	<div id = dropdown>\n");  
    fprintf(outputFile,"</div>\n");
    fprintf(outputFile,"<script>\n"); 
    fprintf(outputFile,"	function load() {\n"); 
    fprintf(outputFile,"	var data;\n\n"); 
    fprintf(outputFile,"	d3.json(\"%s\", function(error,json){\n", jsonFile); 
    fprintf(outputFile,"		if (error) return console.warn(error);\n"); 
    fprintf(outputFile,"		data = json;\n"); 
    fprintf(outputFile,"			d3.dendrogram.makeRadialDendrogram('#dendrogram', data,{\n"); 
    fprintf(outputFile,"			});\n"); 
    fprintf(outputFile,"		});\n"); 
    fprintf(outputFile,"  	}\n"); 
    fprintf(outputFile,"</script>\n"); 
    fprintf(outputFile,"<style type=\"text/css\" media=\"screen\">\n"); 
    fprintf(outputFile,"  body { font-family: \"Helvetica Neue\", Helvetica, sans-serif; }\n"); 
    fprintf(outputFile,"  td { vertical-align: top; }\n"); 
    fprintf(outputFile,"</style>\n"); 
    fprintf(outputFile,"</head>\n"); 
    fprintf(outputFile,"<body onload=\"load()\">\n"); 
    fprintf(outputFile,"<table>\n"); 
    fprintf(outputFile,"  <tr>\n"); 
    fprintf(outputFile,"	<td>\n"); 
    fprintf(outputFile,"	  <h1>Dendrogram</h1>\n"); 
    fprintf(outputFile,"	  <div id='dendrogram'></div>\n"); 
    fprintf(outputFile,"	</td>\n"); 
    fprintf(outputFile,"  </tr>\n"); 
    fprintf(outputFile,"</table>\n"); 
    fprintf(outputFile,"</body>\n");
    fprintf(outputFile,"</html>\n"); 

    carefulClose(&outputFile); 
    }



void expData(char *matrixFile, char *outDir, char *descFile)
/* Read matrix and names into a list of bioExpVectors, run hacTree to
 * associate them, and write output. */
    {
    verbose(2,"Start binary clustering of the expression matrix by euclidean distance (expMatrixToJson).\n");
    clock_t begin, end; 
    begin = clock(); 

    convertInput(matrixFile, descFile, clCSV); 
    struct bioExpVector *list = bioExpVectorListFromFile(catTwoStrings(matrixFile,".transposedMatrix"));
    verbose(2,"%lld allocated after bioExpVectorListFromFile\n", (long long)carefulTotalAllocated());
    FILE *f = mustOpen(catTwoStrings(outDir,".json"),"w");
    struct lm *localMem = lmInit(0);
    int size = fillInNames(list, catTwoStrings(matrixFile,".cellNames"));
    /* Allocate new string that is a concatenation of two strings. */
    struct hacTree *clusters = NULL;

    const struct slList *lastLeaf = NULL; 
    verbose(2,"Using %i threads. \n", clThreads);  
    clusters = hacTreeMultiThread(clThreads, (struct slList *)list, localMem, slBioExpVectorDistance,
				slBioExpVectorMerge, slBioExpVectorCmp,(void *)lastLeaf, NULL);

    struct slRef *orderedList = getOrderedLeafList(clusters);
    colorLeaves(orderedList);
    printHierarchicalJson(f, clusters, catTwoStrings(matrixFile, ".genes"));
    FILE *htmlF = mustOpen(catTwoStrings(outDir,".html"),"w");
    generateHtml(htmlF,size,catTwoStrings(outDir,".json")); 

    // Remove temporary files
    // These temporary files are awful sloppy... Long term goal to get rid of them. 
    char cleanup[1024], cleanup2[1024], cleanup3[1024];
    safef(cleanup, 1024, "rm %s.cellNames", matrixFile); 
    safef(cleanup2, 1024, "rm %s.transposedMatrix", matrixFile); 
    safef(cleanup3, 1024, "rm %s.genes", matrixFile); 
    mustSystem(cleanup);
    mustSystem(cleanup2);
    mustSystem(cleanup3);

    end = clock(); 
    verbose(2,"%lld allocated at end. The program took %f seconds to complete.\n", (long long)carefulTotalAllocated(), (double)(end-begin)/CLOCKS_PER_SEC);
    verbose(2,"Completed binary clustering of the expression matrix by euclidean distance (expMatrixToJson).\n");
    }

int main(int argc, char *argv[])
/* Process command line. */
    {
    optionInit(&argc, argv, options);
    clCSV = optionExists("CSV");
    clThreads = optionInt("threads", clThreads);
    clMemLim = optionInt("memLim", clMemLim); 
    clDescFile = optionVal("descFile", clDescFile);
    if (argc != 3)
	usage();
    pushCarefulMemHandler(1L*1024*1024*1024*clMemLim);
    expData(argv[1], argv[2], clDescFile);
    return 0;
    }
