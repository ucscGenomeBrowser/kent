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

struct slFileRow
// A linked list of slInt's, each slFileRow holds all the file id's for a given row.  
    {
    struct slFileRow *next; 
    struct slInt *fileList; 
    };

void makeFileLink(char *file, struct cart *cart)
/* Write out wrapper that links us to metadata display */
{
printf("<A HREF=\"../cgi-bin/cdwWebBrowse?cdwCommand=oneFile&cdwFileTag=file_name&cdwFileVal=%s&%s\">",
    file, cartSidUrlString(cart));
printf("%s</A>", file);
}

static struct slFileRow *slFileRowNew(struct slInt *fileList)
/* Return a new slFileRow. */
{
struct slFileRow *a;
AllocVar(a);
a->fileList = fileList;
return a;
}


static int findFirstParent(struct sqlConnection *conn, int fileId)
// Return a fileID that corresponds to the first row of the flow chart
{
char query[1024]; 
int result = 0;
// Check if the fileId is in the stepOut table, if it is then start looping back
sqlSafef(query, 1024, "select * from cdwStepOut where fileId = '%i'", fileId);
struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn,query);

// If the file is not in the step out field its either the starting row, or it hasn't
// been linked up yet. 
if (cSO == NULL)
    {
    sqlSafef(query, 1024, "select * from cdwStepIn where fileId = '%i'", fileId);
    struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn,query);   
    if (cSI == NULL) 
	fileId = 0; 
    return fileId; 
    }

while(cSO != NULL)
    {
    // All steps have input files, go find them 
    sqlSafef(query, 1024, "select * from cdwStepIn where stepRunId = '%u'", cSO->stepRunId); 
    struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);
    result = (int) cSI->fileId; 
    // Update the while loop to see if any of these input files are also output files. 
    sqlSafef(query, 1024, "select * from cdwStepOut where fileId = '%u'", cSI->fileId);
    cSO = cdwStepOutLoadByQuery(conn,query);
    }
return result; 
}

static void lookForward(struct sqlConnection *conn, int fileId, struct slFileRow *files, struct slInt *steps)
/* Look forwards through the cdwStep tables until the source is found, store 
 * the file and step rows along the way. Start by looking at cdwStepIn. */ 
{
char query[1024]; 

// Get the cdwStepIn entry for an entry in the first row 
sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId = '%i'", fileId);
struct cdwStepIn *iter, *cSI = cdwStepInLoadByQuery(conn,query);

// Look for siblings 
sqlSafef(query, sizeof(query), "select * from cdwStepIn where stepRunId = '%u'", cSI->stepRunId); 
cSI = cdwStepInLoadByQuery(conn, query); 

// Make the first row 
struct slInt *itemList = NULL;
for (iter = cSI; iter != NULL; iter=iter->next)
    { 
    struct slInt *fileId = slIntNew((int) iter->fileId);
    slAddHead(&itemList, fileId);
    }
struct slFileRow *newRow = slFileRowNew(itemList); 

// Add the first file row to the flowchart
slAddTail(&files, newRow);

while(cSI != NULL)
    {
    struct slInt *newStep = slIntNew(cSI->stepRunId);
    // Add a new step row to the flowchart
    slAddTail(&steps, newStep);

    // All files in cdwStepIn have at least one corresponding file in cdwStepOut keyed into via the stepRun 
    sqlSafef(query, 1024, "select * from cdwStepOut where stepRunId = '%u'", cSI->stepRunId); 
    struct cdwStepOut *iter, *cSOList = cdwStepOutLoadByQuery(conn, query);
    // Go through the list of items and make up a list of slInt's that correpond to the fileId's
    struct slInt *itemList = NULL; 
    for (iter = cSOList; iter != NULL; iter=iter->next)
	{
	struct slInt *fileId = slIntNew((int) iter->fileId);
	slAddHead(&itemList, fileId);	
	}
    // Generate a new file row
    struct slFileRow *newRow = slFileRowNew(itemList);
    // Add the new file row to the flowchart 
    slAddTail(&files, newRow);
	
    // Update the while loop to see if any of these input files are also output files. 
    sqlSafef(query, 1024, "select * from cdwStepIn where fileId = '%u'", cSOList->fileId);
    cSI = cdwStepInLoadByQuery(conn,query);
    }
return; 
}

