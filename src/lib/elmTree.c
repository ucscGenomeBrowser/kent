/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

#include "common.h"
#include "elmTree.h"

struct elmNode *treeNewNode(struct lm *lm,struct elmNode **tree,struct slList *content)
// Returns a new node to attach to a tree.
{
assert((*tree != NULL && content != NULL) || (*tree == NULL && content == NULL));

struct elmNode *node;
lmAllocVar(lm,node);
if (tree != NULL) // Keep all nodes in a simple slList
    slAddHead(tree,node);
node->selfAndDescendants = 1;
node->content = content;
return node;
}
// NOTE the root of the tree always has NULL content.
#define treePlant(lm,tree) treeNewNode(lm,tree,NULL)

INLINE void treeSprout(struct elmNode *parent,struct elmNode *node)
// Adds a leaf or branch node to an existing tree node.
{
node->parent = parent;
node->sibling = parent->firstChild;
parent->firstChild = node;
parent->selfAndDescendants += node->selfAndDescendants;
}

struct elmNode *treeClip(struct elmNode *node)
// Clips leaf or branch node from a tree returning the node's former parent
{
assert(node->parent != NULL);
struct elmNode *parent = node->parent;
node->parent = NULL;

// node may not be first child, so find node in children 
struct elmNode *child = parent->firstChild; 
struct elmNode *prevChild = NULL;
struct elmNode *nextChild = NULL;
for (; child != NULL; prevChild = child, child = nextChild)
    {
    nextChild = child->sibling;
    if (child == node)
        {
        child->sibling = NULL;
        if (prevChild != NULL)
            prevChild->sibling = nextChild;
        else
            parent->firstChild = nextChild;
        break;
        }
    }
parent->selfAndDescendants -= node->selfAndDescendants;
return parent;
}


INLINE void treeBranchBefore(struct elmNode *branch,struct elmNode *node)
// Adds a new 'branch' node just before the current 'node'
{
struct elmNode *parent = treeClip(node);
treeSprout(parent,branch);
treeSprout(branch,node);
}

enum elmNodeOverlap rTreeBranchFindMaxWeight(struct elmNode *branch, struct slList *element, 
                                                    elmNodeCompareFunction *neighborCmp, 
                                                    int *pWeight, void *extra)
// Using the compare routine, the max weight is found an element and a candidate branch
{
int bestWeight = 0;
int topNodeResult = neighborCmp(branch->content,element,&bestWeight,extra);
if (topNodeResult != enoEqual && topNodeResult != enoSuperset // could do better
&&  bestWeight > 0 && bestWeight >= *pWeight)           // reason to look for more
    {
    struct elmNode *child = branch->firstChild;
    for ( ;child != NULL; child = child->sibling)
        {
        int weight = bestWeight;  // will not recurse if new weight is less than best
        int result = rTreeBranchFindMaxWeight(child,element,neighborCmp,&weight,extra);
        if (bestWeight < weight)
            bestWeight = weight;
        if (result == enoEqual
        ||  result == enoSuperset)
            break;  // don't return result of compare to child, but do select this best weight
        }
    }
*pWeight = bestWeight;
return topNodeResult;
}

static enum elmNodeOverlap treeChooseBranch(struct elmNode **pBranch, struct slList *element,
                                            elmNodeCompareFunction *neighborCmp, void *extra)
// Using the compare routine, a branch is chosen that most closely matches the element being tested
// Simple case: one of two branches has best weight so follow that branch.  More subtle case:
// both branches have equal weight.  In this case using recursive routine will look deeper into 
// each branch to find the best weight before choosing a branch.
{
struct elmNode *branch = *pBranch;
assert(branch->content != NULL);

struct elmNode *bestBranch = branch;
int bestWeight = 0;
// NOTE: using recursive routine will look deeper into a branch to find the best weight
//int bestResult = neighborCmp(bestBranch->content,element,&bestWeight,extra);
int bestResult = rTreeBranchFindMaxWeight(bestBranch,element,neighborCmp,&bestWeight,extra);

// Is there need to compare further?
if (bestResult != enoEqual && bestResult != enoSuperset)  // Could do better
    {
    for (branch = branch->sibling;branch != NULL; branch = branch->sibling)
        {
        int weight = bestWeight;  // will not recurse if new weight is less than best
        //int result = neighborCmp(branch->content,element,&weight,extra);
        int result = rTreeBranchFindMaxWeight(branch,element,neighborCmp,&weight,extra);
        if (bestWeight <  weight)                         // better than last branch
        // NOTE: preferring larger of 2 equal branches does not help.
        //|| (bestWeight == weight                         // prefer largest branch
        //   &&  branch->selfAndDescendants > bestBranch->selfAndDescendants))
            {
            bestBranch = branch;
            bestWeight = weight;
            bestResult = result;
            if (bestResult == enoEqual
            ||  bestResult == enoSuperset) // doesn't get any better
                break;
            }
        }
    }
*pBranch   = bestBranch;
return bestResult;
}


