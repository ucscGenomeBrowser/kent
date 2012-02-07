/* diGraph - Directed graph routines. */
#include "common.h"
#include "hash.h"
#include "dlist.h"
#include "diGraph.h"


struct diGraph *dgNew()
/* Return a new directed graph object. */
{
struct diGraph *dg;
AllocVar(dg);
dg->nodeHash = newHash(0);
dg->edgeList = newDlList();
return dg;
}

static void dgNodeFree(struct dgNode **pNode)
/* Free a diGraph node. */
{
struct dgNode *node = *pNode;
if (node == NULL)
    return;
slFreeList(&node->nextList);
slFreeList(&node->prevList);
freez(pNode);
}

static void dgNodeFreeList(struct dgNode **pList)
/* Free list of diGraph nodes. */
{
struct dgNode *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dgNodeFree(&el);
    }
*pList = NULL;
}

void dgFree(struct diGraph **pGraph)
/* Free a directed graph. */
{
struct diGraph *dg = *pGraph;
if (dg == NULL)
    return;
freeHash(&dg->nodeHash);
dgNodeFreeList(&dg->nodeList);
freeDlListAndVals(&dg->edgeList);
freez(pGraph);
}


struct dgNode *dgAddNode(struct diGraph *dg, char *name, void *val)
/* Create new node in graph. It's legal (but not efficient) to add
 * a node with the same name and value twice.  It's not legal to
 * add a node with the same name and a different value.  
 * You can pass in NULL for the name in which case the 
 * hexadecimal representation of val will become the name. */
{
struct dgNode *node;
struct hashEl *hel;
struct hash *hash = dg->nodeHash;
char nbuf[17];

if (name == NULL)
    {
    sprintf(nbuf, "%p", val);
    name = nbuf;
    }
hel = hashLookup(hash, name);
if (hel != NULL)
    {
    node = hel->val;
    if (node->val != val)
	{
	errAbort("Trying to add node %s with a new value (old 0x%llx new 0x%llx)",
	    name, ptrToLL(node->val), ptrToLL(val));
	}
    return node;
    }
AllocVar(node);
hel = hashAdd(hash, name, node);
node->name = hel->name;
node->val = val;
slAddHead(&dg->nodeList, node);
return node;
}

struct dgNode *dgAddNumberedNode(struct diGraph *dg, int id, void *val)
/* Create new node with a number instead of a name. */
{
char buf[16];
sprintf(buf, "%d", id);
return dgAddNode(dg, buf, val);
}

struct dgNode *dgFindNode(struct diGraph *dg, char *name)
/* Find existing node in graph. Return NULL if not in graph. */
{
struct hashEl *hel;
if ((hel = hashLookup(dg->nodeHash, name)) == NULL)
    return NULL;
return hel->val;
}

struct dgNode *dgFindNumberedNode(struct diGraph *dg, int id)
/* Find node given number. */
{
char buf[16];
sprintf(buf, "%d", id);
return dgFindNode(dg, buf);
}


void *dgNodeVal(struct dgNode *node)
/* Return value associated with node. */
{
return node->val;
}

void *dgNodeName(struct dgNode *node)
/* Return name associated with node. */
{
return node->name;
}

int dgNodeNumber(struct dgNode *node)
/* Return number of node.  (Will likely return 0 if node
 * was added with a name rather than a number). */
{
return atoi(node->name);
}

struct dgNodeRef *dgFindNodeInRefList(struct dgNodeRef *refList, struct dgNode *node)
/* Return reference to node if in list, or NULL if not. */
{
struct dgNodeRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    if (ref->node == node)
	return ref;
return NULL;
}

struct dgConnection *dgFindNodeInConList(struct dgConnection *conList, struct dgNode *node)
/* Return connection to node if in list, or NULL if not. */
{
struct dgConnection *con;
for (con = conList; con != NULL; con = con->next)
    if (con->node == node)
	return con;
return NULL;
}