static void printRowToStep(struct sqlConnection *conn, struct slFileRow *fileRow, struct slInt *stepRow, int filesPerRow)
{
char query[1024];
struct slInt *item;
int count = 0;
// Go through the list of int's that are the file Id's for each row
for (item = fileRow->fileList; item != NULL; item = item->next)
    {
    ++count;
    // Check if the files all share the same stepRow
    // List the last file as a 'n'
    
    
    if(count == filesPerRow)
	{
	sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", item->val);
	struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
	printf("g.setEdge(\"... %s %i\",", cVF->format, slCount(fileRow->fileList)); 
	sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", stepRow->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	printf("\"%s\", ""{label:\"\"});\n", cSD->name);
	break;
	}
    sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId='%u' and stepRunId='%i'", item->val, stepRow->val);
    struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);
    if (cSI==NULL)
	continue;
    

    sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", item->val);
    struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
    printf("g.setEdge(\"%s %i\",", cVF->format, count); 
    sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", stepRow->val);
    struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
    sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
    struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
    printf("\"%s\", ""{label:\"\"});\n", cSD->name);
    }
}

static void printStepToRow(struct sqlConnection *conn, struct slFileRow *fileRow, struct slInt *stepRow, int filesPerRow)
// The difficult case, a step can produce 1 file in the next row, 2 files, or up to n files.  
{
char query[1024]; 
// Get the run tag from stepRow
sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", stepRow->val);
struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
// Use the run tag to get the step definition from stepDef
sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query);

struct slInt *item;
sqlSafef(query, 1024, "select * from cdwStepOut where fileId = '%u'", fileRow->fileList->val);
struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn, query);
int ourStep = cSO->stepRunId, count=0; 
for (item = fileRow->fileList; item != NULL; item = item->next)
    {
    ++count; 
    if (count == filesPerRow)
	{
	if (cSO->stepRunId == ourStep)
	    {
	    sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", fileRow->fileList->val);
	    struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
	    printf("g.setEdge(\"%s\", \"... %s %i\", {label:\"\"});\n", cSD->name, cVF->format, slCount(fileRow->fileList));
	    }
	break;
	}
    sqlSafef(query, 1024, "select * from cdwStepOut where fileId = '%u'", fileRow->fileList->val);
    cSO = cdwStepOutLoadByQuery(conn, query);
    // Check if the file shares the same stepRunId, if it does draw a link to it.  
    if (cSO->stepRunId == ourStep)
	{
	sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", fileRow->fileList->val);
	struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
	printf("g.setEdge(\"%s\", \"%s %i\", {label:\"\"});\n", cSD->name, cVF->format, count);
	}
    }
}

