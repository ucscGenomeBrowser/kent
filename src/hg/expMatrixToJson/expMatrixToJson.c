/* expData -  Takes in an expression matrix and clusters it using a hierarchical agglomerative clustering algorithm. The output defaults to a hierarchichal .json format, with two additional options. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "memalloc.h"
#include "jksql.h"
#include "expData.h"
#include "sqlList.h"
#include "hacTree.h"
#include "rainbow.h" 

boolean clCSV = FALSE; // Converts the comma separated matrix into a tab based file. 
boolean clMultiThreads = FALSE; // Allows the user to run the program with multiple threads, default is off. 
int clThreads = 10; // The number of threads to run with the multiThreads option
int clMemLim = 4; // The amount of memeory the program can use, read in Gigabytes. 
float clLongest = 0;  // Used to normalize link distances in rlinkJson.
char* clDescFile = NULL; // The user can provide a description file 
char* clAttributeTable = NULL; // The user can provide an attributes table... this may get removed soon.  
int nodeCount; //The number of nodes. 

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
    "    -multiThreads      The program will run on multiple threads. \n"
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
   {"multiThreads", OPTION_BOOLEAN},
   {"CSV", OPTION_BOOLEAN},
   {"threads", OPTION_INT},
   {"memLim", OPTION_INT},
   {"descFile", OPTION_STRING},
   {"attributeTable", OPTION_STRING},
   {NULL, 0},
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
    };

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

static void rPrintHierarchicalJson(FILE *f, struct hacTree *tree, int level, double distance)
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

if (tree->left == NULL && tree->right == NULL)
    // Print the leaf nodes
    {
    if (bio->desc)
	fprintf(f, "{\"name\":\"%s\",\"length\":%f,\"distance\":\"%s\",\"colorGroup\":\"rgb(%i,%i,%i)\"}", tissue, (tree->parent->childDistance*tree->parent->childDistance)  /1000, bio->desc, colors.r, colors.g, colors.b); 
    else
	fprintf(f, "{\"name\":\"%s\",\"length\":%f,\"distance\":\"%s\",\"colorGroup\":\"rgb(%i,%i,%i)\"}", tissue, tree->parent->childDistance, " ", colors.r, colors.g, colors.b); 
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");

// Prints out the node object and opens a new children block.
for (i = 0;  i < level + 1;  i++)
    fputc(' ', f);
distance = tree->childDistance/clLongest;
double temp = 0; 
if (tree->parent != NULL)
    {
    temp = tree->parent->childDistance;
    }
fprintf(f, "{\"name\": \" \", \"longestDistance\":\"%f\", \"tpmDistance\": \"%f\", \"length\": %f,  \"colorGroup\": \"rgb(%i,"
	    "%i,%i)\",",clLongest,tree->childDistance, temp*temp/1000, colors.r,colors.g,colors.b);
if (distance != distance) distance = 0;
//fprintf(f, "\"%s\"%s \"%f\"%s\n", "distance", ":", 100*distance, ",");
struct rgbColor wTB; 
struct rgbColor wTBsqrt; 
struct rgbColor wTBquad; 
if (distance == 0) {
    wTB = whiteToBlackRainbowAtPos(0);
    wTBsqrt = whiteToBlackRainbowAtPos(0);
    wTBquad = whiteToBlackRainbowAtPos(0);
    }
else  {
    wTB = whiteToBlackRainbowAtPos(distance*.95);
    wTBsqrt = whiteToBlackRainbowAtPos(sqrt(distance*.95));
    wTBquad = whiteToBlackRainbowAtPos(sqrt(sqrt(distance*.95)));
    }
fprintf(f, "\"distance\": \"%f\", \"whiteToBlack\":\"rgb(%i,%i,%i)\", \"whiteTo", distance, wTB.r, wTB.g, wTB.b); 
fprintf(f, "BlackSqrt\":\"rgb(%i,%i,%i)\", \"whiteToBlackQuad\":\"rgb(%i,%i,%i)\",\n", wTBsqrt.r, wTBsqrt.g, wTBsqrt.b, wTBquad.r, wTBquad.g, wTBquad.b);
for (i = 0;  i < level + 1;  i++)
    fputc(' ', f);
fprintf(f, "\"children\":[\n");
rPrintHierarchicalJson(f, tree->left, level+1, distance);
fputs(",\n", f);
rPrintHierarchicalJson(f, tree->right, level+1, distance);
fputc('\n', f);
// Closes the children block for node objects
for (i=0;  i < level + 1;  i++)
    fputc(' ', f);
fputs("]\n", f);
for (i = 0;  i < level;  i++)
    fputc(' ', f);
fputs("}", f);
}

void printHierarchicalJson(FILE *f, struct hacTree *tree)
/* Prints out the binary tree into .json format intended for d3
 * hierarchical layouts */
{
if (tree == NULL)
    {
    fputs("Empty tree.\n", f);
    return;
    }
double distance = 0;
rPrintHierarchicalJson(f, tree, 0, distance);
fputc('\n', f);
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


struct slList *slBioExpVectorMerge(const struct slList *item1, const struct slList *item2,
				void *unusedExtraData)
/* Make a new slPair where the name is the children names concattenated and the 
 * value is the average of kids' values.
 * Designed for HAC tree use*/
{
verbose(3,"Merging...\n");
const struct bioExpVector *kid1 = (const struct bioExpVector *)item1;
const struct bioExpVector *kid2 = (const struct bioExpVector *)item2;
float kid1Weight = kid1->children / (float)(kid1->children + kid2->children);
float kid2Weight = kid2->children / (float)(kid1->children + kid2->children);
struct bioExpVector *el;
AllocVar(el);
AllocArray(el->vector, kid1->count);
assert(kid1->count == kid2->count);
el->count = kid1->count; 
el->name = catTwoStrings(kid1->name, kid2->name);
int i;
for (i = 0; i < el->count; ++i)
    {
    el->vector[i] = (kid1Weight*kid1->vector[i] + kid2Weight*kid2->vector[i]);
    }
el->children = kid1->children + kid2->children; 
return (struct slList *)(el);
}

void colorLeaves(struct slRef *leafList)
/* Assign colors of rainbow to leaves. */
{
float total = 0.0;
//double purplePos = 0.80;
struct slRef *el, *nextEl;

/* Loop through list once to figure out total, since we need to normalize */
for (el = leafList; el != NULL; el = nextEl)
    {
    nextEl = el->next;
    if (nextEl == NULL)
	break;
    struct bioExpVector *bio1 = el->val;
    struct bioExpVector *bio2 = nextEl->val;
    double distance = slBioExpVectorDistance((struct slList *)bio1, (struct slList *)bio2, NULL);
    if (distance != distance ) distance = 0;
    total += distance;
    }

if (total == 0) errAbort("There doesn't seem to be any difference between these matrix columns. Aborting."); 
double soFar = 0;
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
    if (firstLine) 
	{
	double normalized = distance/total; 
	bio1->color = whiteToBlackRainbowAtPos(normalized); 
	firstLine = FALSE;
	}
    //if (distance != distance ) distance = 0 ;
    //soFar += distance;
    //double normalized = soFar/total;
    double normalized = distance/total; 
    if (normalized * 100 >= .95) bio2->color = whiteToBlackRainbowAtPos(.95);
    else bio2->color = whiteToBlackRainbowAtPos(normalized*100); 
    //bio2->color = saturatedRainbowAtPos(distance);
    soFar += normalized;     
    }
/* Set first color to correspond to 0, since not set in above loop */
//struct bioExpVector *bio = leafList->val;
//bio->color = saturatedRainbowAtPos(0);
//bio->color = whiteToBlackRainbowAtPos(0.0);
}

