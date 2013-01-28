// elmTree.c/.h - Extensible local memory tree grown from an slList of objects, via
// NEIGHBOR JOINING and a supplied compare routine.  Unlike hacTree, this is not a binary tree
// and any node can have original content, a single parent and N children.  Superficially similar
// to running slSort(), running elmTreeGrow() creates a tree from a single "root".  All branches
// and leaves are represented as "tree nodes" which contain pointers to a user defined object,
// a single parent, a first child (if the node is not a leaf) and a next sibling.  All nodes are
// additionally joined together as an slList with the single "root" as the first member.  Thus
// the tree can be traversed linerarly through node->next or hierarchically through recursive
// node->firstChild,child->sibling access.
// The tree is grown through neighbor joins and involves figuring out whether each successive 
// content should be attached as a branch node or leaf node to the existing tree.  To facilitate
// neighbor joining, each node has cumulative content from the single root node (whcih has NULL
// content).  An example might be a tree of bitmaps where each node contains all the bits of its
// parent plus additional bits.
// Since an elmTree is build with local memory, free the tree by lmCleanup().

#ifndef ELMTREE_H
#define ELMTREE_H

#include "common.h"
#include "localmem.h"

struct elmNode {
// A tree node may be either a leaf or a branch, with branches having children
// NOTE the root of the tree always has NULL content.
    struct elmNode * next;       // slList of all nodes in tree starting with root
    struct elmNode * parent;     // NULL: root of tree, else only one parent allowed
    struct elmNode * firstChild; // NULL: leaf, or firstChild (successive are siblings)
    struct elmNode * sibling;    // children are singly linked as on "next" sibling
    int selfAndDescendants;      // count of self and descendants, but not siblings and parents
    void *content; // Content of node, such as bitMap that defines node.
};

enum elmNodeOverlap {
    enoExcluding  =    0,   // Nothing in common between 2 elements
    enoEqual      =    1,   // Two node elements are identical
    enoSuperset   =    2,   // First element completely covers second
    enoSubset     =    3,   // First element is a subset of second
    enoMixed      =    4    // Elements match some and differ some (use weight to distinguish)
};


// - - - - - growing a tree:
typedef enum elmNodeOverlap (elmNodeCompareFunction)(const struct slList *elA,
                                                     const struct slList *elB,
                                                     int *joinWeight, void *extra);
// Prototype of variable compare routines.  Caller may request joinWeight.  Fill this with a
// weight of the commonality between two elements, with a greater weight representing a better
// possible join.  An example might be bit compares and the weight being the number of bits
// that match with the max being the number of possible matches.  This value will be used to
// help decide what branch a new node should join.

typedef struct slList *(elmNodeJoiningFunction)(const struct slList *elA,
                                                const struct slList *elB,void *extra);
// Prototype of function for creating the common content of 2 elements that are part of a tree.
// As the tree grows, elements will get attached to common branches and those branches will
// have content created by this function.  An example application might be two of your bitmaps
// representing actual data being used to create a third bitmap with is the common bits set
// but not representing one of your original element bitmaps (actual data).

struct elmNode *elmTreeGrow(struct lm *lm,struct slList *collection, void *extra,
                            elmNodeCompareFunction *neighborCmp,
                            elmNodeJoiningFunction *neighborJoin);
// Given a collection of content and a function to compare elements of the content,
// returns a tree based on neighbor joining.  The nodes of the tree will have content
// which is cumulative out to the leaves: the root node has null content, while any child
// has content that is the superset of all it's parents.  Fill 'extra' with anything (like lm)
// that you want passed into your compare and joining functions.


// - - - - - picking nodes from a tree:
typedef boolean (elmNodePickFunction)(struct slList *element,
                                      struct slList *parent,void *extra,
                                      struct slList **result);
// Prototype for picking nodes by using recursive tree climbing routine.  This user provided 
// function receives the content of a node and any extra structure provided.
// The routine can return a result if desired.  When the function returns a result, then the
// result of the nearest "parent" (if applicable) will be provided as well.  All results will
// ultimately be strung together in a list in traversal order.  The routine should
// return TRUE to keep traversing the tree and FALSE if tree traversal should stop.  

boolean rElmTreeClimb(const struct elmNode *node, struct slList *parent, void *extra,
                      elmNodePickFunction *elmPick, struct slList **results);
// Recursively climbs tree and examines every node using the supplied function.
// Each call on a node iterates through its siblings, recursively calling itself on any children.
// This function might be used to build a list of objects from each or a subset of nodes.
// If all examinations resulted in a structure, then the list will be in REVERSE traversal order.
// If you immediately slReverse(results) then the list will ascend to the furthest leaf before
// moving on to sibling leaves, twigs and branches.
// Note: if results are returned, then "parent" is filled with nearest parent's result.
// Return FALSE from the elmPick function to stop the traversal.  Thus, a complete traversal
// returns TRUE, but one that has been stopped (after finding one node?) returns FALSE.


// - - - - - general tree examination
INLINE int elmNodeCountSiblings(const struct elmNode *node)
// Counts siblings, including current
{
int count = 0;
const struct elmNode *sibling = node;
for (;sibling != NULL; sibling = sibling->sibling)
    count++;
return count;
}
#define elmNodeCountChildren(node) elmNodeCountSiblings((node)->firstChild)

INLINE int elmNodeCountGenerations(const struct elmNode *node)
// Counts direct generations (first children only), including current
{
int count = 0;
const struct elmNode *child = node;
for (;child != NULL; child = child->firstChild)
    count++;
return count;
}

#define elmNodeIsBranch(node) (node->firstChild != NULL)
#define elmNodeIsLeaf(node)   (node->firstChild == NULL)
#define elmNodeIsRoot(node)   (node->parent     == NULL)


#endif//ndef ELMTREE_H
