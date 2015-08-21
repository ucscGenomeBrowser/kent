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
  "   -forceLayout = The output is a D3 forcelayout .json. \n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"forceLayout", OPTION_BOOLEAN},
   {NULL, 0},
};

bool clForceLayout = FALSE; // Prints the data in .json format for d3 force layout visualizations
int totalNodes = 0;
int gStart = 0; 
int gKey = 0;
int gX = 200; 
int gY = 200; 
struct jsonNode *nodeList;
struct jsonLink *linkList;

struct jsonLink
    {
    struct jsonLink *next; 
    char *text;
    int start;
    int end; 
    };

struct jsonNode
    {
    struct jsonNode *next;
    char *text; 
    int key; 
    bool diamond;
    int xloc; 
    int yloc; 
    };

    
struct jsonNode *newJsonNode(char *text, int key, bool diamond, int xloc, int yloc)
{
struct jsonNode *newNode; 
AllocVar(newNode); 
newNode->text=cloneString(text); 
newNode->key=key; 
newNode->diamond=diamond; 
newNode->xloc=xloc; 
newNode->yloc=yloc;
return newNode; 
}

struct jsonLink *newJsonLink(char *text, int start, int end)
{
struct jsonLink *newLink; 
AllocVar(newLink); 
newLink->text=cloneString(text); 
newLink->start=start; 
newLink->end=end;
return newLink; 
}



void printToForceLayoutJson(FILE *f)
{
// Print to D3 forceLayout format. 
int normalizeX = 200;
int currentLevel = 0;
bool firstLine = true; 
struct jsonNode *iterN;
slReverse(&nodeList);
fprintf(f,"{\n\t\"nodes\":[\n"); 
for (iterN = nodeList; iterN -> next!=NULL; iterN = iterN->next)
    {
    if (firstLine)
	{
	currentLevel = iterN->yloc/200; 
	firstLine = false; 
	}
    int updatedXloc = iterN->xloc;
    if (iterN->xloc > normalizeX) normalizeX=iterN->xloc; 
    if (iterN->yloc/200 != currentLevel) 
	updatedXloc = normalizeX/2; 
	currentLevel = iterN->yloc/200; 
    fprintf(f,"\t{\"name\":\"%s\",\"group\":%i}", iterN->text, currentLevel);     
    if (iterN->next->next !=NULL) fprintf(f,",");
    fprintf(f,"\n"); 
    }
fprintf(f,"],\n\t\"links\":[\n"); 
struct jsonLink *iterL; 
for (iterL = linkList; iterL->next !=NULL; iterL = iterL->next)
    {
    fprintf(f,"\t{\"source\":%i,\"target\":%i,\"value\":%i}", iterL->start, iterL->end, 1);//iterL->text); 
    if (iterL->next->next != NULL) fprintf(f,",");
    fprintf(f,"\n"); 
    }
fprintf(f,"]}\n"); 
}


void printToGoJson(FILE *f)
{
/* Print a list of jsonNodes and jsonLinks to go.js format. Will likely get things working here then jump ship to a more
 * free option */ 
/*
int normalizeX = 200;
int currentLevel = 0;
bool firstLine = true; 
int t1=0, t2=0;
*/
struct jsonNode *iterN;
fprintf(f,"{ \"class\":\"go.GraphLinksModel\",\"linkFromPortIdProperty\":\"fromPort\",\"linkToPortIdProperty\": \"toPort\",\"nodeDataArray\":[\n");
for (iterN = nodeList; iterN->next !=NULL; iterN = iterN->next)
    {
    /*
    if (firstLine)
	{
	currentLevel = iterN->yloc/200; 
	firstLine = false; 
	}
    int updatedXloc = iterN->xloc;

    t1 += iterN->yloc; 
    ++t2; 
    if (iterN->xloc > normalizeX) normalizeX=iterN->xloc; 
    if (iterN->yloc/200 != currentLevel) 
	updatedXloc = t1/t2;
	t2 =0;
	t1 = 0; 
	currentLevel = iterN->yloc/200; 
    */
    fprintf(f,"{\"text\":\"%s\",\"key\":\"%i\",\"loc\":\"%i %i\"}",iterN->text, iterN->key, iterN->xloc, iterN->yloc);
    if (iterN->next->next !=NULL) fprintf(f,",");
    fprintf(f,"\n"); 
    }
fprintf(f,"],\n\"linkDataArray\":[\n"); 
struct jsonLink *iterL; 
for (iterL = linkList; iterL->next !=NULL; iterL = iterL->next)
    {
    fprintf(f,"{\"from\":%i,\"to\":%i,\"fromPort\":\"B\", \"toPort\":\"T\", \"visible\":true, \"text\":\"%s\"}",iterL->start, iterL->end, iterL->text);
    if (iterL->next->next != NULL) fprintf(f,",");
    fprintf(f,"\n"); 
    }
fprintf(f,"]}\n"); 
}



void addTransitionNode(int stepRunId, int yPos)
/* takes in stepRunId and adds a node to the linked list. The nodes
 * name is from the cdwStepDef table */ 
{
// Take stepRunId and map it to id in cdwStepRun, this gives a stepDefId
struct sqlConnection *conn = sqlConnect("cdw"); 
char query[1024]; 
sqlSafef(query, 1024, "select * from cdwStepRun where id = '%i';", stepRunId);
struct cdwStepRun *out = cdwStepRunLoadByQuery(conn, query);
sqlDisconnect(&conn);

/* Take stepDefId and map it to id in cdwStepDef, this gives a name which is used 
 * to label a node */ 
struct sqlConnection *conn1 = sqlConnect("cdw"); 
char query1[1024]; 
sqlSafef(query1, 1024, "select * from cdwStepDef where id = '%i';", out->stepDef);
struct cdwStepDef *out1 = cdwStepDefLoadByQuery(conn1, query1);
sqlDisconnect(&conn1);

gY+=200; // Move the node down the page 
struct jsonNode *middleNode = newJsonNode(out1->name, totalNodes, false, gX, yPos);
slAddHead(&nodeList, middleNode); // add the node to the list
++totalNodes; // Add one to the total node count (used for links)
}