void convertInput(char *expMatrix, char *descFile, bool csv)
/* Takes in a expression matrix and makes the inputs that this program will use. 
 * Namely a transposed table with the first column removed.  Makes use of system calls
 * to use cut, sed, kent utility rowsToCols, and paste (for descFile option). */
{
char cmd1[1024], cmd2[1024];
if (csv)
    /* A sed one liner will convert comma separated values into a tab separated values*/ 
    {
    char cmd3[1024]; 
    safef(cmd3, 1024, "sed -i 's/,/\\t/g' %s ",expMatrix);  
    verbose(2,"%s\n", cmd3);
    mustSystem(cmd3); 
    }

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
int radius = 540 + 270*log10(nodeCount);  
int labelLength = 10+nameSize*(15-textSize);
if (labelLength > 100) labelLength = 100;


fprintf(outputFile,"<!DOCTYPE html>\n");
fprintf(outputFile,"<meta charset=\"utf-8\">\n");
fprintf(outputFile,"<title>%s</title>\n", pageName); 
// CSS styles
fprintf(outputFile,"<style>\n");
fprintf(outputFile,"\t.node circle{fill: #ff f; stroke: steelblue; stroke-width: .25px;}\n");
fprintf(outputFile,"\t.node{font: %ipx sans-serif;}\n", textSize);
fprintf(outputFile,"\t.link{fill: none; stroke: #ccc; st roke-width: 1.5px;}\n");
fprintf(outputFile,"\t.selectedLink{fill: none; stroke: #ccc; stroke-width: 3.0px;}\n");
fprintf(outputFile,"\t.selected{fill: red;}\n");
fprintf(outputFile,"\t.legend{font-size: 12px;}\n");
fprintf(outputFile,"\trect{stroke-width: 2;}\n");
fprintf(outputFile,"</style>\n");
// Start body
fprintf(outputFile,"<body>\n"); 
// Scripts
fprintf(outputFile,"<link rel=\"stylesheet\" href=\"http://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap.min.css\">\n");
fprintf(outputFile,"<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js\"></script>\n");
fprintf(outputFile,"<script src=\"http://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/js/bootstrap.min.js\"></script>\n");
fprintf(outputFile,"<script src=\"http://d3js.org/d3.v3.min.js\"></script>\n");
// Adding dropdowns...
fprintf(outputFile,"<div class = \"dropdown\">\n\t<ul style=\"list-style-type:none;display:inline-flex\">\n"); 
// Inner nodes
fprintf(outputFile,"\t\t<li class=\"dropdown\">\n"); 
fprintf(outputFile,"\t\t\t<button class=\"btn btn-default dropdown-toggle\" type=\"button\" data-toggle=\"dropdown\"> Inner nodes\n"); 
fprintf(outputFile,"\t\t\t<span class=\"caret\"</span></button>\n"); 
fprintf(outputFile,"\t\t\t<ul class=\"dropdown-menu\">\n"); 
fprintf(outputFile,"\t\t\t\t<li><button onclick=\"nodeColors(0)\">tpm distance</button></li>\n"); 
fprintf(outputFile,"\t\t\t\t<li><button onclick=\"nodeColors(1)\">sqrt tpm distance</button></li>\n"); 
fprintf(outputFile,"\t\t\t\t<li><button onclick=\"nodeColors(2)\">quad rt tpm distance</button></li>\n"); 
fprintf(outputFile,"\t\t\t</ul>\n\t\t</li>\n");
// Inner leaves
fprintf(outputFile,"\t\t<li class=\"dropdown\">\n"); 
fprintf(outputFile,"\t\t\t<button class=\"btn btn-default dropdown-toggle\" type=\"button\" data-toggle=\"dropdown\"> Inner leaves\n"); 
fprintf(outputFile,"\t\t\t<span class=\"caret\"</span></button>\n"); 
fprintf(outputFile,"\t\t\t<ul class=\"dropdown-menu\">\n"); 
fprintf(outputFile,"<! Identifier  >\n"); 
fprintf(outputFile,"\t\t\t</ul>\n\t\t</li>\n");
// Middle leaves
fprintf(outputFile,"\t\t<li class=\"dropdown\">\n"); 
fprintf(outputFile,"\t\t\t<button class=\"btn btn-default dropdown-toggle\" type=\"button\" data-toggle=\"dropdown\"> Middle leaves\n"); 
fprintf(outputFile,"\t\t\t<span class=\"caret\"</span></button>\n"); 
fprintf(outputFile,"\t\t\t<ul class=\"dropdown-menu\">\n"); 
fprintf(outputFile,"<! Identifier  >\n"); 
fprintf(outputFile,"\t\t\t</ul>\n\t\t</li>\n");
// Outer leaves
fprintf(outputFile,"\t\t<li class=\"dropdown\">\n"); 
fprintf(outputFile,"\t\t\t<button class=\"btn btn-default dropdown-toggle\" type=\"button\" data-toggle=\"dropdown\"> Outer leaves\n"); 
fprintf(outputFile,"\t\t\t<span class=\"caret\"</span></button>\n"); 
fprintf(outputFile,"\t\t\t<ul class=\"dropdown-menu\">\n"); 
fprintf(outputFile,"<! Identifier  >\n"); 
fprintf(outputFile,"\t\t\t</ul>\n\t\t</li>\n");


fprintf(outputFile,"\t</ul>\n</div>\n"); 
fprintf(outputFile,"<div id=\"graph\"></div>\n");
// Div the graph
fprintf(outputFile,"<script>\n");
// Global variables and stuffs
fprintf(outputFile,"\tvar radius= %i / 2;\n", radius); 
fprintf(outputFile,"\tvar cluster=d3.layout.cluster().size([360, radius - %i]);\n", labelLength); 
fprintf(outputFile,"\tvar colors=d3.scale.category20();\n"); 
fprintf(outputFile,"\tvar diagonal=d3.svg.diagonal.radial().projection(function(d){return [d.y, d.x / 180 * Math.PI];});\n"); 
fprintf(outputFile,"\tvar svg=d3.select(\"#graph\") .append(\"svg\") .attr(\"width\", 150 + (radius * 2) )\n");  
fprintf(outputFile,"\t\t.attr(\"height\", radius * 2) .append(\"g\") .attr(\"transform\",\n"); 
fprintf(outputFile,"\t\t\"translate(\" + (radius + 150) + \",\" + radius + \")\");\n"); 
fprintf(outputFile,"\tvar legendRectSize=20;\n");
fprintf(outputFile,"\tvar legendSpacing=4;\n");
fprintf(outputFile,"\tvar currentLegend=-1;\n");
// Start D3 processing
fprintf(outputFile,"\td3.json(\"%s\", function(error, root){\n", jsonFile); 
// Variable nodes
fprintf(outputFile,"\t\tvar nodes=cluster.nodes(root);\n\n"); 
// Variable link
fprintf(outputFile,"\t\tvar link=svg.selectAll(\"path.link\")\n"); 
fprintf(outputFile,"\t\t\t.data(cluster.links(nodes)).enter().append(\"path\")\n"); 
fprintf(outputFile,"\t\t\t.attr(\"class\", \"link\").on(\"click\", function(){\n");
fprintf(outputFile,"\t\t\td3.select(\".selectedLink\").classed(\"selectedLink\", false);\n");
fprintf(outputFile,"\t\t\td3.select(this).classed(\"selectedLink\", true);}).attr(\"d\", diagonal);\n\n");
// Graph variables!!! These are parsed by D3 and used to generate the actual dendrogram
// Variable internalNodes
fprintf(outputFile,"\t\tvar internalNodes = svg.selectAll(\"g.internalNode\")\n");
fprintf(outputFile,"\t\t\t.data(nodes).enter().append(\"g\")\n"); 
fprintf(outputFile,"\t\t\t.attr(\"class\", \"internalNode\").attr(\"transform\",\n");
fprintf(outputFile,"\t\t\tfunction(d){\n"); 
fprintf(outputFile,"\t\t\t\treturn \"rotate(\" + (d.x - 90) + \")translate(\" + d.y + \")\";});\n"); 
fprintf(outputFile,"\t\t\t\tinternalNodes.append(\"circle\").attr(\"r\", function(d){\n"); 
fprintf(outputFile,"\t\t\t\tif (d.name==\" \")\n");
fprintf(outputFile,"\t\t\t\t{return 5;}\n"); //d.kids;}\n"); 
fprintf(outputFile,"\t\t\t\telse{return 0;}})\n"); 
fprintf(outputFile,"\t\t\t\t.style(\"fill\", function(d){\n");
fprintf(outputFile,"\t\t\t\tif (d.name==\" \")\n"); 
fprintf(outputFile,"\t\t\t\t{return d.whiteToBlack;}\n"); 
fprintf(outputFile,"\t\t\t\telse{return \"none\";}})\n");
fprintf(outputFile,"\t\t\t\t.style(\"stroke\", \"steelblue\")\n"); 
fprintf(outputFile,"\t\t\t\t.style(\"stroke-width\", \"1.25px\");\n\n"); 
// Variable innerLeaves
fprintf(outputFile,"\t\tvar innerLeaves = svg.selectAll(\"g.innerLeaf\")\n");
fprintf(outputFile,"\t\t\t.data(nodes).enter().append(\"g\")\n"); 
fprintf(outputFile,"\t\t\t.attr(\"class\", \"innerLeaf\").attr(\"transform\",\n");
fprintf(outputFile,"\t\t\tfunction(d){\n"); 
fprintf(outputFile,"\t\t\t\treturn \"rotate(\" + (d.x - 90) + \")translate(\" + d.y + \")\";});\n"); 
fprintf(outputFile,"\t\t\t\tinnerLeaves.append(\"circle\").attr(\"r\", function(d){\n"); 
fprintf(outputFile,"\t\t\t\tif (d.name !==\" \")\n");
fprintf(outputFile,"\t\t\t\t{return 5;}})\n"); 
fprintf(outputFile,"\t\t\t\t.style(\"fill\", function(d){\n");
fprintf(outputFile,"\t\t\t\tif (d.name !==\" \")\n"); 
fprintf(outputFile,"\t\t\t\t{return \"d.whiteToBlack\";}\n"); 
fprintf(outputFile,"\t\t\t\t})\n\n");
// Variable middleLeaves
fprintf(outputFile,"\t\tvar middleLeaves = svg.selectAll(\"g.middleLeaf\")\n");
fprintf(outputFile,"\t\t\t.data(nodes).enter().append(\"g\")\n"); 
fprintf(outputFile,"\t\t\t.attr(\"class\", \"middleLeaf\").attr(\"transform\",\n");
fprintf(outputFile,"\t\t\tfunction(d){\n"); 
fprintf(outputFile,"\t\t\t\treturn \"rotate(\" + (d.x - 90) + \")translate(\" + d.y + \")\";});\n"); 
fprintf(outputFile,"\t\t\t\tmiddleLeaves.append(\"circle\").attr(\"r\", 5).style(\"fill\",\"none\");\n\n"); 
// Variable outerLeaves
fprintf(outputFile,"\t\tvar outerLeaves = svg.selectAll(\"g.outerLeaf\")\n");
fprintf(outputFile,"\t\t\t.data(nodes).enter().append(\"g\")\n"); 
fprintf(outputFile,"\t\t\t.attr(\"class\", \"outerLeaf\").attr(\"transform\",\n");
fprintf(outputFile,"\t\t\tfunction(d){\n"); 
fprintf(outputFile,"\t\t\t\treturn \"rotate(\" + (d.x - 90) + \")translate(\" + d.y + \")\";});\n"); 
fprintf(outputFile,"\t\t\t\touterLeaves.append(\"circle\").attr(\"r\", 5).style(\"fill\",\"none\");\n\n"); 
// Legend variables!!! These are parsed by D3 and used to generate the legends
// Variable innerLegend
fprintf(outputFile,"\t\t\tvar innerLegend=svg.selectAll('.innerLegend') .data(colors.domain()) .enter()\n"); 
fprintf(outputFile,"\t\t\t.append('g') .attr('class', 'innerLegend')\n"); 
fprintf(outputFile,"\t\t\t.attr('transform', \n"); 
fprintf(outputFile,"\t\t\tfunction (d, i){\n"); 
fprintf(outputFile,"\t\t\t\tvar height=legendRectSize + legendSpacing;\n"); 
fprintf(outputFile,"\t\t\t\tvar offset=height * colors.domain().length / 2;\n"); 
fprintf(outputFile,"\t\t\t\tvar horz=-2 * legendRectSize;\n"); 
fprintf(outputFile,"\t\t\t\tvar vert=i * height - offset;\n"); 
fprintf(outputFile,"\t\t\t\treturn 'translate(' + horz + ',' + vert + ')';}\n"); 
fprintf(outputFile,"\t\t\t);\n");
fprintf(outputFile,"\t\t\tinnerLegend.append('rect') .attr('width', legendRectSize) .attr('height', legendRectSize)\n"); 
fprintf(outputFile,"\t\t\t.style('fill', colors) .style('stroke', colors);\n"); 
fprintf(outputFile,"\t\t\tinnerLegend.append('text') .attr('x', legendRectSize + legendSpacing)\n");
fprintf(outputFile,"\t\t\t.attr('y', legendRectSize - legendSpacing) .text(function (d){return d;});\n\n"); 
// Variable middleLegend
fprintf(outputFile,"\t\t\tvar middleLegend=svg.selectAll('.middleLegend') .data(colors.domain()) .enter()\n"); 
fprintf(outputFile,"\t\t\t.append('g') .attr('class', 'middlerLegend')\n"); 
fprintf(outputFile,"\t\t\t.attr('transform', \n"); 
fprintf(outputFile,"\t\t\tfunction (d, i){\n"); 
fprintf(outputFile,"\t\t\t\tvar height=legendRectSize + legendSpacing;\n"); 
fprintf(outputFile,"\t\t\t\tvar offset=height * colors.domain().length / 2;\n"); 
fprintf(outputFile,"\t\t\t\tvar horz=-2 * legendRectSize;\n"); 
fprintf(outputFile,"\t\t\t\tvar vert=i * height - offset;\n"); 
fprintf(outputFile,"\t\t\t\treturn 'translate(' + horz + ',' + vert + ')';}\n"); 
fprintf(outputFile,"\t\t\t);\n");
fprintf(outputFile,"\t\t\tmiddleLegend.append('rect') .attr('width', legendRectSize) .attr('height', legendRectSize)\n"); 
fprintf(outputFile,"\t\t\t.style('fill', colors) .style('stroke', colors);\n"); 
fprintf(outputFile,"\t\t\tmiddleLegend.append('text') .attr('x', legendRectSize + legendSpacing)\n");
fprintf(outputFile,"\t\t\t.attr('y', legendRectSize - legendSpacing) .text(function (d){return d;});\n\n"); 
// Variable outerLegend
fprintf(outputFile,"\t\t\tvar outerLegend=svg.selectAll('.outerLegend') .data(colors.domain()) .enter()\n"); 
fprintf(outputFile,"\t\t\t.append('g') .attr('class', 'outerLegend')\n"); 
fprintf(outputFile,"\t\t\t.attr('transform', \n"); 
fprintf(outputFile,"\t\t\tfunction (d, i){\n"); 
fprintf(outputFile,"\t\t\t\tvar height=legendRectSize + legendSpacing;\n"); 
fprintf(outputFile,"\t\t\t\tvar offset=height * colors.domain().length / 2;\n"); 
fprintf(outputFile,"\t\t\t\tvar horz=-2 * legendRectSize;\n"); 
fprintf(outputFile,"\t\t\t\tvar vert=i * height - offset;\n"); 
fprintf(outputFile,"\t\t\t\treturn 'translate(' + horz + ',' + vert + ')';}\n"); 
fprintf(outputFile,"\t\t\t);\n");
fprintf(outputFile,"\t\t\touterLegend.append('rect') .attr('width', legendRectSize) .attr('height', legendRectSize)\n"); 
fprintf(outputFile,"\t\t\t.style('fill', colors) .style('stroke', colors);\n"); 
fprintf(outputFile,"\t\t\touterLegend.append('text') .attr('x', legendRectSize + legendSpacing)\n");
fprintf(outputFile,"\t\t\t.attr('y', legendRectSize - legendSpacing) .text(function (d){return d;});\n\n"); 
fprintf(outputFile,"\t\t\t});\n");
// Function updateLegend
fprintf(outputFile,"\tfunction updateLegend(val, layer, shift){\n"); 
fprintf(outputFile,"\t\tvar legend=svg.selectAll('g.'+layer+'legend').data(val);\n"); 
fprintf(outputFile,"\t\tlegend.enter() .append('g') .attr('class', 'legend') .attr('transform', function (d, i){\n");
fprintf(outputFile,"\t\t\tvar height=legendRectSize + legendSpacing;\n"); 
fprintf(outputFile,"\t\t\tvar offset=height *colors.domain().length / 2;\n"); 
fprintf(outputFile,"\t\t\tvar horz=(-2 * legendRectSize) - radius - 75;\n");
fprintf(outputFile,"\t\t\tvar vert=-(i * height - offset);\n"); 
fprintf(outputFile,"\t\t\treturn 'translate(' + horz + ',' + vert + ')';});\n"); 
fprintf(outputFile,"\t\tlegend.append('rect') .attr('width', legendRectSize) .attr('height', legendRectSize)\n");
fprintf(outputFile,"\t\t.style('fill', colors) .style('stroke', colors);\n"); 
fprintf(outputFile,"\t\tlegend.append('text') .attr('x', legendRectSize + legendSpacing)\n"); 
fprintf(outputFile,"\t\t.attr('y', legendRectSize - legendSpacing) .text(function (d){return d;});}\n\n");
// Function leafColors
fprintf(outputFile,"\tfunction leafColors(val, layer, shift){\n"); 
fprintf(outputFile,"\t\tvar node = svg.selectAll(\"g.\" + layer + \"Leaf\")\n"); 
fprintf(outputFile,"\t\t.selectAll(\"circle\").style(\"fill\", function(d){\n"); 
fprintf(outputFile,"\t\t\tif (d.name !== \" \"){\n"); 
fprintf(outputFile,"\t\t\t\tif (val ===0) {return d3.rgb(d.colorGroup);}\n"); 
fprintf(outputFile,"\t\t /* Identifier */\n"); 
fprintf(outputFile,"\t\t\t}\n"); 
fprintf(outputFile,"\t\t\t\telse{return 'none'}});\n"); 
fprintf(outputFile,"\t\tvar legend=svg.selectAll('g.' + layer + 'Legend').remove();\n"); 
fprintf(outputFile,"\t\tif (val >= 0) updateLegend(colors.domain(), layer);\n"); 
fprintf(outputFile,"\t\tcolors=d3.scale.category20();}\n\n"); 
// Function nodeColors
fprintf(outputFile,"\tfunction nodeColors(val){\n"); 
fprintf(outputFile,"\t\tvar node=svg.selectAll(\"g.internalNode\").selectAll(\"circle\")\n"); 
fprintf(outputFile,"\t\t.style(\"fill\",function(d){\n"); 
fprintf(outputFile,"\t\t\tif (d.name === \" \"){\n"); 
fprintf(outputFile,"\t\t\t\tif (val==-1) return d3.rgb(d.colorGroup);\n"); 
fprintf(outputFile,"\t\t\t\tif (val==0) return d3.rgb(d.whiteToBlack);\n"); 
fprintf(outputFile,"\t\t\t\tif (val==1) return d3.rgb(d.whiteToBlackSqrt);\n"); 
fprintf(outputFile,"\t\t\t\tif (val==2) return d3.rgb(d.whiteToBlackQuad);\n"); 
fprintf(outputFile,"}});\n"); 
fprintf(outputFile,"\t\t}\n"); 
fprintf(outputFile,"</script>\n"); 
fprintf(outputFile,"</body>"); 

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

if (clMultiThreads)
    {
    verbose(2,"Using %i threads. \n", clThreads);  
    clusters = hacTreeMultiThread(clThreads, (struct slList *)list, localMem,
  					    slBioExpVectorDistance, slBioExpVectorMerge, NULL, NULL);
    }
else
    {
    verbose(2,"Using 1 threads. \n");  
    clusters = hacTreeFromItems((struct slList *)list, localMem,
						slBioExpVectorDistance, slBioExpVectorMerge, NULL, NULL);
    }

struct slRef *orderedList = getOrderedLeafList(clusters);
colorLeaves(orderedList);
printHierarchicalJson(f, clusters);
FILE *htmlF = mustOpen(catTwoStrings(outDir,".html"),"w");
generateHtml(htmlF,size,catTwoStrings(outDir,".json")); 

// Remove temporary files
char cleanup[1024], cleanup2[1024];
safef(cleanup, 1024, "rm %s.cellNames", matrixFile); 
safef(cleanup2, 1024, "rm %s.transposedMatrix", matrixFile); 
mustSystem(cleanup);
mustSystem(cleanup2);
end = clock(); 
verbose(2,"%lld allocated at end. The program took %f seconds to complete.\n", (long long)carefulTotalAllocated(), (double)(end-begin)/CLOCKS_PER_SEC);

verbose(2,"Completed binary clustering of the expression matrix by euclidean distance (expMatrixToJson).\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clCSV = optionExists("CSV");
clMultiThreads = optionExists("multiThreads");
clThreads = optionInt("threads", clThreads);
clMemLim = optionInt("memLim", clMemLim); 
clDescFile = optionVal("descFile", clDescFile);
if (argc != 3)
    usage();
pushCarefulMemHandler(1L*1024*1024*1024*clMemLim);
expData(argv[1], argv[2], clDescFile);
return 0;
}
