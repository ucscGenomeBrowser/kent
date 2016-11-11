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

//Global variables needed for the recursive function.  
struct slRow *files; 
struct slRow *steps; 

static struct slRow *slRowNew(struct slInt *fileList)
/* Return a new slRow. */
{
struct slRow *a;
AllocVar(a);
a->fileList = fileList;
return a;
}

static char *stripFilePath(char *url)
/* Return everything past the last '/' as a string. */ 
{
struct slName *pathPieces = charSepToSlNames(url, '/');
slReverse(&pathPieces); 
return (pathPieces->name); 
}

static bool baseCase(struct sqlConnection *conn, int fileId)
/* Generate the row that the clicked file is on, the clicked file will always be first in the row. 
 * Return False if there is no flowchart for the file. */ 
{
char query[1024]; 
struct slInt *itemList = NULL; // Used to build up the item list that our file is in manually

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
/* Add a single step to the global list of steps at the propper depth. */ 
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
return; 
}

static void rLookForward(struct sqlConnection *conn, int fileId, int depth)
/* Recursively look forward through the sql databases. The function has two options to recurse, 
 * forward down an analysis pipeline, or horizontally to a new analysis pipeline. */
{
char query[1024]; 
// Get the cdwStepIn entry for an entry in the first row, note that the file could have
// multiple downstream pipelines and will then have multiple entries in cdwStepIn.
sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId = '%i'", fileId);
struct cdwStepIn *cSIList = cdwStepInLoadByQuery(conn,query);
if (cSIList == NULL)
    return; 

struct cdwStepIn *curStep; 
/* This looks through the pipeline steps horizontally. For each step; load the step and row 
 * information at the correct depth; (uses global row/step variables). Then recurse on the first
 * associated cdwStepOut files at depth + 1. */
for (curStep = cSIList; curStep != NULL; curStep = curStep->next) 
    {
    addToSteps(depth, slIntNew(curStep->stepRunId));
    
    // Get the associated stepOut files.
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where stepRunId = '%u'", curStep->stepRunId); 
    struct cdwStepOut *iter, *cSOList = cdwStepOutLoadByQuery(conn, query);
    
    // Go through the step out files and make a list of ints where each int is a file id;  
    struct slInt *fileIdList = NULL; 
    // All files in cdwStepIn have at least one corresponding file in cdwStepOut keyed into via the stepRun. 
    // Go through the list of items and make up a list of slInt's that correpond to the fileId'.
    for (iter = cSOList; iter != NULL; iter=iter->next)
	{
	slAddHead(&fileIdList, slIntNew((int) iter->fileId));
	}
    // Keep things ordered. 
    slSort(fileIdList,  *slIntCmp);
    addToFiles(depth + 1, fileIdList); 
    // Each recursion has a step and file row, hence increase depth by 2. 
    rLookForward(conn, fileIdList->val, depth+2); 
    }
}

static void printRowToStep(struct sqlConnection *conn, struct slInt *fileRow, struct slInt *stepRow, int filesPerRow)
/* Given a list of files and a list of steps connect the files to the steps where appropriate.  */ 
{
char query[1024];
struct slInt *stepIter; 
//stepIter;
int count = 0;
// The input files could go down multiple pipelines, for each step link up the files to the step if appropriate.
for (stepIter = stepRow; stepIter != NULL; stepIter = stepIter->next)
    {
    struct slInt *fileIter; 
    for (fileIter = fileRow; fileIter != NULL; fileIter = fileIter->next)
	{
	// For each file in the file row check if it goes into the current step.  
	sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%u' and stepRunId='%i'", fileIter->val, stepIter->val);
	struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);
	if (cSI==NULL) // These files are not passed into the current step. 
	    continue;
	++count;
	// All files below connect to the current step.  
	// Grab the file name and define an edge from the file to the step.  
	sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", fileIter->val);
	struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
	printf("g.setEdge(\"%s %i\",", stripFilePath(cF->cdwFileName), count); // The file. 
	sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepIter->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	printf("\"%s\", ""{label:\"\"});\n", cSD->name);  // The step. 

	// Handle overflow. 
	if (count +1  == filesPerRow)
	    {
	    struct slInt *lastFileId = slElementFromIx(fileRow, slCount(fileRow)-1);
	    sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%u' and stepRunId='%i'", lastFileId->val, stepIter->val);
	    struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);
	    if (cSI==NULL)
		continue;
	    // Get the last file name and define an edge from it to the step. 
	    sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", lastFileId->val);
	    struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
	    printf("g.setEdge(\"... %s %i\",", stripFilePath(cF->cdwFileName), slCount(fileRow)); 
	    sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepIter->val);
	    struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	    sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	    struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	    printf("\"%s\", ""{label:\"\"});\n", cSD->name);	
	    break;
	    }	
	}
    }
}

