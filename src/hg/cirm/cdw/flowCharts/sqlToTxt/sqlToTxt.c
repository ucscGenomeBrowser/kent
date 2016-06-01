/* sqlToTxt - A program that runs through SQL tables and generates history flow chart information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwStep.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "sqlToTxt - A program that runs through SQL tables and generates history flow chart information\n"
  "usage:\n"
  "   sqlToTxt fileId output\n"
  "options:\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct slFileRow
    {
    struct slFileRow *next; 
    unsigned fileId1; 
    unsigned fileId2; 
    bool moreThanTwo; 
    };

struct slFileRow *slFileRowNew(unsigned fileId1, unsigned fileId2, bool moreThanTwo)
/* Return a new slFileRow. */
{
struct slFileRow *a;
AllocVar(a);
a->fileId1 = fileId1;
a->fileId2 = fileId2;
a->moreThanTwo = moreThanTwo; 
return a;
}

bool lookBackward(char *fileId, struct slFileRow *files, struct slInt *steps)
/* Look backwards through the cdwStep tables until the source is found, store 
 * the file and step rows along the way */ 
{
bool findAnything = TRUE; 
struct sqlConnection *conn = sqlConnect("cdw_chris"); 
char query[1024]; 

// Check if the fileId is in the stepOut table, if it is then start looping back
sqlSafef(query, 1024, "select * from cdwStepOut where fileId = '%s'", fileId);
struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn,query);
if (cSO != NULL)  // Base case
    {
    // Check for siblings 
    sqlSafef(query, 1024, "select * from cdwStepOut where stepRunId = '%u'", cSO->stepRunId); 
    struct cdwStepOut *cSOList = cdwStepOutLoadByQuery(conn,query);
    unsigned siblingId = 0; 
    bool moreThanOneSibling = FALSE; 
    struct cdwStepOut *iter;
    for (iter = cSOList; iter != NULL; iter=iter->next)
	{
	if (iter->fileId != cSO->fileId)
	    {
	    siblingId = iter->fileId; 
	    }
	}
    if (slCount(cSOList) > 2)
	moreThanOneSibling = TRUE;
    struct slFileRow *newRow = slFileRowNew(atoi(fileId), siblingId, moreThanOneSibling); 
    slAddTail(&files, newRow);
    findAnything = FALSE; 
    }

while(cSO != NULL)
    {
    // Add the step to the list of steps 
    struct slInt *newStep = slIntNew(cSO->stepRunId);
    slAddTail(&steps, newStep);
    
    // All steps have input files, go find them 
    sqlSafef(query, 1024, "select * from cdwStepIn where stepRunId = '%u'", cSO->stepRunId); 
    struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn, query);

    // To keep the graphs simple only the first file is further analyzed, the 
    // existance of one, or possibly N more is determined and stored for future use
    unsigned fileId2;
    bool moreThanTwo = FALSE; 
    if (cSI->next != NULL)
	{
	if (cSI->next->next != NULL)
	    moreThanTwo = TRUE;
	fileId2 = cSI->next->fileId;
	}
    else 
	fileId2 = 0;
    struct slFileRow *newRow = slFileRowNew(cSI->fileId, fileId2, moreThanTwo); 
    slAddTail(&files, newRow);  
	
    // Update the while loop to see if any of these input files are also output files. 
    sqlSafef(query, 1024, "select * from cdwStepOut where fileId = '%u'", cSI->fileId);
    cSO = cdwStepOutLoadByQuery(conn,query);
    }
sqlDisconnect(&conn); 
slReverse(files);
slReverse(steps);
return findAnything; 
}

void lookForward(char *fileId, struct slFileRow *files, struct slInt *steps, bool upstream)
/* Look backwards through the cdwStep tables until the source is found, store 
 * the file and step rows along the way */ 
{
struct sqlConnection *conn = sqlConnect("cdw_chris"); 
char query[1024]; 

// Check if the fileId is in the stepOut table, if it is then start looping back
sqlSafef(query, 1024, "select * from cdwStepIn where fileId = '%s'", fileId);
struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn,query);

