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
#include "cheapcgi.h"

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
/* Return everything past the last '/' as a string. */ 
{
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
slAddTail(&gFiles, fileRow); 
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
    slAddTail(&gSteps, newStep);

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
    slAddTail(&gFiles, newRow);
	
    // Update the while loop to see if any of these input files are also output files. 
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId = '%u'", cSIList->fileId);
    cSO = cdwStepOutLoadByQuery(conn,query);
    }
// Reverse things.  
slReverse(gFiles);
slReverse(gSteps);
return; 
}

static void addToSteps(int depth, struct slInt *stepRow)
/* Add a single step to the global list of steps at the propper depth. */ 
{

// Check if there is a row at the current depth; 
if (slCount(gSteps) < depth) // Make a new row
    {
    slAddTail(&gSteps, slRowNew(stepRow)); 
    return;
    }
// Go to the propper depth in the slRow
struct slRow *existingRow = slElementFromIx(&gSteps, depth);
slAddTail(&existingRow->fileList, stepRow); 
}

static void addToFiles(int depth, struct slInt *fileRow)
// Add a list with  multiple files to the global files list at the propper depth.  
{
if (slCount(gFiles) < depth) // Make a new row
    {
    slAddTail(&gFiles, slRowNew(fileRow)); 
    return;
    }
// Go to the propper depth in the slRow
struct slRow *existingRow = slElementFromIx(&gFiles, depth);
struct slInt *iter, *iter2; 
for (iter=fileRow; iter != NULL; )
    {
    iter2 = iter->next; 
    slAddHead(&existingRow->fileList, iter);
    iter = iter2; 
    }
slReverse(&existingRow);
return; 
}

static void rLookForward(struct sqlConnection *conn, int fileId, int depth, int filesPerRow)
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
    addToSteps(depth, slIntNew(curStep->stepRunId));
    
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
    addToFiles(depth + 1, fileIdList); 
    // Each recursion has a step and file row, hence increase depth by 2. 
    rLookForward(conn, fileIdList->val, depth+2, filesPerRow); 
    }
depth=0; 
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

static void printFileRowToAnalysisRow(struct sqlConnection *conn, struct dyString *dy, struct slInt *fileRow, struct slInt *stepRow, int filesPerRow)
/* Take an analysis row and connect it to the output files, there may be multiple analysis steps in the analysis row. */
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

	// Handle overflow. 
	if (fileCount  == filesPerRow)
	    {
	    dyStringPrintf(dy,"g.setEdge(\"... %s %i\",", stripFilePath(cF->cdwFileName), getSiblingCount(conn, fileIter->val));
	    dyStringPrintf(dy,"\"%s\", ""{label:\"\"});\n", cSD->name);	
	    break;
	    }	
	dyStringPrintf(dy,"g.setEdge(\"%s\",", stripFilePath(cF->cdwFileName)); // The file. 
	dyStringPrintf(dy,"\"%s\", ""{label:\"\"});\n", cSD->name);  // The step. 
	}
    }
}

static void printAnalysisStepToFileRow(struct sqlConnection *conn, struct dyString *dy, struct slInt *fileRow, int stepId, int filesPerRow)
// For a single analysis step, identify the output files and write the links from the analysis step to the output files. 
{
char query[1024]; 

// Get the a name to use for the step, it lies in cdwStepDef, indexed through cdwStepRun.
sqlSafef(query, sizeof(query), "select * from cdwStepRun where id = '%i'", stepId);
struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
sqlSafef(query, sizeof(query), "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); // cSD->name is the analysis step name (ie Kallisto). 

int count = 0;
struct slInt *file;
slSort(fileRow, slIntCmp); 
// Go through the file row and identify the files that were created by the current analysis step.  
for (file = fileRow; file != NULL; file = file->next)
    {
    ++count;
    sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId='%i'", file->val); 
    struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn, query); 
    if (cSO->stepRunId != stepId)
	continue;
    // All files below are generated by the current analysis step.  
    
    sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", file->val);
    struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
    
    if (count == filesPerRow)
	{
	dyStringPrintf(dy,"g.setEdge(\"%s\", \"... %s %i\", {label:\"\"});\n", cSD->name, stripFilePath(cF->cdwFileName), getSiblingCount(conn, file->val));
	break;
	}

    dyStringPrintf(dy,"g.setEdge(\"%s\", \"%s\", {label:\"\"});\n", cSD->name, stripFilePath(cF->cdwFileName));
    }
}


