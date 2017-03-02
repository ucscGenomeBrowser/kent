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
struct slRow *gFiles; 
struct slRow *gSteps; 
int gLocation = 0; 

static struct slRow *slRowNew(struct slInt *fileList)
/* Return a new slRow. */
{
struct slRow *a;
AllocVar(a);
a->fileList = fileList;
return a;
}

static char *stripFilePath(char *url)
/* Return everything past the last '/' in a file path as a string. */ 
{
// use splitPath here -JK
struct slName *pathPieces = charSepToSlNames(url, '/');
slReverse(&pathPieces); 
return (pathPieces->name); 
}

static bool baseCase(struct sqlConnection *conn, int fileId, int filesPerRow)
/* Generate the row that the clicked file is on. 
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
int count = 0; 
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
	++count; 
	struct slInt *fileId = slIntNew((int) iter->fileId);
	slAddHead(&itemList, fileId);	
	if (count == filesPerRow) break;
	}
    }

// There are downstream files so check if their are any sibiling files passed into the step. 
if (cSI != NULL)
    {
    sqlSafef(query, sizeof(query), "select * from cdwStepIn where stepRunId = '%u'", cSI->stepRunId); 
    struct cdwStepIn *cSIList = cdwStepInLoadByQuery(conn, query); 
    count = 0; 
    for (iter2 = cSIList; iter2 != NULL; iter2=iter2->next)
	{
	struct slInt *curFile = slIntNew((int) iter2->fileId);
	if (slIntFind(itemList, (int) iter2->fileId) == NULL)
	    {
	    ++count; 
	    slAddHead(&itemList, curFile);	
	    if (count + 1 == filesPerRow) break;
	    }	
	}
    }

// Keep track of the file that was clicked on so it can be highlighted later.  
struct slInt *temp =slIntFind(itemList, fileId); 
slSort(&itemList, slIntCmp);	
gLocation = slIxFromElement(&itemList, temp);
// Make the new file row.
struct slRow *fileRow = slRowNew(itemList); 
// Add it to the list of file rows. 
slAddHead(&gFiles, fileRow);
return TRUE;
}

static void lookBackward(struct sqlConnection *conn, int fileId, int filesPerRow)
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
    slAddHead(&gSteps, newStep);

    // All files in cdwStepOut have at least one corresponding file in cdwStepIn connected via the stepRun id 
    // Find those files with a common stepRun id and add them to the file row. 
    sqlSafef(query, sizeof(query), "select * from cdwStepIn where stepRunId = '%u'", cSO->stepRunId); 
    struct cdwStepIn *iter, *cSIList = cdwStepInLoadByQuery(conn, query);
    // Go through the list of items and make up a list of slInt's that correpond to the fileId's
    struct slInt *itemList = NULL; 
    int count = 0; 
    for (iter = cSIList; iter != NULL; iter=iter->next)
	{
	++count; 
	struct slInt *fileId = slIntNew((int) iter->fileId);
	slAddHead(&itemList, fileId);
	if (count == filesPerRow) break; 
	}
    // Generate a new file row
    struct slRow *newRow = slRowNew(itemList);
    // Add the new file row to the flowchart 
    slAddHead(&gFiles, newRow);
	
    // Update the while loop to see if any of these input files are also output files. 
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%u'", cSIList->fileId);
    cSO = cdwStepOutLoadByQuery(conn,query);
    }
// Reverse things.  
return; 
}

static char *getOutputType(struct sqlConnection *conn, int fileId)
{
char query[1024]; 
sqlSafef(query, sizeof(query), "select name from cdwStepIn where fileId='%u'", fileId); 
char buf[2048];
char *output = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (output == NULL) 
    {
    sqlSafef(query, sizeof(query), "select name from cdwStepOut where fileId='%u'", fileId); 
    output = sqlQuickQuery(conn, query, buf, sizeof(buf));
    }
return output; 
}

static void addToSteps(int depth, struct slInt *stepRow)
/* Add a single step to the global list of steps at the propper depth. */ 
{
int gsc = slCount(gSteps);
int remaining = gsc - depth;
// Check if there is a row at the current depth; 
if (depth > gsc) // Make a new row
    {
    slAddHead(&gSteps, slRowNew(stepRow)); 
    return;
    }
else 
    {
    // Extend a previous row 
    if (remaining > 0)
	{
	struct slRow *row = slElementFromIx(gSteps, remaining);
	row->fileList = slCat(row->fileList, stepRow);
	return;
	}
    // Extend current depth
    gSteps->fileList = slCat(gSteps->fileList,stepRow); 
    }
return;
}