if (cSI != NULL && upstream)
    {
    // Check for siblings 
    sqlSafef(query, 1024, "select * from cdwStepIn where stepRunId = '%u'", cSI->stepRunId); 
    struct cdwStepIn *cSIList = cdwStepInLoadByQuery(conn,query);
    unsigned siblingId = 0; 
    bool moreThanOneSibling = FALSE; 
    struct cdwStepIn *iter;
    for (iter = cSIList; iter != NULL; iter=iter->next)
	{
	if (iter->fileId != cSI->fileId)
	    {
	    siblingId = iter->fileId; 
	    }
	}
    if (slCount(cSIList) > 2)
	moreThanOneSibling = TRUE;
    struct slFileRow *newRow = slFileRowNew(atoi(fileId), siblingId, moreThanOneSibling); 
    slAddTail(&files, newRow);
    }

while(cSI != NULL)
    {
    // Add the step to the list of steps 
    struct slInt *newStep = slIntNew(cSI->stepRunId);
    slAddTail(&steps, newStep);
    
    // All steps have input files, go find them 
    sqlSafef(query, 1024, "select * from cdwStepOut where stepRunId = '%u'", cSI->stepRunId); 
    struct cdwStepOut *cSO = cdwStepOutLoadByQuery(conn, query);

    // To keep the graphs simple only the first file is further analyzed, the 
    // existance of one, or possibly N more is determined and stored for future use
    unsigned fileId2;
    bool moreThanTwo = FALSE; 
    if (cSO->next != NULL)
	{
	if (cSO->next->next != NULL)
	    moreThanTwo = TRUE;
	fileId2 = cSO->next->fileId;
	}
    else 
	fileId2 = 0;
    struct slFileRow *newRow = slFileRowNew(cSO->fileId, fileId2, moreThanTwo); 
    slAddTail(&files, newRow);  
	
    // Update the while loop to see if any of these input files are also output files. 
    sqlSafef(query, 1024, "select * from cdwStepIn where fileId = '%u'", cSO->fileId);
    cSI = cdwStepInLoadByQuery(conn,query);
    }
sqlDisconnect(&conn); 
return; 
}


void printToStrictJson(FILE *f, struct slFileRow *files, struct slInt *steps)
/* Print the files to .json format.  This prints a valid .json file
 * However it is difficult to actually make a flowchart from it! 
 * Mostly due to the complexity of having a node with multiple parents.*/
{
struct slFileRow *fileRow; 
struct slInt *stepRow = steps->next;  
fprintf(f,"[");
for (fileRow = files->next; fileRow != NULL; fileRow=fileRow->next)
    {
    if (fileRow->fileId2 != 0)
	{
	fprintf(f,"{\"name\":\"%u\"},", fileRow->fileId2);
	}
    if (fileRow->moreThanTwo)
	{
	fprintf(f,"{\"name\":\"%i\"},",  fileRow->moreThanTwo); 
	}
    fprintf(f,"{\"name\":\"%u\"", fileRow->fileId1);
    if (fileRow->next != NULL) 
	fprintf(f,",\"children\":[{\"name\":\"%i\",\"children\":[", stepRow->val); 
    }
int iter; 
for (iter = 0; iter < slCount(files) + 1 ; ++iter)
    {
    fprintf(f,"}]");     
    }
fprintf(f,"\n");
}
    