struct elmNode *elmTreeGrow(struct lm *lm,struct slList *collection, void *extra,
                            elmNodeCompareFunction *neighborCmp,
                            elmNodeJoiningFunction *neighborJoin)
// Given a collection of content and a function to compare elements of the content,
// returns a tree based on neighbor joining.  The nodes of the tree will have content
// which is cumulative out to the leaves: the root node has null content, while any child
// has content that is the superset of all it's parents.  Fill 'extra' with anything (like lm)
// that you want passed into your compare and joining functions.
{
if (collection == NULL)
    return NULL;

// Begin tree
struct elmNode *treeNodeList = NULL;
struct elmNode *root = treePlant(lm,&treeNodeList);

// Add first element:
struct slList *element = collection;
struct elmNode *newNode = treeNewNode(lm,&treeNodeList,element);
treeSprout(root,newNode);
element = element->next;

// for each additional element, walks current tree level by level, choosing the best branch and
// ultimately adding the element as a leaf.  Current leaves can become branches and
// current branches may get split to add a new leaf sprout in the middle.
struct elmNode *curNode = NULL;
struct elmNode *newBranch = NULL;
for ( ;element != NULL; element = element->next)
    {
    for (curNode = root->firstChild;  // For each element, start at just off root
         curNode != NULL; 
         curNode = curNode->firstChild) // walks down
        {
        int result = treeChooseBranch(&curNode,element,neighborCmp,extra); // chooses across
        if (result == enoEqual)                       // Replaces current
            {
            // Should only get here if a previous node has the same content as this new element
            struct slList *replacement = neighborJoin(curNode->content,element,extra);
            curNode->content = replacement;
            }
        else if (result == enoExcluding)              // New leaf
            {
            newNode = treeNewNode(lm,&treeNodeList,element);
            treeSprout(curNode->parent,newNode);  // shares nothing
            }
        else if (result == enoSubset)                 // on to children or new leaf
            {
            if (curNode->firstChild != NULL) 
                continue;                             // Only case where we continue!
            newNode = treeNewNode(lm,&treeNodeList,element);
            treeSprout(curNode,newNode);          // shares all of curNode
            }
        else if (result == enoSuperset)               // New branch with old node as child
            {
            newBranch = treeNewNode(lm,&treeNodeList,element);
            treeBranchBefore(newBranch,curNode);
            }
        else if (result == enoMixed)                  // New branch with both nodes as children
            {
            struct slList *matching = neighborJoin(curNode->content,element,extra);
            newBranch = treeNewNode(lm,&treeNodeList,matching);
            treeBranchBefore(newBranch,curNode);
            newNode   = treeNewNode(lm,&treeNodeList,element);
            treeSprout(newBranch,newNode);
            }
        break; // done, move on to next element
        }
    }
slReverse(&treeNodeList);
assert(elmNodeIsRoot(treeNodeList) && root == treeNodeList);
return root;
}

boolean rElmTreeClimb(const struct elmNode *node, struct slList *parent, void *extra,
                      elmNodePickFunction *elmPick, struct slList **results)
// Recursively climbs tree and examines every node using the supplied function.
// Each call on a node iterates through its siblings, recursively calling itself on any children.
// This function might be used to build a list of objects from each or a subset of nodes.
// If all examinations resulted in a structure, then the list will be in REVERSE traversal order.
// If you immediately slReverse(results) then the list will ascend to the furthest leaf before
// moving on to sibling leaves, twigs and branches.
// Note: if results are returned, then "parent" is filled with nearest parent's result.
// Return FALSE from the elmPick function to stop the traversal.  Thus, a complete traversal
// returns TRUE, but one that has been stopped (after finding one node?) returns FALSE.
{
const struct elmNode *sibling = elmNodeIsRoot(node) ? node->firstChild: node;
for (;sibling!=NULL;sibling = sibling->sibling)
    {
    // Some nodes are subsets rather than alleles
    struct slList *localParent = NULL;
    struct slList *result = NULL;
    boolean ret = elmPick(sibling->content,parent,extra,&result);
    if (result)
        {
        //assert(results != NULL);
        slAddHead(results,result);
        localParent = result;
        result = NULL;
        }
    else
        localParent = parent;

    if (!ret)
        return FALSE; // Stop traversing

    // a node points to only one child, but that child may have siblings
    struct elmNode *child = sibling->firstChild; // additional children are siblings
    if (child)
        {
        assert(child->content != NULL);
        ret = rElmTreeClimb(child, localParent, extra, elmPick, &result);
        if (result != NULL)
            {
            //assert(results != NULL);
            *results = slCat(result,*results); // newer results go to front!
            }
        if (!ret)
            return FALSE; // Stop traversing
        }
    }
return TRUE; // continue traversing
}


