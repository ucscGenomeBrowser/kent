/* cdwFlowCharts - Library functions to look through SQL tables and generate dagger d3 flowcharts.*/
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwStep.h"
#include "cart.h"
#include "cdwLib.h" 
#include "obscure.h"



struct slRow
// A linked list of slInt's, each slRow holds all the id's for a given row.  
    {
    struct slRow *next; 
    struct slInt *fileList; 
    };

struct slRow *files; 
struct slRow *steps; 

void makeFileLink(char *file, struct cart *cart)
/* Write out wrapper that links us to metadata display */
{
printf("<A HREF=\"../cgi-bin/cdwWebBrowse?cdwCommand=oneFile&cdwFileTag=file_name&cdwFileVal=%s&%s\">",
    file, cartSidUrlString(cart));
printf("%s</A>", file);
}

static struct slRow *slRowNew(struct slInt *fileList)
/* Return a new slRow. */
{
struct slRow *a;
AllocVar(a);
a->fileList = fileList;
return a;
}

static char *stripFilePath(char *url)
// Grab everything past the last '/' 
{
struct slName *pathPieces = charSepToSlNames(url, '/');
slReverse(&pathPieces); 
return (pathPieces->name); 
}

/*int slIntCmp(const void *a, const void *b)
{
const struct slInt *intA = ((struct slInt*) a); 
const struct slInt *intB = ((struct slInt*) b); 
if (intA->val > intB->val)
    return 0;
else return 1; 
}*/


static bool baseCase(struct sqlConnection *conn, int fileId)
// Generate the row that the clicked file is on, the clicked file will always be first in the row. 
// Return False if there is no flowchart for the file. 
{
char query[1024]; 
struct slInt *itemList = NULL; // We are building up the item list that our file is in manually

// Look for the file in stepOut (it has upstream files)
sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%i'", fileId);
struct cdwStepOut *iter, *cSO = cdwStepOutLoadByQuery(conn,query);
// Look for the file in stepIn (it has downstream files)
sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId = '%i'", fileId);
struct cdwStepIn *iter2, *cSI = cdwStepInLoadByQuery(conn,query);
if (cSI == NULL && cSO == NULL) // If the file has no upstream or downstream files return; 
    {
    sqlDisconnect(&conn); 
    return FALSE; 	
    }
// There are upstream files so check if the file has any siblings to give some context (they won't have any children). 
if (cSO != NULL) 
    {
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where stepRunId = '%u'", cSO->stepRunId); 
    struct cdwStepOut *cSOList = cdwStepOutLoadByQuery(conn, query); 
    for (iter = cSOList; iter != NULL; iter=iter->next)
	{
	struct slInt *fileId = slIntNew((int) iter->fileId);
	slAddHead(&itemList, fileId);	
	}
    }

// There are downstream files so check if their are any sibiling files passed into the step. 
if (cSI != NULL)
    {
    sqlSafef(query, sizeof(query), "select * from cdwStepIn where stepRunId = '%u'", cSI->stepRunId); 
    struct cdwStepIn *cSIList = cdwStepInLoadByQuery(conn, query); 
    for (iter2 = cSIList; iter2 != NULL; iter2=iter2->next)
	{
	struct slInt *curFile = slIntNew((int) iter2->fileId);
	if (slIntFind(itemList, (int) iter2->fileId) == NULL)
	    slAddHead(&itemList, curFile);	
	}
    }

// Pull our file to the front of the row. 
struct slInt *temp =slIntFind(itemList, fileId); 
if (slRemoveEl(&itemList,temp))
    slAddHead(&itemList, temp); 
// Make the new file row.
struct slRow *fileRow = slRowNew(itemList); 
// Add it to the list of file rows. 
slAddTail(&files, fileRow); 
return TRUE;
}

static void lookBackward(struct sqlConnection *conn, int fileId)
/* Look backwards through the cdwStep tables until the source is found, store 
 * the file and step rows along the way. Start by looking if this file is in cdwStepOut.
 * Assumes that each file can only have one parent step.  */ 
{
char query[1024];

sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%i'", fileId);
struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn,query);
if (cSO == NULL)
    return; 