void printRowToStep(FILE *f, struct slFileRow *fileRow, struct slInt *stepRow)
{
struct sqlConnection *conn = sqlConnect("cdw_chris"); 
char query[1024]; 
// The main branch file 
sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", fileRow->fileId1);
struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
fprintf(f,"g.setEdge(\"%s 1\",", cVF->format); 

// The main branch step 
sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", stepRow->val);
struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
fprintf(f,"\"%s\", ""{label:\"\"});\n", cSD->name);
if (fileRow->fileId2 != 0) // Any siblings?  
    {
    sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", fileRow->fileId2);
    cVF = cdwValidFileLoadByQuery(conn, query);
    fprintf(f,"g.setEdge(\"%s 2\", \"%s\", {label:\"\"} );\n", cVF->format, cSD->name);
    }
if (fileRow->moreThanTwo) // Too many siblings, put a placeholder instead.  
    {
    fprintf(f,"g.setEdge(\"%s n\", \"%s\", {label:\"\"} );\n", cVF->format, cSD->name);
    }
sqlDisconnect(&conn);
}

void printStepToRow(FILE *f, struct slFileRow *fileRow, struct slInt *stepRow)
{
struct sqlConnection *conn = sqlConnect("cdw_chris"); 
char query[1024]; 
sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", stepRow->val);
struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", fileRow->next->fileId1);
struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
fprintf(f,"g.setEdge(\"%s\", \"%s 1\", {label:\"\"});\n", cSD->name, cVF->format);

// Check if the fileId is in the stepOut table, if it is then start looping back
sqlSafef(query, 1024, "select * from cdwStepOut where fileId = '%u'", fileRow->next->fileId1);

struct cdwStepOut *cSOStepRuns = cdwStepOutLoadByQuery(conn, query);
sqlSafef(query, 1024, "select * from cdwStepOut where stepRunId = '%u'", cSOStepRuns->stepRunId);


struct cdwStepOut *cSOIter, *cSOList = cdwStepOutLoadByQuery(conn, query);

for (cSOIter = cSOList; cSOIter != NULL; cSOIter=cSOIter->next)
    {
    if (cSOIter->fileId == fileRow->next->fileId2)
	fprintf(f,"g.setEdge(\"%s\", \"%s 2\", {label:\"\"});\n", cSD->name, cVF->format);
    }
sqlDisconnect(&conn);
}

void printToDagreD3(FILE *f, char *fileId, struct slFileRow *files, struct slInt *steps)
/* Print out the modular parts of a a dager-d3 javascript visualization
 * These will need to be put into the .js code at the right place. The
 * base flowchart code that is being used was found here; 
 * http://cpettitt.github.io/project/dagre-d3/latest/demo/tcp-state-diagram.html */ 
{
struct slFileRow *fileRow; 
struct slInt *stepRow = steps->next;  
fprintf(f,"<!doctype html>  <meta charset=\"utf-8\"> <title>file flowchart</title>  <link rel=\"stylesheet\" href=\"demo.css\"> <script src=\"http://d3js.org/d3.v3.min.js\" charset=\"utf-8\"></script> <script src=\"http://cpettitt.github.io/project/dagre-d3/latest/dagre-d3.js\"></script>  <style id=\"css\"> body { font: 300 14px 'Helvetica Neue', Helvetica; } "); 
fprintf(f,".node rect,.node ellipse {stroke: #333;fill: #fff;stroke-width: 1.5px;}"); // Node appearances defined via CSS. 
fprintf(f,"  .edgePath path { stroke: #333; fill: #333; stroke-width: 1.5px; } </style> <svg width=600 height=800><g/>"); 
fprintf(f,"</svg>  <script id=\"js\"> var g = new dagreD3.graphlib.Graph().setGraph({});");  
// Define all the nodes, this sets the visual node name
fprintf(f,"var states = [");
struct sqlConnection *conn = sqlConnect("cdw_chris"); 
char query[1024]; 
for (fileRow = files->next; fileRow != NULL; fileRow=fileRow->next)
    {
    sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%u'", fileRow->fileId1);
    struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
    fprintf(f,"\"%s 1\"", cVF->format);
    if (fileRow->fileId2 != 0)
	{
	fprintf(f,",\"%s 2\"", cVF->format);
	}
    if (fileRow->moreThanTwo)
	{
	fprintf(f,",\"%s n\"", cVF->format); 
	}
    if (fileRow->next != NULL) 
	{
	sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", stepRow->val);
	struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
	sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
	struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
	fprintf(f,",\"%s\",", cSD->name); 
	stepRow = stepRow->next; 
	}
    }
fprintf(f,"]\n"); 
// Link the nodes up 
fprintf(f,"states.forEach(function(state) { g.setNode(state, { label: state }); });");
stepRow = steps->next; 
struct slInt *iter; 
// Make the step's ellipses, the actual appearance is defined with CSS above. 
for (iter = stepRow; iter != NULL; iter=iter->next)
    {
    sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i'", iter->val);
    struct cdwStepRun *cSR = cdwStepRunLoadByQuery(conn,query); 
    sqlSafef(query, 1024, "select * from cdwStepDef where id = '%i'", cSR->stepDef); 
    struct cdwStepDef *cSD = cdwStepDefLoadByQuery(conn,query); 
    fprintf(f, "g.setNode(\"%s\", {shape:\"ellipse\"});", cSD->name ); 
    }
stepRow = steps->next; 
for (fileRow = files->next; fileRow != NULL; fileRow=fileRow->next)
    {
    // Print row to step 
    printRowToStep(f, fileRow, stepRow);  
    // Print step to next row 
    if (fileRow->next != NULL) 
	{
	printStepToRow(f, fileRow, stepRow);
	stepRow = stepRow->next; 
	}
    if (stepRow == NULL) break;
    }
fprintf(f,"g.nodes().forEach(function(v) { var node = g.node(v); node.rx = node.ry = 5; }); ");

sqlSafef(query, 1024, "select * from cdwValidFile where id = '%s'", fileId); 
struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn,query); 
// Color the node that is being selected 
fprintf(f,"g.node('%s 1').style = \"fill: #7f7\";", cVF->format); 