static void addToFiles(int depth, struct slInt *fileRow)
// Add a list with  multiple files to the global files list at the propper depth.  
{
int gfc = slCount(gFiles);
int remaining = gfc - depth;
// Check the current depth of the file struct
if (depth > gfc) // Make a new row
    {
    slAddHead(&gFiles, slRowNew(fileRow)); 
    return;
    }
else // Go to the right row
    {
    // Extend a previous row 
    if (remaining > 0)
	{
	struct slRow *row = slElementFromIx(gFiles, remaining);
	row->fileList = slCat(row->fileList, fileRow);
	return;
	}
    // Extend the current row
    gFiles->fileList = slCat(gFiles->fileList,fileRow); 
    }
return;
}

static void rLookForward(struct sqlConnection *conn, int fileId, int fileDepth, int stepDepth, int filesPerRow)
/* Recursively look forward through the sql databases. The function has two options of progression, 
 * forward recursively down an analysis pipeline, or horizontally to a new analysis pipeline. */
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
    addToSteps(stepDepth, slIntNew(curStep->stepRunId));
    //addToGRow(stepDepth, slIntNew(curStep->stepRunId), FALSE);

    // Get the associated stepOut files.
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where stepRunId = '%u'", curStep->stepRunId); 
    struct cdwStepOut *iter, *cSOList = cdwStepOutLoadByQuery(conn, query);
    
    // Go through the step out files and make a list of ints where each int is a file id;  
    struct slInt *fileIdList = NULL; 
    // All files in cdwStepIn have at least one corresponding file in cdwStepOut keyed into via the stepRun. 
    // Go through the list of items and make up a list of slInt's that correpond to the fileId'.
    int count = 0; 
    for (iter = cSOList; iter != NULL; iter=iter->next)
	{
	++count; 
	slAddHead(&fileIdList, slIntNew((int) iter->fileId));
	if (count == filesPerRow) 
	    {
	    break;
	    }
	}
    // Keep things ordered. 
    slSort(fileIdList, *slIntCmp);
    addToFiles(fileDepth, fileIdList); 
    // Each recursion has a step and file row, hence increase depth by 2. 
    rLookForward(conn, fileIdList->val, fileDepth + 1, stepDepth + 1, filesPerRow); 
    }
}

static int getSiblingCount(struct sqlConnection *conn, int fileId)
/* Get the number of siblings for a given fileId*/
{
char query[1024];
int count = 0; 
    
// Look for the file in stepOut (it has upstream files)
sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%i'", fileId);
struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn,query);
// Look for the file in stepIn (it has downstream files)
sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId = '%i'", fileId);
struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn,query);
// There are upstream files so check if the file has any siblings to give some context (they won't have any children). 
if (cSO != NULL) 
    {
    sqlSafef(query, sizeof(query), "select count(*) from cdwStepOut where stepRunId = '%u'", cSO->stepRunId); 
    count += sqlQuickNum(conn, query); 
    }

// There are downstream files so check if their are any sibiling files passed into the step. 
if (cSI != NULL)
    {
    sqlSafef(query, sizeof(query), "select count(*) from cdwStepIn where stepRunId = '%u'", cSI->stepRunId); 
    count += sqlQuickNum(conn, query); 
    }
return count; 
}