while(cSO != NULL)
    {
    // Add the step.  Looking backwards there is only one step to consider.  
    struct slInt *curStep = slIntNew(cSO->stepRunId);
    struct slRow *newStep = slRowNew(curStep); 
    slAddTail(&steps, newStep);

    // All files in cdwStepOut have at least one corresponding file in cdwStepIn connected via the stepRun id 
    // Find those files with a common stepRun id and add them to the file row. 
    sqlSafef(query, sizeof(query), "select * from cdwStepIn where stepRunId = '%u'", cSO->stepRunId); 
    struct cdwStepIn *iter, *cSIList = cdwStepInLoadByQuery(conn, query);
    // Go through the list of items and make up a list of slInt's that correpond to the fileId's
    struct slInt *itemList = NULL; 
    for (iter = cSIList; iter != NULL; iter=iter->next)
	{
	struct slInt *fileId = slIntNew((int) iter->fileId);
	slAddHead(&itemList, fileId);	
	}
    // Generate a new file row
    struct slRow *newRow = slRowNew(itemList);
    // Add the new file row to the flowchart 
    slAddTail(&files, newRow);
	
    // Update the while loop to see if any of these input files are also output files. 
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%u'", cSIList->fileId);
    cSO = cdwStepOutLoadByQuery(conn,query);
    }
// Reverse things.  
slReverse(files);
slReverse(steps);
return; 
}

static void addToSteps(int depth, struct slInt *stepRow)
// add a single step to the global steps; 
{

// Check if there is a row at the current depth; 
if (slCount(steps) < depth) // Make a new row
    {
    slAddTail(&steps, slRowNew(stepRow)); 
    return;
    }
// Go to the propper depth in the slRow
struct slRow *existingRow = slElementFromIx(&steps, depth);
slAddTail(&existingRow->fileList, stepRow); 
}

static void addToFiles(int depth, struct slInt *fileRow)
// add a multiple files to the global files; 
{
if (slCount(files) < depth) // Make a new row
    {
    slAddTail(&files, slRowNew(fileRow)); 
    return;
    }
// Go to the propper depth in the slRow
struct slRow *existingRow = slElementFromIx(&files, depth);
slAddTail(&existingRow->fileList, fileRow); 

}

static void rLookForward(struct sqlConnection *conn, int fileId, int depth)
{
uglyf("rLookForward\n");
//check for the number of times the file shows up in cdwStepIn
char query[1024]; 
// Get the cdwStepIn entry for an entry in the first row, note that the file could have
// multiple downstream pipelines and will then have multiple entries in cdwStepIn
sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId = '%i'", fileId);
struct cdwStepIn *cSIList = cdwStepInLoadByQuery(conn,query);
//if it doesnt; 	return; 
if (cSIList == NULL)
    return; 
uglyf("recursing...\n"); 

struct cdwStepIn *curStep; 
// This looks through the pipeline steps horizontally 
/*	for each step;
	    load the step && row information at the correct depth; (uses global row/step variables)
	    recurse on all associated cdwStepOut files at depth + 1; 
*/
for (curStep = cSIList; curStep != NULL; curStep = curStep->next) 
    {
    uglyf("s count here is %i depth is % i\n", slCount(steps), depth);
    addToSteps(depth, slIntNew(curStep->stepRunId));
    uglyf("s count here is %i depth is % i\n", slCount(steps), depth);
    
    // get the associated stepOut files 
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where stepRunId = '%u'", curStep->stepRunId); 
    struct cdwStepOut *iter, *cSOList = cdwStepOutLoadByQuery(conn, query);
    
    // Go through the step out files and make a list of ints where each int is a file id;  
    struct slInt *fileIdList = NULL; 
    // All files in cdwStepIn have at least one corresponding file in cdwStepOut keyed into via the stepRun 
    // Go through the list of items and make up a list of slInt's that correpond to the fileId's
    for (iter = cSOList; iter != NULL; iter=iter->next)
	{
	slAddHead(&fileIdList, slIntNew((int) iter->fileId));
	}
    uglyf("f count here is %i depth is %i\n", slCount(files),depth);
    slSort(fileIdList,  *slIntCmp);
    addToFiles(depth + 1, fileIdList); 
    uglyf("f count here is %i depth is %i\n", slCount(files),depth);
    uglyf("the fileId is %i", cSOList->fileId);
    uglyf("the fileId is %i", cSOList->fileId);

    rLookForward(conn, fileIdList->val, depth+2); // Could put this in the for loop to recurse on all children instead of just 1
    }
}

