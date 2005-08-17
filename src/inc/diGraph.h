/* diGraph - Directed graph routines. */
#ifndef DIGRAPH_H
#define DIGRAPH_H

#ifndef DLIST_H
#include "dlist.h"
#endif

struct diGraph
/* A directed graph. */
    {
    struct dgNode *nodeList;
    struct hash *nodeHash;
    struct dlList *edgeList;
    };

struct dgNode
/* A node on a directed graph */
    {
    struct dgNode *next;   /* Next in master node list. */
    char *name;            /* Name of node (allocated in hash). */
    void *val;             /* Value of node. */
    struct dgConnection *nextList;  /* Ways out of node. */
    struct dgConnection *prevList;  /* Ways into node. */
    int topoOrder;      /* Topographical order after call to dgTopoSort. */
    int component;      /* Connected component number after call to dgConnectedComponents. */
    int priority;       /* Node priority. */
    struct dgNode *tempEntry;  /* Way in for breadth first search */
    bool visited;       /* Visit order in depth first traversal. */
    bool conn;	        /* Flag for connection algorithms. */
    bool flagA;         /* Flag used in rangeGraph algorithms. */
    bool flagB;         /* Currently unused flag. */
    };

struct dgEdge
/* An edge on graph */
    {
    void *val;		/* Value of edge. */
    struct dgNode *a;   /* Node on incoming side. */
    struct dgNode *b;   /* Node on outgoing side. */
    };

struct dgConnection
/* Connects one node to another. Linked to edge. */
    {
    struct dgConnection *next;  /* Next in list. */
    struct dgNode *node;        /* Node this connects to. */
    struct dlNode *edgeOnList;  /* Associated edge. */
    };

struct dgNodeRef
/* A reference to a node. */
    {
    struct dgNodeRef *next;	/* Next in list. */
    struct dgNode *node;        /* Pointer to node. */
    };

struct dgEdgeRef
/* A reference to an edge. */
    {
    struct dgEdgeRef *next;	/* Next in list. */
    struct dgEdge *edge;        /* Reference to edge. */
    };

struct diGraph *dgNew();
/* Return a new directed graph object. */

void dgFree(struct diGraph **pGraph);
/* Free a directed graph. */

struct dgNode *dgAddNode(struct diGraph *dg, char *name, void *val);
/* Create new node in graph. */

struct dgNode *dgAddNumberedNode(struct diGraph *dg, int id, void *val);
/* Create new node with a number instead of a name. */
 
struct dgNode *dgFindNode(struct diGraph *dg, char *name);
/* Find existing node in graph. */

struct dgNode *dgFindNumberedNode(struct diGraph *dg, int id);
/* Find node given number. */

void *dgNodeVal(struct dgNode *node);
/* Return value associated with node. */

void *dgNodeName(struct dgNode *node);
/* Return name associated with node. */

int dgNodeNumber(struct dgNode *node);
/* Return number of node.  (Will likely return 0 if node
 * was added with a name rather than a number). */

struct dgEdge *dgConnect(struct diGraph *dg, struct dgNode *a, struct dgNode *b);
/* Connect node a to node b.  Returns connecting edge. */

struct dgEdge *dgConnectWithVal(struct diGraph *dg, struct dgNode *a, struct dgNode *b, void *val);
/* Connect node a to node b and put val on edge. */

void dgDisconnect(struct diGraph *dg, struct dgNode *a, struct dgNode *b);
/* Disconnect nodes a and b. */

struct dgEdge *dgDirectlyFollows(struct diGraph *dg, struct dgNode *a, struct dgNode *b);
/* Return TRUE if b directly follows a. */

boolean dgPathExists(struct diGraph *dg, struct dgNode *a, struct dgNode *b);
/* Return TRUE if there's a path from a to b. */

struct dgNodeRef *dgFindPath(struct diGraph *dg, struct dgNode *a, struct dgNode *b);
/* Find shortest path from a to b.  Return NULL if can't be found. */

