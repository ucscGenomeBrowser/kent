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

struct sortNode *newSortNode(char *name, double val)
{
struct sortNode *node = AllocA(struct sortNode);

node->children = NULL;
node->parent = NULL;
node->val = val;
node->name = cloneString(name);

return node;
}

struct sortNode *findChildWithVal(struct sortNode *parent, double val)
{
struct sortList *child, *children = parent->children;

for (child = children; child; child = child->next)
    {
    struct sortNode *node = child->node;
    if (node->val == val)
	return node;
    }

return NULL;
}


struct sortNode *addChild(struct sortNode *parent, char* name, double val, int sortType)
/* Add a sortNode below the parent's level of the tree */
{
if (parent == NULL)
    return NULL;

struct sortNode *node = NULL;

struct sortList *child, *children = parent->children;

struct sortList *newChild = AllocA(struct sortList);
node = newSortNode(name, val);
node->parent = parent;
newChild->node = node;    

if (children == NULL)
    {
    parent->children = newChild;
    return newChild->node;
    }

struct sortList *prevChild = NULL;
struct sortList *nextChild = NULL; 
for (child = children; child; child = child->next)
    {
    node = child->node;
    
    if (sortType * node->val <= sortType * val)
	prevChild = child;

    if (sortType * node->val > sortType * val)
	{
	nextChild = child;
	break;
	}
    }

if (prevChild != NULL)
    {
    nextChild = prevChild->next;
    prevChild->next = newChild;
    newChild->next = nextChild;
    }
else
    {
    nextChild = children;
    newChild->next = nextChild;
    parent->children = newChild;
    }

return newChild->node;
}

struct dyString *printSortedNodes(struct dyString *dy, struct sortNode *node)
{
if (node == NULL)
    return dy;

struct sortList *child, *children = node->children;

if (children == NULL)
    {
    dyStringAppend(dy, node->name);
    dyStringAppend(dy, ",");
    return dy;
    }

for (child = children; child; child = child->next)
    {
    dy = printSortedNodes(dy, child->node);
    }

return dy;
}

char *sortPatients(struct sqlConnection *conn, struct column *colList, char *patientStr)
/* Sort a comma-separated set of patients based on active columns */
{
if (patientStr == NULL)
    return NULL;

struct slName *pa, *patients = slNameListFromComma(patientStr);
struct column *lastCol, *col = NULL;

struct sortNode *root = newSortNode(NULL, 0);
struct sortNode *child, *parent;

for (col = colList; col; col = col->next)
{
if ((col->on) && (col->cellSortDirection))
    lastCol = col;
}

for (pa = patients; pa; pa = pa->next)
    {
    child = NULL;
    parent = root;
    for (col = colList; col; col = col->next)
	{
	if (!col->on || (col->cellSortDirection == 0))
	    continue;

	double val = NULL_FEATURE_VAL;
	char* cellVal = col->cellVal(col, pa, conn);
	if (cellVal)
	    val = atof(cellVal);

	child = NULL;
	if (!sameString(col->name, lastCol->name))
	    child = findChildWithVal(parent, val);
	    
	if (child == NULL)
	    child = addChild(parent, pa->name, val, col->cellSortDirection);

	parent = child;
	}   
    }

struct dyString *dy = AllocA(struct dyString);
printSortedNodes(dy, root);

return dy->string;
}

 



