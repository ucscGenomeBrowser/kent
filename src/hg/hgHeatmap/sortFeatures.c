/* hgHeatmap is a CGI script that produces a web page containing
 * a graphic with all chromosomes in genome, and a heatmap or two
 * on top of them. This module just contains the main routine,
 * the routine that dispatches to various page handlers, and shared
 * utility functions. */

#include "common.h"
#include "cart.h"
#include "hash.h"
#include "hCommon.h"
#include "hdb.h"
#include "hPrint.h"
#include "htmshell.h"
#include "web.h"
#include "ispyFeatures.h"
#include "sortFeatures.h"

#define NULL_FEATURE_VAL 1000.0

struct sortNode *newSortNode(char *name)
{
struct sortNode *node = AllocA(struct sortNode);

node->name = cloneString(name);
node->list = NULL;

return node;
}

void printSortedNodes(struct dyString *dy, struct sortNode *nodes)
{
struct sortNode *node = NULL;
for (node = nodes; node; node = node->next)
    {
    dyStringAppend(dy, node->name);
    dyStringAppend(dy, ",");
    }
}

int sortNodeCmp(const void *va, const void *vb)
/* Sort function to compare two sortNodes progressively.  This sort checks
 * the first members of each sortNode's sortLists to start the search.  In
 * the event of a tie, the next member of each sortList is used in an 
 * attempt to break the tie.  This continues until either a tie-breaker is
 * found or the sortList is exhausted */
{
const struct sortNode *a = *((struct sortNode **)va);
const struct sortNode *b = *((struct sortNode **)vb);

struct sortList *slA = NULL, *slB = NULL;
for (slA = a->list, slB = b->list; slA && slB; slA = slA->next, slB = slB->next)
{
int sortDir = slA->sortDirection;
if (slA->val > slB->val)
    return sortDir;

if (slA->val < slB->val)
    return -1 * sortDir;
} 

return 0; /* No tie breaker found, lists must be equal */ 
}


struct slName *sortPatients(struct sqlConnection *conn, struct column *colList, struct slName *patientList)
/* Sort a list of patients based on active columns */
{
if (patientList == NULL)
    return NULL;

struct column *col = NULL;
struct sortNode *nodes = NULL;
struct slName *pa=NULL;
for (pa = patientList; pa; pa = pa->next)
    {
    struct sortNode *node = newSortNode(pa->name);
    
    for (col = colList; col; col = col->next)
	{
	if (!col->on || (col->cellSortDirection == 0))
	    continue;

	double val = NULL_FEATURE_VAL;
	char* cellVal = col->cellVal(col, pa, conn);
	if (cellVal)
	    val = atof(cellVal);
	
	struct sortList *sl = AllocA(struct sortList);
	sl->val = val;
	sl->sortDirection = col->cellSortDirection;

	slAddHead(&node->list, sl);
	}
    slReverse(&node->list);
    slAddHead(&nodes, node);
    }

slReverse(&nodes);

slSort(&nodes, sortNodeCmp);

struct dyString *dy = AllocA(struct dyString);
printSortedNodes(dy, nodes);

struct slName *sortList = slNameListFromComma(dy->string);
return sortList;

}

 