static void printRowToStep(struct sqlConnection *conn, struct slRow *fileRow, struct slInt *stepRow, int filesPerRow)
{
char query[1024];
struct slInt *item;
int count = 0;
// Go through the list of int's that are the file Id's for each row
//uglyf("in here somewhere \n"); 
for (item = fileRow->fileList; item != NULL; item = item->next)
    {
    // Check if the files all share the same stepRow
    // List the last file as a 'n'
    
    
    if(count +1 == filesPerRow)
	{
	struct slInt *lastFileId = slElementFromIx(fileRow->fileList, slCount(fileRow->fileList)-1);
	sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%u' and stepRunId='%i'", lastFileId->val, stepRow->val);
	struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);
	if (cSI==NULL)
	    continue;
	// Get the file name 
	sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", lastFileId->val);
	struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
	printf("g.setEdge(\"... %s %i\",", stripFilePath(cF->cdwFileName), slCount(fileRow->fileList)); 
	// Get the step name 
	sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepRow->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	printf("\"%s\", ""{label:\"\"});\n", cSD->name);	
	break;
	}
    
    sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%u' and stepRunId='%i'", item->val, stepRow->val);
    struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);
    //uglyf("the query is %s\n", query); 
    //uglyf("here...,%i  %i \n", item->val, stepRow->val);
    if (stepRow->next != NULL)
	stepRow = stepRow->next; 
    
    if (cSI==NULL)
	continue;
    

    ++count;
    sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", item->val);
    struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
    printf("g.setEdge(\"%s %i\",", stripFilePath(cF->cdwFileName), count); 
    
    sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepRow->val);
    struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
    sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
    struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
    printf("\"%s\", ""{label:\"\"});\n", cSD->name);
    }
}

static void printStepToRow(struct sqlConnection *conn, struct slRow *fileRow, struct slInt *stepRow, int filesPerRow)
// The difficult case, a step can produce 1 file in the next row, 2 files, or up to n files.  
{
char query[1024]; 
// Get the run tag from stepRow
sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepRow->val);
struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
// Use the run tag to get the step definition from stepDef
sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query);

struct slInt *item;
sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%u'", fileRow->fileList->val);
struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn, query);
int ourStep = cSO->stepRunId, count=0; 
for (item = fileRow->fileList; item != NULL; item = item->next)
    {
    ++count; 
    if (count == filesPerRow)
	{
	if (cSO->stepRunId == ourStep)
	    {
	    struct slInt *lastFileId = slElementFromIx(fileRow->fileList, slCount(fileRow->fileList)-1);
	    sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", lastFileId->val);
	    struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
	    printf("g.setEdge(\"%s\", \"... %s %i\", {label:\"\"});\n", cSD->name, stripFilePath(cF->cdwFileName), slCount(fileRow->fileList));
	    }
	break;
	}
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%u'", item->val);
    cSO = cdwStepOutLoadByQuery(conn, query);
    // Check if the file shares the same stepRunId, if it does draw a link to it.  
    if (cSO->stepRunId == ourStep)
	{
	sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", item->val);
	struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
	printf("g.setEdge(\"%s\", \"%s %i\", {label:\"\"});\n", cSD->name, stripFilePath(cF->cdwFileName), count);
    	}
    }
}