static void printStates(int filesPerRow, struct sqlConnection *conn, struct dyString *dy)
{
// Define all the nodes, this sets the visual node name
dyStringPrintf(dy,"var states = [");
char query[1024];
struct slRow *fileRow, *stepRow=gSteps->next;
// For each file in the file row generate a box for it.
// Go through the files and rows level by level 
struct slInt *seenFiles = NULL; 
for (fileRow = gFiles->next; fileRow != NULL; fileRow=fileRow->next)
    {
    struct slInt *step; 
    // each step can have up to 'filesPerRow' files in it. 
    // Go through each step row
    for (step = stepRow->fileList; step != NULL; step = step->next)
	{
	// Step is the stepRunId value, ignore all the files that don't share it.  
	struct slInt *item;
	int inCount = 0, outCount = 0;
	// Go through the files in a specific level, each stepId has input and output files, handle them accordingly. 
	for (item = fileRow->fileList; item != NULL; item = item->next)
	    {   
	    sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%i' and stepRunId = '%i'", item->val, step->val); 
	    struct cdwStepIn *cSI2 = cdwStepInLoadByQuery(conn, query);
	    if (cSI2 != NULL)//An input file, only allow up to five.  
		{
		if (cSI2->stepRunId != step->val) continue;
		if (slIntFind(seenFiles, item->val) != NULL) continue; // If we have already seen this file ignore it.  
		struct slInt *fileId = slIntNew((int) item->val); // Otherwise add it to the list of seen files. 
		slAddHead(&seenFiles, fileId);	
		++inCount; 
		sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", item->val);
		struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
		if (inCount == filesPerRow)
		    {
		    dyStringPrintf(dy,"\"... %s %i\",", stripFilePath(cF->cdwFileName), getSiblingCount(conn, item->val));
		    break;
		    }
		dyStringPrintf(dy,"\"%s\",", stripFilePath(cF->cdwFileName));
		}
	    sqlSafef(query, sizeof(query), "select * from cdwStepOut where fileId='%i' and stepRunId = '%i'", item->val, step->val); 
	    struct cdwStepOut *cSO2 = cdwStepOutLoadByQuery(conn, query); 
	    if (cSO2 != NULL) //Output files. 
		{
		if (cSO2->stepRunId != step->val) continue; 
		if (slIntFind(seenFiles, item->val) != NULL) continue; // If we have already seen this file ignore it.  
		struct slInt *fileId = slIntNew((int) item->val); // Otherwise add it to the list of seen files. 
		slAddHead(&seenFiles, fileId);	
		++outCount; 
		sqlSafef(query, sizeof(query), "select * from cdwFile where id = '%u'", item->val);
		struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
		if (outCount == filesPerRow)
		    {
		    dyStringPrintf(dy,"\"... %s %i\",", stripFilePath(cF->cdwFileName), getSiblingCount(conn, item->val));
		    break;
		    }
		dyStringPrintf(dy,"\"%s\",", stripFilePath(cF->cdwFileName));
		
		}
	    }
	}
    if (stepRow->next !=NULL)
	stepRow = stepRow->next;
    }
// Print out the analysis steps.
stepRow = gSteps->next; 
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

static void printToDagreD3(struct sqlConnection *conn, int fileId, struct slRow *files, struct slRow *gSteps, int filesPerRow, struct cart *cart)
/* Print out the modular parts of a a dager-d3 javascript visualization
 * These will need to be put into the .js code at the right place. The
 * base flowchart code that is being used was found here; 
 * http://cpettitt.github.io/project/dagre-d3/latest/demo/tcp-state-diagram.html */ 
{
struct slRow *fileRow; 
struct slRow *stepRow = gSteps->next;
char query[1024];
printf("<script src=\"http://cpettitt.github.io/project/dagre-d3/latest/dagre-d3.js\"></script>\n");
printf("<style id=\"css\">\n");
printf(".node rect,.node ellipse {\n\tstroke: #333;\n\tfill: #fff;\n\tstroke-width: 1.5px;}\n");
printf(".edgePath path {\n\tstroke: #333;\n\tfill: #333;\n\tstroke-width: 1.5px;}\n");

// Calculate a rough SVG box size. Since the table is always very wide we can ignore
// width and focus on height. 
int height = (slCount(files) + slCount(gSteps) - 2) * 80;
printf("</style>\n<svg width=5000 height=%i>\n<g/>\n</svg>\n", height);

struct dyString *dy = dyStringNew(4096);
dyStringPrintf(dy,"var g = new dagreD3.graphlib.Graph().setGraph({});\n");  

printStates(filesPerRow, conn, dy);

// Define the step's as 'ellipses' so they look different, css definition of elipse is above. 
stepRow = gSteps->next; 
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

stepRow = gSteps->next; 
for (fileRow = files->next; fileRow != NULL; fileRow=fileRow->next)
    {
    // Print links from a file row to an analysis row, there may be multiple unique analysis pipelines
    printFileRowToAnalysisRow(conn, dy, fileRow->fileList, stepRow->fileList, filesPerRow);  
    if (fileRow->next != NULL) // If there is another fileRow there is another step row 
	{
	struct slInt *iter; 
	for (iter = stepRow->fileList; iter != NULL; iter = iter->next)
	    {
	    // Print step to next row 
	    printAnalysisStepToFileRow(conn, dy, fileRow->next->fileList, iter->val, filesPerRow);
	    }
	stepRow = stepRow->next; 
	}
	
    if (stepRow == NULL) break;
    }
dyStringPrintf(dy,"\ng.nodes().forEach(function(v) \n{ \n\tvar node = g.node(v); \n\tnode.rx = node.ry = 5; \n}); \n");


/* Color the node that is being selected */ 

sqlSafef(query, sizeof(query), "select * from cdwFile where id= '%i'", fileId); 
struct cdwFile *cF = cdwFileLoadByQuery(conn,query);
if (gLocation > 4 )
    {
    dyStringPrintf(dy,"g.node('... %s %i').style = \"fill: #7f7\";\n", stripFilePath(cF->cdwFileName), getSiblingCount(conn, fileId)); 
    }
else
    dyStringPrintf(dy,"g.node('%s').style = \"fill: #7f7\";\n", stripFilePath(cF->cdwFileName)); 
  

dyStringPrintf(dy,"var svg = d3.select(\"svg\"), inner = svg.select(\"g\");\nvar render = new dagreD3.render();\nrender(inner, g);\n");
// Add hyperlinking to the nodes, first build a list of all nodes and find its length.
dyStringPrintf(dy,"var nodeList = d3.select(inner.select(\"g.output\").select(\"g.nodes\").selectAll(\"g.node\"))[0][0][0];\n"); 
dyStringPrintf(dy,"var nodeListLen = d3.select(inner.select(\"g.output\").select(\"g.nodes\").selectAll(\"g.node\"))[0][0][0].length;\n");
// Use a for loop to traverse the list of nodes, for each node attach a hyperlink to the 'on click' function . 
dyStringPrintf(dy,"for (i = 0; i < nodeListLen; i++)\n"); 
dyStringPrintf(dy,"\t\t{\n\t\td3.select(nodeList[i])\n\t\t\t.on(\"click\", function (d)\n\t\t\t\t{\n"); // Define an on click event. 
dyStringPrintf(dy,"\t\t\t\tfileNameParts = d.split(\" \");\n"); // Find the file name (SCH000***) which is used to build the hyperlink. 
dyStringPrintf(dy,"\t\t\t\tfileNameExt = d.split(\".\");"); // Find the file name (SCH000***) which is used to build the hyperlink. 
dyStringPrintf(dy,"\n\t\t\t\tif (fileNameExt.length > 1)");
dyStringPrintf(dy,"\n\t\t\t\t\t{if (fileNameParts.length == 1)");
dyStringPrintf(dy,"\n\t\t\t\t\t{var fileLink = \"../cgi-bin/cdwWebBrowse?cdwCommand=doFileFlowchart&cdwFileTag=file_name&cdwFileVal=\"+fileNameParts[0]+\"&%s\";}\n", cartSidUrlString(cart)); 
//dyStringPrintf(dy,"\n\t\t\t\t\t{return;}"); // This grabs the step nodes and returns before applying a bad hyperlink. 
dyStringPrintf(dy,"\n\t\t\t\tif (fileNameParts[0] === \"...\")\n\t\t\t\t\t{");  // This grabs the files that start with '...'. 
dyStringPrintf(dy,"\n\t\t\t\t\tvar fileLink = \"../cgi-bin/cdwWebBrowse?cdwCommand=doFileFlowchart&cdwFileTag=file_name&cdwFileVal=\"+fileNameParts[1]+\"&%s\";\n", cartSidUrlString(cart)); 
dyStringPrintf(dy,"\t\t\t\t\twindow.location.href = fileLink;\n\t\t\t\t\t}\n");  
dyStringPrintf(dy,"\t\t\t\telse{\n"); // This deals with the majority of nodes.  
dyStringPrintf(dy,"\t\t\t\t\tvar fileLink = \"../cgi-bin/cdwWebBrowse?cdwCommand=doFileFlowchart&cdwFileTag=file_name&cdwFileVal=\"+fileNameParts[0]+\"&%s\";\n", cartSidUrlString(cart)); 
dyStringPrintf(dy,"\t\t\t\t\twindow.location.href = fileLink;\n");
dyStringPrintf(dy,"\t\t\t\t\t}}\n\t\t\t\t});\n\t\t}\n\n"); 
dyStringPrintf(dy,"var initialScale = 0.75;</script>\n"); 
jsInline(dy->string);
dyStringFree(&dy);
}

void makeCdwFlowchart(int fileId, struct cart *cart)
/* Run through SQL tables and generate history flow chart information. */
{
AllocVar(gFiles); 
AllocVar(gSteps); 
struct sqlConnection *conn = cdwConnect(); 
int filesPerRow = 5; 

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
if (baseCase(conn, fileId, filesPerRow) == FALSE)
    return; 

// Look backwards and use an assert to ensure things are going as they should. 
lookBackward(conn, fileId, filesPerRow);
assert(slCount(gFiles) == slCount(gSteps) + 1); 


// Look forwards and use an assert to ensure things are going as they should. 
rLookForward(conn, fileId, slCount(gSteps) + slCount(gFiles) -1, filesPerRow);
assert(slCount(gFiles) == slCount(gSteps) + 1); 

if (slCount(gFiles) == 1 || slCount(gSteps) == 1)
    {
    sqlDisconnect(&conn); 
    return; 
    }

// Sort all the file rows; 
struct slRow *iter;
for (iter = gFiles; iter != NULL; iter = iter->next)
    {
    if (iter->fileList != NULL)
	{
	slSort(&iter->fileList, slIntCmp);
	}
    }
for (iter = gSteps; iter != NULL; iter = iter->next)
    {
    if (iter->fileList != NULL)
	{
	slSort(&iter->fileList, slIntCmp);	
	}
    }

// Print everything 
printToDagreD3(conn, fileId, gFiles, gSteps, filesPerRow, cart);
sqlDisconnect(&conn); 
}