static void printFileRowToAnalysisRow(struct sqlConnection *conn, struct slInt *fileRow, struct slInt *stepRow, int filesPerRow, struct dyString *dy)
/* Print out a file row to an analysis row, this is the more difficult of the two cases as each file can have multiple analysis steps
 * or no analysis step at all.*/
{
char query[1024];
struct slInt *stepIter; 
// The input files could go down multiple pipelines, check each analysis step.
for (stepIter = stepRow; stepIter != NULL; stepIter = stepIter->next)
    {
    struct slInt *fileIter; 
    int fileCount = 0; 
    // Each analysis step will have specific files, go through the file list and find them. 
    for (fileIter = fileRow; fileIter != NULL; fileIter = fileIter->next)
	{
	// For each file in the file row check if it goes into the current step.  
	sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%u' and stepRunId='%i'", fileIter->val, stepIter->val);
	struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);
	if (cSI == NULL) // The current file is not produced by this step.   
	    continue;
	
	// All files below connect to the current step.  
	++fileCount;
	// Grab the file name and define an edge from the step to the file.    
	sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", fileIter->val);
	struct cdwFile *cF = cdwFileLoadByQuery(conn, query); // cF->name is the file name that appears on the flowchart. 
	
	sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepIter->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	
	sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); // cSD->name is the analysis step name. 
    
	char *output = getOutputType(conn, fileIter->val); 
	// Handle overflow. 
	if (fileCount  == filesPerRow)
	    {
	    dyStringPrintf(dy,"g.setEdge(\"... %s\\n%s %i\",", stripFilePath(cF->cdwFileName), output, getSiblingCount(conn, fileIter->val));
	    dyStringPrintf(dy,"\"%s\", ""{label:\"\"});\n", cSD->name);	
	    break;
	    }	
	dyStringPrintf(dy,"g.setEdge(\"%s\\n%s\",", stripFilePath(cF->cdwFileName), output); // The file. 
	dyStringPrintf(dy,"\"%s\", ""{label:\"\"});\n", cSD->name);  // The step. 
	}
    }
}

static void printAnalysisStepsToFileRow(struct sqlConnection *conn, struct slInt *fileRow, int filesPerRow, struct dyString *dy)
// Print out an analysis row to the file row, this is the easier of the two cases as each file has one and 
// only one analysis step that generates it. 
{
char query[1024]; 
// Get the a name to use for the step, it lies in cdwStepDef, indexed through cdwStepRun.
sqlSafef(query, sizeof(query), "select stepRunId from cdwStepOut where fileId='%i'", fileRow->val); 
int stepOutId = sqlQuickNum(conn, query), stepInId = 0;
sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%i'", fileRow->val); 
struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query); 
if (cSI != NULL)
    stepInId = cSI->stepRunId; 

int siblingOutput = 0, siblingInput = 0; 
struct slInt *file;
slSort(fileRow, slIntCmp); 
// Go through the file row and identify the files that were created by the current analysis step.  
for (file = fileRow; file != NULL; file = file->next)
    { 
    int curId; // curId is the step that generated this file. 
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId='%i'", file->val); 
    struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn, query);
    curId=cSO->stepRunId;
    ++siblingOutput; 

    sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%i'", file->val); 
    cSI = cdwStepInLoadByQuery(conn, query);
    if (cSI != NULL)
	{
	if (cSI->stepRunId == stepInId)
	    ++siblingInput;  
	else 
	    siblingInput = 0; 
	}
    if (stepOutId != curId)
	{
	stepOutId = curId; 
	siblingOutput = 0; 
	}
    
    sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", file->val);
    struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
    char *output = getOutputType(conn, file->val); 
    sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepOutId);
    struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
    sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
    struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); // cSD->name is the analysis step name (ie Kallisto). 
    
    if (siblingInput == filesPerRow || siblingOutput == filesPerRow)
	{
	dyStringPrintf(dy,"g.setEdge(\"%s\", \"... %s\\n%s %i\", {label:\"\"});\n", cSD->name, stripFilePath(cF->cdwFileName), output, getSiblingCount(conn, file->val));
	break;
	}
    dyStringPrintf(dy,"g.setEdge(\"%s\", \"%s\\n%s\", {label:\"\"});\n", cSD->name, stripFilePath(cF->cdwFileName), output);
    }
}