static void printToDagreD3(struct sqlConnection *conn, int fileId, struct slRow *files, struct slRow *steps, int filesPerRow)
/* Print out the modular parts of a a dager-d3 javascript visualization
 * These will need to be put into the .js code at the right place. The
 * base flowchart code that is being used was found here; 
 * http://cpettitt.github.io/project/dagre-d3/latest/demo/tcp-state-diagram.html */ 
{
struct slRow *fileRow; 
struct slRow *stepRow = steps->next; 
printf("<script src=\"http://cpettitt.github.io/project/dagre-d3/latest/dagre-d3.js\"></script>\n");
printf("<style id=\"css\">\n");
printf(".node rect,.node ellipse {\n\tstroke: #333;\n\tfill: #fff;\n\tstroke-width: 1.5px;}\n");
printf(".edgePath path {\n\tstroke: #333;\n\tfill: #333;\n\tstroke-width: 1.5px;}\n");

// Calculate a rough SVG box size. Since the table is always very wide we can ignore
// width and focus on height. 
int height = (slCount(files) + slCount(steps) - 2)*80; 
printf("</style>\n<svg width=5000 height=%i>\n<g/>\n</svg>\n", height);
printf("<script id=\"js\">\n");
printf("var g = new dagreD3.graphlib.Graph().setGraph({});\n");  

// Define all the nodes, this sets the visual node name
printf("var states = [");
char query[1024];
for (fileRow = files->next; fileRow != NULL; fileRow=fileRow->next)
    {
    struct slInt *item;
    int count=0;
    // Add the files in the row to the graph, up to 'filesPerRow' files are added if found.
    
    for (item = fileRow->fileList; item != NULL; item = item->next)
	{
	++count; 
	if(count == filesPerRow)
	    {
	    struct slInt *lastFileId = slElementFromIx(fileRow->fileList, slCount(fileRow->fileList)-1);
	    sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", lastFileId->val);
	    struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
	    printf("\"... %s %i\"", stripFilePath(cF->cdwFileName), slCount(fileRow->fileList));
	    break;
	    }
	sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", item->val);
	struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
	printf("\"%s %i\"", stripFilePath(cF->cdwFileName), count);
	if (item->next != NULL)
	    printf(","); 
	}
    // Add the steps to the graqph, only one step per row is allowed.  
    if (fileRow->next != NULL) 
	{
	sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepRow->fileList->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	printf(",\"%s\",", cSD->name); 
	stepRow = stepRow->next; 
	}
    }
printf("];\n"); 
printf("states.forEach(function(state) { g.setNode(state, { label: state }); });\n");

// Define the step's as 'ellipses' so they look different, css definition of elipse is above 
stepRow = steps->next; 
struct slInt *iter; 
for (iter = stepRow->fileList; iter != NULL; iter=iter->next)
    {
    sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", iter->val);
    struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
    sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
    struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
    printf("g.setNode(\"%s\", {shape:\"ellipse\"});\n", cSD->name ); 
    }

// Link the nodes up, all the files from a given row are linked to the next row. 
stepRow = steps->next; 
for (fileRow = files->next; fileRow != NULL; fileRow=fileRow->next)
    {
    // Print row to step 
    printRowToStep(conn, fileRow, stepRow->fileList, filesPerRow);  
    // Print step to next row 
    if (fileRow->next != NULL) 
	{
	printStepToRow(conn, fileRow->next, stepRow->fileList, filesPerRow);
	stepRow = stepRow->next; 
	}
    if (stepRow == NULL) break;
    }
printf("\nconsole.log(g.nodes())"); 

printf("\ng.nodes().forEach(function(v) \n{ \n\tvar node = g.node(v); \n\tnode.rx = node.ry = 5; \n}); \n");


sqlSafef(query, sizeof(query), "select * from cdwFile where id= '%i'", fileId); 
struct cdwFile *cF = cdwFileLoadByQuery(conn,query); 
// Color the node that is being selected 
printf("g.node('%s 1').style = \"fill: #7f7\";\n", stripFilePath(cF->cdwFileName)); 

printf("var svg = d3.select(\"svg\"), inner = svg.select(\"g\");\nvar render = new dagreD3.render();\nrender(inner, g);");
printf("var initialScale = 0.75;\n"); 
printf("zoom .translate([(svg.attr(\"width\") - g.graph().width * initialScale) / 2, 20]) .scale(initialScale) .event(svg);\n"); 
printf("svg.attr('height', g.graph().height * initialScale + 40); </script>\n"); 
}

void makeCdwFlowchart(int fileId, struct cart *cart)
/* sqlToTxt - A program that runs through SQL tables and generates history flow chart information. */
{
//struct slRow *files, *steps; // All the file id's for each row are stored in files, and the steps in steps
AllocVar(files); 
AllocVar(steps); 

struct sqlConnection *conn = cdwConnect(); 

// Verify that the file is a valid file (this should happen in all cases, but check anyways). 
char query[1024]; 
sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%i'", fileId);
struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
if (cF == NULL) 
    {
    sqlDisconnect(&conn); 
    return; 
    }

// Fill up the current row return false if their is no flowchart. 
if (baseCase(conn, fileId) == FALSE)
    return; 

lookBackward(conn, fileId);
uglyf("There are this many items in steps %i and this many in files %i", slCount(steps), slCount(files)); 
assert(slCount(files) == slCount(steps) + 1); 

rLookForward(conn, fileId, slCount(steps) + slCount(files) -1);
uglyf("There are this many items in steps %i and this many in files %i", slCount(steps), slCount(files)); 
assert(slCount(files) == slCount(steps) + 1); 



/*struct slRow *stepIter, *fileIter; 
for (stepIter = steps; stepIter !=NULL; stepIter = stepIter->next)
    {
    slSort(stepIter->fileList,  *slIntCmp);
    }

for (fileIter = files; fileIter !=NULL; fileIter = fileIter->next)
    {
    slSort(fileIter->fileList,  *slIntCmp);
    }
*/
uglyf("getting here..?"); 

if (slCount(files) == 1 || slCount(steps) == 1)
    {
    sqlDisconnect(&conn); 
    return; 
    }
printToDagreD3(conn, fileId, files, steps, 5);
sqlDisconnect(&conn); 
}