static void printToDagreD3(struct sqlConnection *conn, int fileId, struct slFileRow *files, struct slInt *steps, int filesPerRow)
/* Print out the modular parts of a a dager-d3 javascript visualization
 * These will need to be put into the .js code at the right place. The
 * base flowchart code that is being used was found here; 
 * http://cpettitt.github.io/project/dagre-d3/latest/demo/tcp-state-diagram.html */ 
{
struct slFileRow *fileRow; 
struct slInt *stepRow = steps->next; 
printf("<script src=\"http://cpettitt.github.io/project/dagre-d3/latest/dagre-d3.js\"></script>\n");
printf("<style id=\"css\">\n");
printf(".node rect,.node ellipse {\n\tstroke: #333;\n\tfill: #fff;\n\tstroke-width: 1.5px;}\n");
printf(".edgePath path {\n\tstroke: #333;\n\tfill: #333;\n\tstroke-width: 1.5px;}\n");

// Calculate a rough SVG box size. Since the table is always very wide we can ignore
// width and focus on height. 
int height = (slCount(files) + slCount(steps) - 2)*80; 
printf("</style>\n<svg width=1000 height=%i>\n<g/>\n</svg>\n", height);
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
	    sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", item->val);
	    struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
	    printf("\"... %s %i\"", cVF->format, slCount(fileRow->fileList));
	    break;
	    }
	sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", item->val);
	struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
	printf("\"%s %i\"", cVF->format, count);
	if (item->next != NULL)
	    printf(","); 
	}
    // Add the steps to the graqph, only one step per row is allowed.  
    if (fileRow->next != NULL) 
	{
	sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", stepRow->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
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
for (iter = stepRow; iter != NULL; iter=iter->next)
    {
    sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", iter->val);
    struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
    sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
    struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
    printf("g.setNode(\"%s\", {shape:\"ellipse\"});\n", cSD->name ); 
    }

// Link the nodes up, all the files from a given row are linked to the next row. 
stepRow = steps->next; 
for (fileRow = files->next; fileRow != NULL; fileRow=fileRow->next)
    {
    // Print row to step 
    printRowToStep(conn, fileRow, stepRow, filesPerRow);  
    // Print step to next row 
    if (fileRow->next != NULL) 
	{
	printStepToRow(conn, fileRow->next, stepRow, filesPerRow);
	stepRow = stepRow->next; 
	}
    if (stepRow == NULL) break;
    }
printf("\ng.nodes().forEach(function(v) \n{ \n\tvar node = g.node(v); \n\tnode.rx = node.ry = 5; \n}); \n");

sqlSafef(query, 1024, "select * from cdwValidFile where fileId= '%i'", fileId); 
struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn,query); 
// Color the node that is being selected 
printf("g.node('%s 1').style = \"fill: #7f7\";\n", cVF->format); 

printf("var svg = d3.select(\"svg\"), inner = svg.select(\"g\");\nvar render = new dagreD3.render();\nrender(inner, g);");
printf("var initialScale = 0.75;\n"); 
printf("zoom .translate([(svg.attr(\"width\") - g.graph().width * initialScale) / 2, 20]) .scale(initialScale) .event(svg);\n"); 
printf("svg.attr('height', g.graph().height * initialScale + 40); </script>\n"); 
}


void makeCdwFlowchart(int fileId, struct cart *cart)
/* sqlToTxt - A program that runs through SQL tables and generates history flow chart information. */
{
struct slFileRow *files;  
struct slInt *steps;
AllocVar(files); 
AllocVar(steps); 


struct sqlConnection *conn = cdwConnect(); 

/* Go backwards (in the pipeline from a time/analysis perpsective) until there are no more steps */

char query[1024]; 
sqlSafef(query, 1024, "select * from cdwFile where id = '%i'", fileId);
struct cdwFile *cF = cdwFileLoadByQuery(conn, query);
if (cF == NULL) 
    {
    printf("There is currently no recognized pipeline for this file.\n"); 
    sqlDisconnect(&conn); 
    return; 
    }
// This boolean helps address the base case
int startId = findFirstParent(conn, fileId);

if (startId == 0)
    printf("There is currently no recognized pipeline for this file.\n"); 
    sqlDisconnect(&conn); 
    return;

/* Go forwards until there are no more steps */ 
lookForward(conn, startId, files, steps);
if (slCount(files) == 1 || slCount(steps) == 1)
    {
    printf("There is currently no recognized pipeline for this file.\n");
    sqlDisconnect(&conn); 
    return; 
    }
assert(slCount(files) == slCount(steps) + 1); 
printToDagreD3(conn, fileId, files, steps, 5);
sqlDisconnect(&conn); 
}