struct dgEdge *dgConnect(struct diGraph *dg, struct dgNode *a, struct dgNode *b)
/* Connect node a to node b.  Returns connecting edge. 
 * Not an error to reconnect.  However all connects can 
 * be broken with a single disconnect. */
{
return dgConnectWithVal(dg, a, b, NULL);
}

struct dgEdge *dgConnectWithVal(struct diGraph *dg, struct dgNode *a, 
     struct dgNode *b, void *val)
/* Connect node a to node b and put val on edge.  An error to
 * reconnect with a different val. */
{
struct dgConnection *con;
struct dgEdge *edge;
struct dlNode *edgeOnList;

/* Check to see if it's already there. */
if ((con = dgFindNodeInConList(a->nextList, b)) != NULL)
    {
    edge =  con->edgeOnList->val;
    if (val != edge->val)
        warn("Trying to add new value to edge between %s and %s, ignoring",
	   a->name, b->name);
    return edge;
    }
/* Allocate edge and put on list. */
AllocVar(edge);
edge->a = a;
edge->b = b;
edge->val = val;
edgeOnList = dlAddValTail(dg->edgeList, edge);

/* Connect nodes to each other. */
AllocVar(con);
con->node = b;
con->edgeOnList = edgeOnList;
slAddHead(&a->nextList, con);
AllocVar(con);
con->node = a;
con->edgeOnList = edgeOnList;
slAddHead(&b->prevList, con);

return edge;
}

static struct dlNode *dgRemoveFromConList(struct dgConnection **pConList, 
	struct dgNode *node, struct dgConnection **retCon)
/* Remove reference to node from list. */
{
struct dgConnection *newList = NULL;
struct dgConnection *con, *next;
struct dlNode *edgeOnList = NULL;

for (con = *pConList; con != NULL; con = next)
    {
    next = con->next;
    if (con->node == node)
	{
	edgeOnList = con->edgeOnList;
	if (retCon != NULL)
	    *retCon = con;
	else
	    freeMem(con);
	}
    else
	{
	slAddHead(&newList, con);
	}
    }
slReverse(&newList);
*pConList = newList;
return edgeOnList;
}

void dgDisconnect(struct diGraph *dg, struct dgNode *a, struct dgNode *b)
/* Disconnect nodes a and b. */
{
struct dlNode *edgeInList;
struct dgEdge *edge;

dgRemoveFromConList(&a->nextList, b, NULL);
edgeInList = dgRemoveFromConList(&b->prevList, a, NULL);
if (edgeInList != NULL)
    {
    edge = edgeInList->val;
    dlRemove(edgeInList);
    freeMem(edgeInList);
    freeMem(edge);
    }
}


void dgClearVisitFlags(struct diGraph *dg)
/* Clear out visit flags. */
{
struct dgNode *node;
for (node = dg->nodeList; node != NULL; node = node->next)
    node->visited = FALSE;
}

void dgClearConnFlags(struct diGraph *dg)
/* Clear out connect flags. */
{
struct dgNode *node;
for (node = dg->nodeList; node != NULL; node = node->next)
    node->conn = FALSE;
}

static struct dgNode *rTarget;

static boolean rPathExists(struct dgNode *a)
/* Recursively find if path from a to b exists. */
{
struct dgConnection *ref;
if (a == rTarget)
    return TRUE;
a->visited = TRUE;

for (ref = a->nextList; ref != NULL; ref = ref->next)
    {
    if (!ref->node->visited && rPathExists(ref->node))
	return TRUE;
    }
return FALSE;
}

boolean dgPathExists(struct diGraph *dg, struct dgNode *a, struct dgNode *b)
/* Return TRUE if there's a path from a to b. */
{
rTarget = b;
dgClearVisitFlags(dg);
return rPathExists(a);
}

static int topoIx;

static void rTopoSort(struct dgNode *node)
{
struct dgConnection *ref;
node->visited = TRUE;
for (ref = node->nextList; ref != NULL; ref = ref->next)
    {
    if (!ref->node->visited)
	rTopoSort(ref->node);
    }
node->topoOrder = ++topoIx;
}