static void printStates(int filesPerRow, struct sqlConnection *conn, struct dyString *dy)
{
/* Define all the nodes, this sets the visual node name. Catches the last file in the analysis step
 * and appends the appropriate sibling count as an integer. */

dyStringPrintf(dy,"var states = [");
char query[1024];
struct slRow *fileRow, *stepRow;

// Go through the file rows. The files within a given file row are sorted
// by their analysis steps. 
for (fileRow = gFiles; fileRow != NULL; fileRow=fileRow->next)
    {
    // Get the initial analysis step. 
    struct slInt *file;
    sqlSafef(query, sizeof(query), "select stepRunId from cdwStepOut where fileId = '%u'", fileRow->fileList->val);
    int stepId = sqlQuickNum(conn, query), count = 1;
    
    // Go through each file in the file row. 
    for (file = fileRow->fileList; file != NULL; file = file->next)
	{
	// Get some meta data on the file. 
	sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", file->val);
	struct cdwFile *cF = cdwFileLoadByQuery(conn, query);		

	sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId = '%u'", file->val);
	struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);
	int curStep;
	// Look for an upstream analysis step
	if (cSI != NULL)
	    curStep = cSI->stepRunId; 
	else
	// Otherwise look for a downstream analysis step
	    {   
	    sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%u'", file->val);
	    struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn, query);
	    curStep = cSO->stepRunId;
	    }
	
	// When the current step no longer is the stepId reset the file count. 
	if (stepId != curStep)
	    {
	    stepId = curStep; 
	    count = 1;
	    }
	dyStringPrintf(dy,"\"");
	if (count == filesPerRow)
	    dyStringPrintf(dy,"... "); 
	char *output = getOutputType(conn, file->val); 
	dyStringPrintf(dy,"%s\\n%s", stripFilePath(cF->cdwFileName), output);
	if (count == filesPerRow)
	    {
	    dyStringPrintf(dy," %i", getSiblingCount(conn, file->val));
	    }
	    
	dyStringPrintf(dy,"\","); 
	++count;
	}
    }

// Print out the analysis steps.
stepRow = gSteps; 
struct slRow *row; 
for (row = stepRow; row != NULL; row = row->next)
    {
    struct slInt *step;
    for (step = row->fileList; step != NULL; step = step->next)
	{
	sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", step->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	dyStringPrintf(dy,"\"%s\"", cSD->name);
	if (step->next != NULL) 
	    dyStringPrintf(dy,","); 
	}
    if (row->next != NULL)
	dyStringPrintf(dy,","); 
    }
dyStringPrintf(dy,"];\n"); 
dyStringPrintf(dy,"states.forEach(function(state) { g.setNode(state, { label: state }); });\n");
}

static void defineSteps(struct sqlConnection *conn, struct dyString *dy) 
// Define the step's as 'ellipses' so they look different, css definition of elipse is above.
{
char query[1024]; 
struct slRow *outerIter; 
for (outerIter = gSteps; outerIter != NULL; outerIter = outerIter->next) // Loop vertically (frequently used).
    {
    struct slInt *innerIter;
    for (innerIter = outerIter->fileList; innerIter != NULL; innerIter=innerIter->next) // Loop horizontally (rarely used).
	{
	sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", innerIter->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	dyStringPrintf(dy,"g.setNode(\"%s\", {shape:\"ellipse\"});\n", cSD->name ); 
	}
    }
dyStringPrintf(dy,"\n"); 
}

static void linkStepAndFileRows(struct sqlConnection *conn, int filesPerRow, struct dyString *dy)
// Link up the nodes in the graph. Go through the file row and step row global structs
// in lockstep. 
{
struct slRow *stepRow = gSteps, *fileRow; 
for (fileRow = gFiles; fileRow != NULL; fileRow=fileRow->next)
    {
    // Print links from a file row to an analysis row, there may be multiple unique analysis pipelines
    printFileRowToAnalysisRow(conn, fileRow->fileList, stepRow->fileList, filesPerRow, dy);  
    if (fileRow->next != NULL) // If there is another fileRow there is another step row 
	{
	printAnalysisStepsToFileRow(conn, fileRow->next->fileList, filesPerRow, dy);
	stepRow = stepRow->next; 
	}
	
    if (stepRow == NULL) break;
    }
}