fprintf(f,"var svg = d3.select(\"svg\"), inner = svg.select(\"g\");   var render = new dagreD3.render();  render(inner, g);  var initialScale = 0.75; zoom .translate([(svg.attr(\"width\") - g.graph().width * initialScale) / 2, 20]) .scale(initialScale) .event(svg)"); 
fprintf(f,"; svg.attr('height', g.graph().height * initialScale + 40); </script>\n"); 
sqlDisconnect(&conn);
}


void sqlToTxt(char *fileId, char *outputFile)
/* sqlToTxt - A program that runs through SQL tables and generates history flow chart information. */
{
struct slFileRow *files;  
struct slInt *steps;
AllocVar(files); 
AllocVar(steps); 

/* Go backwards (in the pipeline from a time/analysis perpsective) until there are no more steps */

struct sqlConnection *conn = sqlConnect("cdw_chris"); 
char query[1024]; 
sqlSafef(query, 1024, "select * from cdwValidFile where id = '%s'", fileId);
struct cdwValidFile *cVF = cdwValidFileLoadByQuery(conn, query);
sqlDisconnect(&conn); 
if (cVF == NULL) 
    errAbort("There is no entry in cdwValidFile for the fileId %s.", fileId); 
// This boolean addresses the base case
bool upstream = lookBackward(fileId, files, steps);

/* Go forwards until there are no more steps */ 
lookForward(fileId, files, steps, upstream);
if (slCount(files) == 1 || slCount(steps) == 1)
    errAbort("There is no entry in either cdwStepIn or cdwStep Out for the fileId %s. Use the script maniMani to populate these tables.", fileId); 

assert(slCount(files) == slCount(steps) + 1); 

/* Iterate over steps and files printing to some output format (json/newick) */ 

// TODO If statement for printing, throw in a flag so the user can choose. 
///fprintf(f,"\n"); 
FILE *f = mustOpen(outputFile,"w");
//printToStrictJson(f, files, steps);
printToDagreD3(f, fileId, files, steps); 
carefulClose(&f); 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
sqlToTxt(argv[1], argv[2]);
return 0;
}