void addEndNode(int fileId, int yPos)
/* Takes in a fileId and adds a node to the linked list. The nodes name 
 * is from the cdwValidFile table.  The yPos is updated locally instead of globablly
 * to account for multiple nodes on this level.  */ 
{
struct sqlConnection *conn6 = sqlConnect("cdw"); 
char query6[1024]; 
sqlSafef(query6, 1024, "select * from cdwValidFile where fileId=\'%i\'",fileId);
struct cdwValidFile *out6 = cdwValidFileLoadByQuery(conn6, query6);
sqlDisconnect(&conn6);
struct jsonNode *endNode = newJsonNode(out6->licensePlate, totalNodes, false, gX, yPos+400);
slAddHead(&nodeList, endNode);
++totalNodes;
gX += 200; 
}

void rLookForNodes(int fileId, int currentNode, int yPos)
{
/* Assumes the first node has been created, now we are looking in the cdwStepIn file. If we find something
 * then the function continues. */  
struct sqlConnection *conn1 = sqlConnect("cdw"); 
char query1[1024]; 
sqlSafef(query1, 1024, "select * from cdwStepIn where fileId = '%i'", fileId); 
struct cdwStepIn *out1 = cdwStepInLoadByQuery(conn1, query1);
sqlDisconnect(&conn1);
struct jsonLink *firstLink = newJsonLink(out1->name, currentNode-1, currentNode);
//Create a link from the previous node to the intermediate node 
slAddHead(&linkList, firstLink);// Add the link to the list 
// Add the intermediate node 
addTransitionNode(out1->stepRunId, yPos+200); 
// Look at the table cdwStepOut for the output nodes, there could be more than one... 
struct sqlConnection *conn5 = sqlConnect("cdw"); 
char query5[1024]; 
sqlSafef(query5, 1024, "select * from cdwStepOut where stepRunId = '%i';", out1->stepRunId);
struct cdwStepOut *out5 = cdwStepOutLoadByQuery(conn5, query5);
struct cdwStepOut *iter = out5; 
for (; iter != NULL; iter = iter->next) 
    // Iterate through the output rows. 
    {
    //create a link from the intermediate node to the end node. 
    struct jsonLink *secondLink = newJsonLink(iter->name, currentNode, totalNodes);
    slAddHead(&linkList, secondLink); // add the link to the list. 
    addEndNode(iter->fileId, yPos);// add the end node to the list.  
    struct sqlConnection *conn3 = sqlConnect("cdw"); 
    char query3[1024];
    sqlSafef(query3, 1024, "select * from cdwStepIn where fileId = '%i'", iter->fileId); 
    struct cdwStepIn *out3 = cdwStepInLoadByQuery(conn3, query3); 
    if (out3 !=NULL)
	{
	uglyf("recursing...\n"); 
	rLookForNodes(out5->fileId, totalNodes, yPos+400);  	
	}
    }
sqlDisconnect(&conn5); 
}

void normalizeXCoords ()
/* Some tricky code I am playing with... It takes advantage of the output 
 * printing convention, so it is very finicky.  */
{
int cLev = nodeList->yloc, xTot = 0, stCnt = 0 ; 
uglyf ("the cLev is %i \n", cLev); 
while (cLev > 0)
    {
    struct jsonNode *iterN;  
    for (iterN=nodeList; iterN!=NULL; iterN = iterN->next) 
	{
	if (iterN->yloc == cLev)
	    {
	    xTot += iterN->xloc; 
	    ++stCnt;
	    //uglyf ("the xTot is %i,  the stCnt is %i \n ", xTot, stCnt); 
	    
	    }
	if (iterN->yloc == cLev-200)
	    {
	    uglyf ("the xTot is %i,  the stCnt is %i, the updated Xloc should be  %i\n ", xTot, stCnt, xTot/stCnt); 
	    iterN->xloc = xTot/stCnt; 
	    uglyf ("The iterN->xloc is %i\n", iterN->xloc); 
	    stCnt = 0; 
	    xTot = 0; 
	    }
	}
    --cLev; 
    }
}

void sqlToTxt(char *startQuery, char *outputFile)
/* sqlToTxt - A program that runs through SQL tables and generates history flow chart information. */
{
AllocVar(nodeList); 
AllocVar(linkList); 
FILE *f = mustOpen(outputFile,"w");
// graph the fileId to a licensePlate for the first node
struct sqlConnection *conn = sqlConnect("cdw"); 
char query[1024]; 
sqlSafef(query, 1024, "select * from cdwValidFile where fileId = '%s'", startQuery); 
struct cdwValidFile *out = cdwValidFileLoadByQuery(conn, query); 
struct jsonNode *startNode = newJsonNode(out->licensePlate, totalNodes, false, 200, 200);
slAddHead(&nodeList, startNode);
++totalNodes;
rLookForNodes(atoi(startQuery), totalNodes, 200); 
//slReverse(&nodeList);
normalizeXCoords(); 
if (clForceLayout) printToForceLayoutJson(f);
else printToGoJson(f); 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clForceLayout = optionExists("forceLayout");
if (argc != 3)
    usage();
sqlToTxt(argv[1], argv[2]);
return 0;
}