static void colorSelectedNode(struct sqlConnection *conn, int filesPerRow, int fileId, struct dyString *dy)
// Color the node that was clicked on. 
{
char query[1024]; 
sqlSafef(query, sizeof(query), "select * from cdwFile where id= '%i'", fileId); 
struct cdwFile *cF = cdwFileLoadByQuery(conn,query);
char *output = getOutputType(conn, fileId); 
if (gLocation > filesPerRow - 1 )
    {
    dyStringPrintf(dy,"g.node('... %s\\n%s %i').style = \"fill: #7f7\";\n", stripFilePath(cF->cdwFileName), output, getSiblingCount(conn, fileId)); 
    }
else
    dyStringPrintf(dy,"g.node('%s\\n%s').style = \"fill: #7f7\";\n", stripFilePath(cF->cdwFileName), output); 
} 

static void printToDagreD3(struct sqlConnection *conn, int fileId, int filesPerRow, struct cart *cart, struct dyString *dy)
/* Print out the data structure to a dagre d3 friendly format. This has two main steps, 
 * defining all the nodes on the graph, then defining the links between them.  The names
 * must match exactly.  
 * The base flowchart code that was found here; 
 * http://cpettitt.github.io/project/dagre-d3/latest/demo/tcp-state-diagram.html */ 
{
// Boiler plate html. 
printf("<style id=\"css\">\n");
printf(".node rect,.node ellipse {\n\tstroke: #333;\n\tfill: #fff;\n\tstroke-width: 1.5px;}\n");
printf(".edgePath path {\n\tstroke: #333;\n\tfill: #333;\n\tstroke-width: 1.5px;}\n");

// Calculate a rough SVG box size. Since the table is always very wide we can ignore
// width and focus on height. 
int height = (slCount(gFiles) + slCount(gSteps)) * 100;
printf("</style>\n<svg width=5000 height=%i>\n<g/>\n</svg>\n", height);
dyStringPrintf(dy,"var g = new dagreD3.graphlib.Graph().setGraph({});\n");  

// Print out the nodes in the graph, this includes both file and step nodes. 
printStates(filesPerRow, conn, dy);
// Define the step nodes to be ellipses. 
defineSteps(conn, dy); 
// Link up the nodes and steps. 
linkStepAndFileRows(conn, filesPerRow, dy); 

dyStringPrintf(dy,"\ng.nodes().forEach(function(v) \n{ \n\tvar node = g.node(v);\n\tnode.rx = node.ry = 5; \n}); \n");

// Color the node that was clicked on. 
colorSelectedNode(conn, filesPerRow, fileId, dy); 

dyStringPrintf(dy,"var svg = d3.select(\"svg\"), inner = svg.select(\"g\");\nvar render = new dagreD3.render();\nrender(inner, g);\n");
// Add hyperlinking to the nodes, first build a list of all nodes and find its length.
dyStringPrintf(dy,"var nodeList = d3.select(inner.select(\"g.output\").select(\"g.nodes\").selectAll(\"g.node\"))[0][0][0];\n"); 
dyStringPrintf(dy,"var nodeListLen = d3.select(inner.select(\"g.output\").select(\"g.nodes\").selectAll(\"g.node\"))[0][0][0].length;\n");
// Use a for loop to traverse the list of nodes, for each node attach a hyperlink to the 'on click' function . 
dyStringPrintf(dy,"for (i = 0; i < nodeListLen; i++)\n"); 
dyStringPrintf(dy,"\t\t{\n\t\td3.select(nodeList[i])\n\t\t\t.on(\"click\", function (d)\n\t\t\t\t{\n"); // Define an on click event. 
dyStringPrintf(dy,"\t\t\t\tfileNameParts = d.split(\"\\n\")[0].split(\" \");\n"); // Find the file name (SCH000***) which is used to build the hyperlink. 
dyStringPrintf(dy,"\t\t\t\tfileNameExt = d.split(\".\");"); // Find the file name (SCH000***) which is used to build the hyperlink. 
dyStringPrintf(dy,"\t\t\t\tconsole.log(fileNameParts,fileNameExt);"); // Find the file name (SCH000***) which is used to build the hyperlink. 
dyStringPrintf(dy,"\n\t\t\t\tif (fileNameExt.length > 1)");
dyStringPrintf(dy,"\n\t\t\t\t\t{if (fileNameParts.length == 1)");
dyStringPrintf(dy,"\n\t\t\t\t\t{var fileLink = \"../cgi-bin/cdwWebBrowse?cdwCommand=doFileFlowchart&cdwFileTag=file_name&cdwFileVal=\"+fileNameParts[0]+\"&%s\";}\n", cartSidUrlString(cart)); 
dyStringPrintf(dy,"\n\t\t\t\tif (fileNameParts[0] === \"...\")\n\t\t\t\t\t{");  // This grabs the files that start with '...'. 
dyStringPrintf(dy,"\n\t\t\t\t\tvar fileLink = \"../cgi-bin/cdwWebBrowse?cdwCommand=doFileFlowchart&cdwFileTag=file_name&cdwFileVal=\"+fileNameParts[1]+\"&%s\";\n", cartSidUrlString(cart)); 
dyStringPrintf(dy,"\t\t\t\t\twindow.location.href = fileLink;\n\t\t\t\t\t}\n");  
dyStringPrintf(dy,"\t\t\t\telse{\n"); // This deals with the majority of nodes.  
dyStringPrintf(dy,"\t\t\t\t\tvar fileLink = \"../cgi-bin/cdwWebBrowse?cdwCommand=doFileFlowchart&cdwFileTag=file_name&cdwFileVal=\"+fileNameParts[0]+\"&%s\";\n", cartSidUrlString(cart)); 
dyStringPrintf(dy,"\t\t\t\t\twindow.location.href = fileLink;\n");
dyStringPrintf(dy,"\t\t\t\t\t}}\n\t\t\t\t});\n\t\t}\n\n"); 
dyStringPrintf(dy,"var initialScale = 0.75;"); 
}