struct dgNodeRef *dgFindConnectedToNode(struct diGraph *dg, struct dgNode *a);
/* Return reference list of all nodes connected to a, including a.
 * slFreeList this list when done. */

struct dgNodeRef *dgFindNewConnected(struct diGraph *dg, struct dgNode *a);
/* Find a connected component guaranteed not to be covered before 
 * that includes a. */

struct dgNodeRef *dgFindNewConnectedWithVals(struct diGraph *dg, struct dgNode *a);
/* Find a connected component guaranteed not to be covered before 
 * that includes a.  Connected components must have values*/

struct dgNodeRef *dgFindNextConnected(struct diGraph *dg);
/* Return list of nodes that make up next connected component,
 * or NULL if no more components.  slFreeList this when
 * done.  Call "dgClearConnFlags" before first call to this.
 * Do not call dgFindConnectedToNode between dgFindFirstConnected 
 * and this as they use the same flag variables to keep track of
 * what vertices are used. */

struct dgNodeRef *dgFindNextConnectedWithVals(struct diGraph *dg);
/* Like dgFindConnected, but only considers graph nodes that
 * have a val attached. */

int dgConnectedComponents(struct diGraph *dg);
/* Count number of connected components and set component field
 * of each node to reflect which component it is in. */

int dgConnectedComponentsWithVals(struct diGraph *dg);
/* Count number of connected components and set component field
 * of each node to reflect which component it is in. Only
 * consider components with values. */

void dgTopoSort(struct diGraph *dg);
/* Fill in topological order of nodes. */

boolean dgHasCycles(struct diGraph *dg);
/* Return TRUE if directed graph has cycles. */

boolean dgParentsAllVisited(struct dgNode *node);
/* Return TRUE if all parents of node have  been visited. */

struct dgNodeRef *dgConstrainedPriorityOrder(struct diGraph *dg);
/* Return traversal of graph in priority order subject to
 * constraint that all parents must be output before
 * their children regardless of node priority. 
 * Graph must be cycle free. */

boolean dgRangesConsistent(struct diGraph *rangeGraph,
   boolean (*findEdgeRange)(struct dgEdge *edge, int *retMin, int *retMax) );
/* Return TRUE if graph with a range of allowable distances associated
 * with each edge is internally consistent. */

boolean dgAddedRangeConsistent(struct diGraph *rangeGraph,
   struct dgNode *a, struct dgNode *b, int abMin, int abMax,
   boolean (*findEdgeRange)(struct dgEdge *edge, int *retMin, int *retMax) );
/* Return TRUE if graph with a range of allowable distances associated
 * with each edge would be internally consistent if add edge from a to b
 * with given min/max values. */

struct dgEdgeRef *dgFindSubEdges(struct diGraph *dg, struct dgNodeRef *subGraph);
/* Return list of edges in graph that connected together nodes in subGraph. */

void dgSwapEdges(struct diGraph *dg, struct dgEdgeRef *erList);
/* Swap polarity of all edges in erList.  (Assumes you don't have
 * edges going both directions in graph.) */

void dgClearConnFlags(struct diGraph *dg);
/* Clear out conn flags. */

void dgClearVisitFlags(struct diGraph *dg);
/* Clear out visit order flags. */

struct dgNodeRef *dgFindNodeInRefList(struct dgNodeRef *refList, struct dgNode *node);
/* Return reference to node if in list, or NULL if not. */

struct dgConnection *dgFindNodeInConList(struct dgConnection *conList, struct dgNode *node);
/* Return connection to node if in list, or NULL if not. */

struct dgConnection *dgNextList(struct dgNode *node);
/* Return list of connections out of this node. Don't free (or rearrange)
 * this list please. */

struct dgConnection *dgPrevList(struct dgNode *node);
/* Return list of connections into this node. Don't free (or rearrange)
 * this list please. */

void dgDumpGraph(struct diGraph *dg, FILE *out, boolean hideIsolated);
/* Dump info on graph to output. */

#endif /* DIGRAPH_H */

