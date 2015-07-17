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

boolean clForceLayout = FALSE; // Prints the data in .json format for d3 force layout visualizations
boolean clCSV = FALSE; // Converts the comma separated matrix into a tab based file. 
boolean clMultiThreads = FALSE; // Allows the user to run the program with multiple threads, default is off. 
int clThreads = 10; // The number of threads to run with the multiThreads option
int clMemLim = 4; // The amount of memeory the program can use, read in Gigabytes. 
int target = 0;  // Used for the target value in rlinkJson.
float longest = 0;  // Used to normalize link distances in rlinkJson.
char* clDescFile = NULL; // The user can provide a description file 

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
    "    -multiThread    The program will run on multiple threads. \n"
    "    -forceLayout    Prints the output in .json format for d3 forceLayouts. NOTE no .html file will be \n"
    "                    generated using this option.\n"
    "    -CSV    The input matrix is in .csv format. \n"
    "    -threads=int    Sets the thread count for the multiThreads option, default is 10 \n"
    "    -memLim=int    Sets the amount of memeory the program can use before aborting. The default is 4G. \n"
    "    -descFile=string    The user is providing a description file. The description file must provide a \n"
    "                  description for each cell line in the expression matrix. There should be one description per \n" 
    "                  line, starting on the left side of the expression matrix. The description will appear over a \n" 
    "                  leaf node when hovered over.\n"
    "    -verbose=2    Show basic run stats. \n"
    "    -verbose=3    Show all run stats. \n" 
    );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"multiThread", OPTION_BOOLEAN},
   {"forceLayout", OPTION_BOOLEAN},
   {"CSV", OPTION_BOOLEAN},
   {"threads", OPTION_INT},
   {"memLim", OPTION_INT},
   {"descFile", OPTION_STRING},
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
    char *desc;	    // description of biosample. 
    int count;	    // Number of genes we have data for.
    double *vector;   //  An array allocated dynamically.
    struct rgbColor color;  // Color for this one
    int children;   // Number of bioExpVectors used to build the current 
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


void rPrintLinks(FILE *f, struct hacTree *tree, int source, struct jsonLinkLine *links)
// Recursively loadslist->children = 0 ;  the link information into a linked list
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
lLink->distance = 100*(tree->childDistance/longest);	//Calculates the link distance
source = target;		// Prepares the source for the right recursion. 
slAddHead(links, lLink); 	// Add the link
rPrintLinks(f, tree->left, source, links);

// Right recursion.
struct jsonLinkLine *rLink;
AllocVar(rLink);
++target;
rLink->source = source - 1;	// The source is dependent on the last target of the left recursion
rLink->target = target;	
rLink->distance = 100*(tree->childDistance/longest);
slAddHead(links, rLink);		// Add the link
rPrintLinks(f, tree->right, ++source, links);
}

void printForceLayoutJson(FILE *f, struct hacTree *tree)
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
rPrintLinks(f,tree, 0, links);
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

static void rPrintHierarchicalJson(FILE *f, struct hacTree *tree, int level, double distance)
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
    // Print the leaf nodes
    {
    if (bio->desc)
	fprintf(f, "{\"name\":\"%s\",\"distance\":\"%s\",\"colorGroup\":\"rgb(%i,%i,%i)\"}", tissue, bio->desc, colors.r, colors.g, colors.b); 
    else
	fprintf(f, "{\"name\":\"%s\",\"distance\":\"%s\",\"colorGroup\":\"rgb(%i,%i,%i)\"}", tissue, " ", colors.r, colors.g, colors.b); 
    return;
    }
else if (tree->left == NULL || tree->right == NULL)
    errAbort("\nHow did we get a node with one NULL kid??");

// Prints out the node object and opens a new children block
fprintf(f, "{\"%s\"%s \"%s\"%s", "name", ":", " ", ",");
fprintf(f, "\"colorGroup\": \"rgb(%i,%i,%i)\",", colors.r, colors.g, colors.b );
distance = tree->childDistance/longest; 
if (distance != distance) distance = 0;
fprintf(f, "\"%s\"%s \"%f\"%s\n", "distance", ":", 100*distance, ",");
for (i = 0;  i < level + 1;  i++)
    fputc(' ', f);
fprintf(f, "\"%s\"%s\n", "children", ": [");
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
float kid1Weight = kid1->children / (float)(kid1->children + kid2->children);
float kid2Weight = kid2->children / (float)(kid1->children + kid2->children);
for (j = 0; j < kid1->count; ++j)
    {
    diff = (kid1Weight*kid1->vector[j]) - (kid2Weight*kid2->vector[j]);
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
/* Loop through list once to figure out total, since we need to
 * normalize */
float total = 0.0;
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
   if (distance != distance ) distance = 0;
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
   if (distance != distance ) distance = 0 ;
   soFar += distance;
   double normalized = soFar/total;
   bio2->color = saturatedRainbowAtPos(normalized * purplePos);
   }