static void printStepToRow(struct sqlConnection *conn, struct slInt *fileRow, struct slInt *stepRow, int filesPerRow)
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
sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%u'", fileRow->val);
struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn, query);
int ourStep = cSO->stepRunId, count=0; 
for (item = fileRow; item != NULL; item = item->next)
    {
    ++count; 
    if (count == filesPerRow)
	{
	if (cSO->stepRunId == ourStep)
	    {
	    struct slInt *lastFileId = slElementFromIx(fileRow, slCount(fileRow)-1);
	    sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", lastFileId->val);
	    struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
	    printf("g.setEdge(\"%s\", \"... %s %i\", {label:\"\"});\n", cSD->name, stripFilePath(cF->cdwFileName), slCount(fileRow));
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

static void printToDagreD3(struct sqlConnection *conn, int fileId, struct slRow *files, struct slRow *steps, int filesPerRow, struct cart *cart)
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
int height = (slCount(files) + slCount(steps) - 2) * 80;
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

// Define the step's as 'ellipses' so they look different, css definition of elipse is above. 
stepRow = steps->next; 
struct slRow *outerIter; 
for (outerIter = steps; outerIter != NULL; outerIter = outerIter->next) // Loop vertically (frequently used).
    {
    struct slInt *innerIter;
    for (innerIter = outerIter->fileList; innerIter != NULL; innerIter=innerIter->next) // Loop horizontally (rarely used).
	{
	sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", innerIter->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	printf("g.setNode(\"%s\", {shape:\"ellipse\"});\n", cSD->name ); 
	}
    }

// Link the nodes up, all the files from a given row are linked to the next row. 
stepRow = steps->next; 
for (fileRow = files->next; fileRow != NULL; fileRow=fileRow->next)
    {
    // Print row to step 
    printRowToStep(conn, fileRow->fileList, stepRow->fileList, filesPerRow);  
    // Print step to next row 
    if (fileRow->next != NULL) 
	{
	printStepToRow(conn, fileRow->next->fileList, stepRow->fileList, filesPerRow);
	stepRow = stepRow->next; 
	}
    if (stepRow == NULL) break;
    }
printf("\ng.nodes().forEach(function(v) \n{ \n\tvar node = g.node(v); \n\tnode.rx = node.ry = 5; \n}); \n");

sqlSafef(query, sizeof(query), "select * from cdwFile where id= '%i'", fileId); 
struct cdwFile *cF = cdwFileLoadByQuery(conn,query); 
// Color the node that is being selected 
printf("g.node('%s 1').style = \"fill: #7f7\";\n", stripFilePath(cF->cdwFileName)); 

printf("var svg = d3.select(\"svg\"), inner = svg.select(\"g\");\nvar render = new dagreD3.render();\nrender(inner, g);\n");
// Add hyperlinking to the nodes, first build a list of all nodes and find its length.
printf("var nodeList = d3.select(inner.select(\"g.output\").select(\"g.nodes\").selectAll(\"g.node\"))[0][0][0];\n"); 
printf("var nodeListLen = d3.select(inner.select(\"g.output\").select(\"g.nodes\").selectAll(\"g.node\"))[0][0][0].length;\n");
// Use a for loop to traverse the list of nodes, for each node attach a hyperlink to the 'on click' function . 
printf("for (i = 0; i < nodeListLen; i++)\n"); 
printf("\t\t{\n\t\td3.select(nodeList[i])\n\t\t\t.on(\"click\", function (d)\n\t\t\t\t{\n"); // Define an on click event. 
printf("\t\t\t\tfileNameParts = d.split(\" \");"); // Find the file name (SCH000***) which is used to build the hyperlink. 
printf("\n\t\t\t\tif (fileNameParts.length == 1)\n\t\t\t\t\t{return;}"); // This grabs the step nodes and returns before applying a bad hyperlink. 
printf("\n\t\t\t\tif (fileNameParts[0] === \"...\")\n\t\t\t\t\t{");  // This grabs the files that start with '...'. 
printf("\n\t\t\t\t\tvar fileLink = \"../cgi-bin/cdwWebBrowse?cdwCommand=oneFile&cdwFileTag=file_name&cdwFileVal=\"+fileNameParts[1]+\"&%s\";\n", cartSidUrlString(cart)); 
printf("\t\t\t\t\twindow.location.href = fileLink;\n\t\t\t\t\t}\n");  
printf("\t\t\t\telse{\n"); // This deals with the majority of nodes.  
printf("\t\t\t\t\tvar fileLink = \"../cgi-bin/cdwWebBrowse?cdwCommand=oneFile&cdwFileTag=file_name&cdwFileVal=\"+fileNameParts[0]+\"&%s\";\n", cartSidUrlString(cart)); 
printf("\t\t\t\t\twindow.location.href = fileLink;\n");
printf("\t\t\t\t\t}\n\t\t\t\t});\n\t\t}\n\n"); 
printf("var initialScale = 0.75;</script>\n"); 
}

void makeCdwFlowchart(int fileId, struct cart *cart)
/* Run through SQL tables and generate history flow chart information. */
{
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

// Look backwards and use an assert to ensure things are going as they should. 
lookBackward(conn, fileId);
assert(slCount(files) == slCount(steps) + 1); 

// Look forwards and use an assert to ensure things are going as they should. 
rLookForward(conn, fileId, slCount(steps) + slCount(files) -1);
assert(slCount(files) == slCount(steps) + 1); 

if (slCount(files) == 1 || slCount(steps) == 1)
    {
    sqlDisconnect(&conn); 
    return; 
    }
// Print everything 
printToDagreD3(conn, fileId, files, steps, 5, cart);
sqlDisconnect(&conn); 
}

