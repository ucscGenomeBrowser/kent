/* cdwFlowCharts - Library functions to look through SQL tables and generate dagger d3 flowcharts.*/
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "cart.h"
#include "cdwStep.h"


struct slFileRow
    {
    struct slFileRow *next; 
    struct slInt *fileList; 
    };

struct slFileRow *slFileRowNew(struct slInt *fileList);
/* Return a new slFileRow. */

int findFirstParent(struct sqlConnection *conn, int fileId); 
// Return a fileID that corresponds to the first row of the flow chart

void lookForward(struct sqlConnection *conn, char *fileId, struct slFileRow *files, struct slInt *steps, bool upstream);
/* Look forwards through the cdwStep tables until the source is found, store 
 * the file and step rows along the way */ 

void printRowToStep(struct sqlConnection *conn, struct slFileRow *fileRow, struct slInt *stepRow, int filesPerRow);
// The easy case, all the files in a given 'row' connect to the next step. 

void printStepToRow(struct sqlConnection *conn, struct slFileRow *fileRow, struct slInt *stepRow, int filesPerRow);
// The difficult case, a step can produce 1 file in the next row, 2 files, or up to n files.  

void printToDagreD3(struct sqlConnection *conn, char *fileId, struct slFileRow *files, struct slInt *steps, int filesPerRow);
/* Print out the modular parts of a a dager-d3 javascript visualization
 * These will need to be put into the .js code at the right place. The
 * base flowchart code that is being used was found here; 
 * http://cpettitt.github.io/project/dagre-d3/latest/demo/tcp-state-diagram.html */ 


void makeCdwFlowchart(int fileId, struct cart *cart);
/* Runs through SQL tables and print out a history flow chart for the file. Note that you must
 * provide a fileId from the cdwValidFile table (or an id from cdwFile). An id from cdwValidFile 
 * can produce a corrupted result!. */