void dgTopoSort(struct diGraph *dg)
/* Fill in topological order of nodes. */
{
struct dgNode *node;

topoIx = 0;
dgClearVisitFlags(dg);
for (node = dg->nodeList; node != NULL; node = node->next)
    {
    if (!node->visited)
	rTopoSort(node);
    }
}

static boolean  rHasCycles(struct dgNode *node)
/* Recursively see if has cycles by looking for
 * backwards topoOrder. */
{
struct dgConnection *ref;
struct dgNode *child;

node->visited = TRUE;
for (ref = node->nextList; ref != NULL; ref = ref->next)
    {
    child = ref->node;
    if (child->topoOrder > node->topoOrder)
	return TRUE;
    if (!child->visited)
	if (rHasCycles(child))
	    return TRUE;
    }
return FALSE;
}

boolean dgHasCycles(struct diGraph *dg)
/* Return TRUE if directed graph has cycles. */
{
struct dgNode *node;

dgTopoSort(dg);
dgClearVisitFlags(dg);
for (node = dg->nodeList; node != NULL; node = node->next)
    {
    if (!node->visited)
	if (rHasCycles(node))
	    return TRUE;
    }
return FALSE;
}

struct dgNodeRef *rRefList;
bool rMustHaveVal;			/* Set to TRUE if rFindConnected only to
                                         * consider nodes with values in connections. */

static void rFindConnected(struct dgNode *a)
/* Find all things connected to a directly or not that haven't
 * already been visited and put them on rRefList. */
{
if (!a->conn && (!rMustHaveVal || a->val))
    {
    struct dgNodeRef *ref;
    struct dgConnection *con;
    AllocVar(ref);
    ref->node = a;
    slAddHead(&rRefList, ref);
    a->conn = TRUE;
    for (con = a->nextList; con != NULL; con = con->next)
	rFindConnected(con->node);
    for (con = a->prevList; con != NULL; con = con->next)
	rFindConnected(con->node);
    }
}

struct dgNodeRef *dgFindNextConnected(struct diGraph *dg)
/* Return list of nodes that make up next connected component,
 * or NULL if no more components.  slFreeList this when
 * done.  Call "dgClearConnFlags" before first call to this.
 * Do not call dgFindConnectedToNode between dgFindFirstConnected 
 * and this as they use the same flag variables to keep track of
 * what vertices are used. */
{
struct dgNode *a;

for (a=dg->nodeList; a != NULL; a = a->next)
    {
    if (!a->conn)
	break;
    }
if (a == NULL)
    return NULL;
rRefList = NULL;
rMustHaveVal = FALSE;
rFindConnected(a);
return rRefList;
}

struct dgNodeRef *dgFindNextConnectedWithVals(struct diGraph *dg)
/* Like dgFindConnected, but only considers graph nodes that
 * have a val attached. */
{
struct dgNode *a;

for (a=dg->nodeList; a != NULL; a = a->next)
    {
    if (!a->conn && a->val != NULL)
	break;
    }
if (a == NULL)
    return NULL;
rRefList = NULL;
rMustHaveVal = TRUE;
rFindConnected(a);
return rRefList;
}


static int connectedComponents(struct diGraph *dg)
/* Count number of connected components and set component field
 * of each node to reflect which component it is in. */
{
struct dgNodeRef *ref;
int conCount = 0;
struct dgNode *a = dg->nodeList;

dgClearConnFlags(dg);
for (;;)
    {
    for (; a != NULL; a = a->next)
	{
	if (!a->conn && (!rMustHaveVal || a->val))
	    break;
	}
    if (a == NULL)
	break;
    rRefList = NULL;
    rFindConnected(a);
    ++conCount;
    for (ref = rRefList; ref != NULL; ref = ref->next)
	{
	ref->node->component = conCount;
	}
    slFreeList(&rRefList);
    }
return conCount;
}

int dgConnectedComponents(struct diGraph *dg)
/* Count number of connected components and set component field
 * of each node to reflect which component it is in. */
{
rMustHaveVal = FALSE;
return connectedComponents(dg);
}