struct dyString *makeCdwFlowchart(int fileId, struct cart *cart)
/* Run through SQL tables and generate history flow chart information. */
{
struct sqlConnection *conn = cdwConnect(); 
int filesPerRow = 5; 

// Verify that the file is a valid file (this should happen in all cases, but check anyways). 
char query[1024]; 
sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%i'", fileId);
struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
if (cF == NULL) 
    {
    sqlDisconnect(&conn); 
    return NULL; 
    }

// Fill up the current row return false if their is no flowchart. 
if (baseCase(conn, fileId, filesPerRow) == FALSE)
    return NULL; 
// There is only one row right now
assert(slCount(gFiles) == 1); 
// Look backwards and use an assert to ensure things are going as they should. 
lookBackward(conn, fileId, filesPerRow);
assert(slCount(gFiles) == slCount(gSteps) + 1); 
slReverse(&gSteps);
slReverse(&gFiles);

// Look forwards and use an assert to ensure things are going as they should. 
rLookForward(conn, fileId, slCount(gFiles) + 1, slCount(gSteps) + 1, filesPerRow);
assert(slCount(gFiles) == slCount(gSteps) + 1); 
if (slCount(gFiles) == 1)
    {
    sqlDisconnect(&conn); 
    return NULL; 
    }

slReverse(&gSteps);
slReverse(&gFiles);

// Sort all the file rows; 
struct slRow *iter;
for (iter = gFiles; iter != NULL; iter = iter->next)
    {
    if (iter->fileList != NULL)
	slSort(&iter->fileList, slIntCmp);
    }
for (iter = gSteps; iter != NULL; iter = iter->next)
    {
    if (iter->fileList != NULL)
	slSort(&iter->fileList, slIntCmp);	
    }

// Print everything 
struct dyString *dy = dyStringNew(1024); 
printToDagreD3(conn, fileId, filesPerRow, cart, dy);
sqlDisconnect(&conn); 
return dy;
}