/* Set first color to correspond to 0, since not set in above loop */
struct bioExpVector *bio = leafList->val;
bio->color = saturatedRainbowAtPos(0);
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
// Generates a new .html file for the dendrogram. 
{
char *pageName = cloneString(jsonFile);
chopSuffix(pageName);
fprintf(outputFile, "<!DOCTYPE html> <meta charset=\"utf-8\"> <title> %s Radial Dendrogram</title> <style>  .node circle {   fill: #fff;   stroke: steelblue;   stroke-width: 1.5px; }  .node {   font: 10px sans-serif; }  .link {   fill: none;   stroke: #ccc;	stroke-width: 1.5px; }  .selectedLink{   fill: none;   stroke: #ccc;   stroke-width: 3.0px; }  .selected{   fill: red; }  </style> <body> <script src=\"http://d3js.org/d3.v3.min.js\"></script> <script> var color = d3.scale.category20();  var radius = 1080 / 2;  var cluster = d3.layout.cluster()     .size([360, radius - %i]) ;  var diagonal = d3.svg.diagonal.radial()     .projection(function(d) { return [d.y, d.x / 180 * Math.PI]; });  var svg = d3.select(\"body\").append(\"svg\")     .attr(\"width\", radius * 2)     .attr(\"height\", radius * 2)   .append(\"g\")     .attr(\"transform\", \"translate(\" + radius + \",\" + radius + \")\");  d3.json(\"%s\", function(error, root)", pageName, 10+nameSize*5, jsonFile);
fprintf(outputFile, "{   var nodes = cluster.nodes(root);    var link = svg.selectAll(\"path.link\")       .data(cluster.links(nodes))     .enter().append(\"path\")       .attr(\"class\", \"link\")       .on(\"click\", function() {               d3.select(\".selectedLink\").classed(\"selectedLink\", false);               d3.select(this).classed(\"selectedLink\",true);       })       .attr(\"d\", diagonal);   var node = svg.selectAll(\"g.node\")       .data(nodes)       .enter().append(\"g\")       .attr(\"class\", \"node\")       .attr(\"transform\", function(d) { return \"rotate(\" + (d.x - 90) + \")translate(\" + d.y + \")\"; })        .on(\"click\", function() {               d3.select(\".selected\").classed(\"selected\", false);               d3.select(this).classed(\"selected\",true);       })       .on(\"mouseover\", function(d) {           var g = d3.select(this);           var info = g.append('text')               .classed('info', true)              .attr('x', 20)              .attr('y', 10)               .attr(\"transform\", function(d) { return \"rotate(\"+ (90 - d.x) +\")\";  })              .text(d.distance);       })       .on(\"mouseout\", function() {                   d3.select(this).select('text.info').remove();          });      node.append(\"circle\")    .attr(\"r\", function (d) {        if (d.name != \" \")       {         return 5;       }       })       .style(\"fill\", function (d) {          if (d.name != \" \") {           return d3.rgb(d.colorGroup);         }         });        node.append(\"circle\")       .attr(\"r\", function(d) {            return d.distance/5;})               .on(\"click\", function() {               d3.select(\".selected\").classed(\"selected\", false);               d3.select(this).classed(\"selected\",true);       })              .style(\"fill\",  \"white\");      node.append(\"text\")       .attr(\"dy\", \".55em\")       .attr(\"text-anchor\", function(d) { return d.x < 180 ? \"start\" : \"end\"; })       .attr(\"transform\", function(d) { return d.x < 180 ? \"translate(8)\" : \"rotate(180)translate(-8)\"; })       .text(function(d) { return d.name; }); });  d3.select(self.frameElement).style(\"height\", radius * 2 + \"px\");  </script>");  
}



void expData(char *matrixFile, char *outDir, char *descFile)
/* Read matrix and names into a list of bioExpVectors, run hacTree to
 * associate them, and write output. */
{
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
    clusters = hacTreeMultiThread(clThreads, (struct slList *)list, localMem,
  					    slBioExpVectorDistance, slBioExpVectorMerge, NULL, NULL);
    }
else
    {
    clusters = hacTreeFromItems((struct slList *)list, localMem,
						slBioExpVectorDistance, slBioExpVectorMerge, NULL, NULL);
    }
struct slRef *orderedList = getOrderedLeafList(clusters);
colorLeaves(orderedList);
if (clForceLayout)
    printForceLayoutJson(f,clusters);
else{
    printHierarchicalJson(f, clusters);
    FILE *htmlF = mustOpen(catTwoStrings(outDir,".html"),"w");
    generateHtml(htmlF,size,catTwoStrings(outDir,".json")); 
    }
// Remove temporary files
char cleanup[1024];
safef(cleanup, 1024, "rm %s", catTwoStrings(matrixFile, ".*")); 
mustSystem(cleanup);
verbose(2,"%lld allocated at end\n", (long long)carefulTotalAllocated());
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clForceLayout = optionExists("forceLayout");
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