int dgConnectedComponentsWithVals(struct diGraph *dg)
/* Count number of connected components and set component field
 * of each node to reflect which component it is in. Only
 * consider components with values. */
{
rMustHaveVal = TRUE;
return connectedComponents(dg);
}

struct dgNodeRef *dgFindNewConnected(struct diGraph *dg, struct dgNode *a)
/* Find a connected component guaranteed not to be covered before 
 * including a. */
{
rRefList = NULL;
rMustHaveVal = FALSE;
rFindConnected(a);
return rRefList;
}

struct dgNodeRef *dgFindNewConnectedWithVals(struct diGraph *dg, struct dgNode *a)
/* Find a connected component guaranteed not to be covered before 
 * that includes a.  Connected components must have values*/
{
rRefList = NULL;
rMustHaveVal = TRUE;
rFindConnected(a);
return rRefList;
}


struct dgNodeRef *dgFindConnectedToNode(struct diGraph *dg, struct dgNode *a)
/* Return reference list of all nodes connected to a, including a.
 * slFreeList this list when done. */
{
dgClearConnFlags(dg);
return dgFindNewConnected(dg, a);
}

struct dgEdge *dgDirectlyFollows(struct diGraph *dg, struct dgNode *a, struct dgNode *b)
/* Return TRUE if b directly follows a. */
{
struct dgConnection *con = dgFindNodeInConList(a->nextList, b);
if (con == NULL)
    return NULL;
return con->edgeOnList->val;
}

struct dgNodeRef *dgFindPath(struct diGraph *dg, struct dgNode *a, struct dgNode *b)
/* Find shortest path from a to b.  Return NULL if can't be found. */
{
struct dgNodeRef *refList  = NULL, *ref;
struct dgConnection *con;
struct dgNode *node, *nNode;
struct dlList *fifo;
struct dlNode *ffNode;
struct dgNode endNode;
int fifoSize = 1;

/* Do some quick and easy tests first to return if have no way out
 * of node A, or if B directly follows A. */
if (a->nextList == NULL)
    return NULL;
if (a == b)
    {
    AllocVar(ref);
    ref->node = a;
    return ref;
    }
if ((con = dgFindNodeInConList(a->nextList, b)) != NULL)
    {
    AllocVar(refList);
    refList->node = a;
    node = con->node;
    AllocVar(ref);
    ref->node = node;
    slAddTail(&refList, ref);
    return refList;
    }

/* Set up for breadth first traversal.  Will use a doubly linked
 * list as a fifo. */
for (node = dg->nodeList; node != NULL; node = node->next)
    node->tempEntry = NULL;
fifo = newDlList();
dlAddValTail(fifo, a);
a->tempEntry = &endNode;

while ((ffNode = dlPopHead(fifo)) != NULL)
    {
    --fifoSize;
    node = ffNode->val;
    freeMem(ffNode);
    for (con = node->nextList; con != NULL; con = con->next)
	{
	nNode = con->node;
	if (nNode->tempEntry == NULL)
	    {
	    nNode->tempEntry = node;
	    if (nNode == b)
		{
		while (nNode != &endNode && nNode != NULL)
		    {
		    AllocVar(ref);
		    ref->node = nNode;
		    slAddHead(&refList, ref);
		    nNode = nNode->tempEntry;
		    }
		break;
		}
	    else
		{
		dlAddValTail(fifo, nNode);
		++fifoSize;
		if (fifoSize > 100000)
		    errAbort("Internal error in dgFindPath");
		}
	    }
	}
    }
freeDlList(&fifo);
return refList;
}

static int cmpPriority(const void *va, const void *vb)
/* Sort smallest offset into needle first. */
{
const struct dgNode *a = *((struct dgNode **)va);
const struct dgNode *b = *((struct dgNode **)vb);
return (a->priority - b->priority);
}

boolean dgParentsAllVisited(struct dgNode *node)
/* Return TRUE if all parents of node have  been visited. */
{
struct dgConnection *con;
for (con = node->prevList; con != NULL; con = con->next)
    {
    if (con->node->visited == FALSE)
	return FALSE;
    }
return TRUE;
}

struct dgNodeRef *dgConstrainedPriorityOrder(struct diGraph *dg)
/* Return traversal of graph in priority order subject to
 * constraint that all parents must be output before
 * their children regardless of node priority. 
 * Graph must be cycle free. */
{
struct dlList *sortedList = newDlList();
struct dgNode *graphNode;
struct dlNode *listNode;
struct dgNodeRef *refList = NULL, *ref;

if (dgHasCycles(dg))
    errAbort("Call to dgConstrainedPriorityOrder on graph with cycles.");

/* Make up list sorted by priority. */
for (graphNode = dg->nodeList; graphNode != NULL; graphNode = graphNode->next)
    {
    dlAddValTail(sortedList, graphNode);
    graphNode->visited = FALSE;
    }
dlSort(sortedList, cmpPriority);

/* Loop taking first member of list with no untraversed parents. */
while (!dlEmpty(sortedList))
    {
    for (listNode = sortedList->head; listNode->next != NULL; listNode = listNode->next)
	{
	graphNode = listNode->val;
	if (dgParentsAllVisited(graphNode))
	    {
	    dlRemove(listNode);
	    freeMem(listNode);
	    AllocVar(ref);
	    ref->node = graphNode;
	    slAddHead(&refList, ref);
	    graphNode->visited = TRUE;
	    break;
	    }
	}
    }
freeDlList(&sortedList);
slReverse(&refList);
return refList;
}

struct dgEdgeRef *dgFindSubEdges(struct diGraph *dg, struct dgNodeRef *subGraph)
/* Return list of edges in graph that connected together nodes in subGraph. */
{
struct hash *hash = newHash(0);
struct dgNodeRef *nr;
struct dgConnection *con;
struct dgEdgeRef *erList = NULL, *er;
struct dgNode *node;

/* Build up hash of nodes in subGraph. */
for (nr = subGraph; nr != NULL; nr = nr->next)
    {
    node = nr->node;
    hashAdd(hash, node->name, node);
    }

for (nr = subGraph; nr != NULL; nr = nr->next)
    {
    node = nr->node;
    for (con = node->nextList; con != NULL; con = con->next)
	{
	if (hashLookup(hash, con->node->name))
	    {
	    AllocVar(er);
	    er->edge = con->edgeOnList->val;
	    slAddHead(&erList, er);
	    }
	}
    }
freeHash(&hash);
return erList;
}

void dgSwapEdges(struct diGraph *dg, struct dgEdgeRef *erList)
/* Swap polarity of all edges in erList.  (Assumes you don't have
 * edges going both directions in graph.) */
{
struct dgEdgeRef *er;
struct dgEdge *edge;
struct dgNode *a, *b;
struct dgConnection *con1, *con2;

/* Remove edges from next and previous list of all
 * involved nodes and swap nodes in edge itself. */
for (er = erList; er != NULL; er = er->next)
    {
    edge = er->edge;
    a = edge->a;
    b = edge->b;
    dgRemoveFromConList(&a->nextList, b, &con1);
    dgRemoveFromConList(&b->prevList, a, &con2);
    edge->a = b;
    edge->b = a;
    con1->node = a;
    slAddHead(&b->nextList, con1);
    con2->node = b;
    slAddHead(&a->prevList, con2);
    }
}


struct dgConnection *dgNextList(struct dgNode *node)
/* Return list of nodes that follow node. */
{
return node->nextList;
}

struct dgConnection *dgPrevList(struct dgNode *node)
/* Return list of nodes that precede node. */
{
return node->prevList;
}

void dgDumpGraph(struct diGraph *dg, FILE *out, boolean hideIsolated)
/* Dump info on graph to output. */
{
struct dgNode *node;
struct dgConnection *con;

for (node = dg->nodeList; node != NULL; node = node->next)
    {
    if (hideIsolated  && node->nextList == NULL)
	continue;
    fprintf(out, "%s:", node->name);
    for (con = node->nextList; con != NULL; con = con->next)
	fprintf(out, " %s", con->node->name);
    fprintf(out, "\n");
    }
}


