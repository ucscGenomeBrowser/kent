/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

// --------------------------------------------------------------------
//             Tree.cc
// data structure for the hierarchical clustering tree.
// Used to compute initial and optimal similarities, and to manipulate
// (based on the optimal ordering) the ordering the tree nodes descendants.
// -------------------------------------------------------------------------

#include <iostream>
#include "Tree.hh"
#include "general.hh"

// generate a leaf (tree with only one node)
Tree::Tree(int index,float **m) {

  mat = m;
  nodeNum = index;
  allLeafs = new Leaf*[1];
  Leaf *myLeaf = new Leaf(index,mat);
  allLeafs[0] = myLeaf;
  numLeafs = 1;
  left = right = 0;
}

// combine two trees 
Tree::Tree(Tree *t1,Tree *t2,int nNum) {
  
  nodeNum = nNum;
  mat = t1->giveMat();
  int n1 = t1->giveNumLeafs();
  int n2 = t2->giveNumLeafs();
  numLeafs = n1+n2;
  allLeafs = new Leaf*[numLeafs];
  int i;
  Leaf **l = t1->giveLeafs();
  for(i=0;i<n1;i++)
    allLeafs[i] = l[i];
  l = t2->giveLeafs();
  for(i=0;i<n2;i++)
    allLeafs[n1+i] = l[i];
  left = t1;
  right = t2;
}

// compute the optimal order for this tree
int Tree::compDist(int imp) {
  if(numLeafs == 1) { // no nodes to flip
    return 0;
  }
  int i,j;
  left->compDist(imp); // compute optimal order matrix t subtree
  right->compDist(imp);
  int n1 = left->giveNumLeafs();
  int n2 = right->giveNumLeafs();
  if(n1 + n2 == imp) { // last two subtrees
    return lastTree(imp,n1,n2); 
    // this is an optimization which
    // reduces the running time by half, see paper for details
  }
  if(n1 > 1 && n2 > 1) {
    return compDist(imp,n1,n2); // calling another optimization
  }
  else { // one subtree has only one leaf
    Leaf **l1 = left->giveLeafs();
    Leaf **l2 = right->giveLeafs();
    for(j=0;j<n2;j++) 
      l2[j]->initNewSize(n1);
    for(i=0;i<n1;i++) {
      l1[i]->initNewSize(n2);
      // this function actually computes the set of optimal orders
      // of leftmost and rightmost leaves in the combined tree.
      l1[i]->addToNew(l2,l2,n1,n2,n2,n2,minInf,imp);
      l1[i]->replace();
    }
    for(j=0;j<n2;j++)
      l2[j]->replace();
  }
  return 0;
}
// optimization, perfromed for combining the last two subtrees
int Tree::lastTree(int imp,int n1,int n2) {

  Leaf **l1 = left->giveLeafs();
  Leaf **l2 = right->giveLeafs();
  int res = l1[0]->findLast(l1,l2,n1,n2,imp);
  return res;
}

// optimization which allows for early termination of the search
// see paper for details
int Tree::compDist(int imp,int tot1,int tot2) {

  Tree *t1 = left;
  Tree *t2 = right;
  Tree *t1l = t1->left;
  Tree *t1r = t1->right;
  Tree *t2l = t2->left;
  Tree *t2r = t2->right;
  int n1 = t1l->giveNumLeafs();
  int n2 = t1r->giveNumLeafs();
  int n3 = t2l->giveNumLeafs();
  int n4 = t2r->giveNumLeafs();
  Leaf **l1 = t1l->giveLeafs();
  Leaf **l2 = t1r->giveLeafs();
  Leaf **l3 = t2l->giveLeafs();
  Leaf **l4 = t2r->giveLeafs();
  Leaf **c2 = t2->giveLeafs();
  float mint1lt2l,mint1lt2r,mint1rt2l,mint1rt2r;
  mint1lt2l = mint1lt2r = mint1rt2l = mint1rt2r = 1;
  int i,j,i1,i2,j3,j4;
  // compute minimum similarity between genes in these 
  // two subtrees
  for(i=0;i<n1;i++) {
    i1 = l1[i]->giveIndex();
    for(j=0;j<n3;j++) {
      j3 = l3[j]->giveIndex();
      if(mat[i1][j3] < mint1rt2r) {
	mint1rt2r = mat[i1][j3];
      }
    }
    for(j=0;j<n4;j++) {
      j4 = l4[j]->giveIndex();
      if(mat[i1][j4] < mint1rt2l) {
	mint1rt2l = mat[i1][j4];
      }
    }
  }
  for(i=0;i<n2;i++) {
    i2 = l2[i]->giveIndex();
    for(j=0;j<n3;j++) {
      j3 = l3[j]->giveIndex();
      if(mat[i2][j3] < mint1lt2r) {
	mint1lt2r = mat[i2][j3];
      }
    }
    for(j=0;j<n4;j++) {
      j4 = l4[j]->giveIndex();
      if(mat[i2][j4] < mint1lt2l) {
	mint1lt2l = mat[i2][j4];
      }
    }
  }
  for(j=0;j<tot2;j++) 
    c2[j]->initNewSize(tot1);
  for(i=0;i<n1;i++) {
    l1[i]->initNewSize(tot2);
    // use precomuted minimums to terminate the search early
    l1[i]->addToNew(l4,l3,tot1,tot2,n4,n3,mint1lt2l,imp);
    l1[i]->addToNew(l3,l4,tot1,tot2,n3,n4,mint1lt2r,imp);
    l1[i]->replace();
  }
  for(i=0;i<n2;i++) {
    l2[i]->initNewSize(tot2);
    l2[i]->addToNew(l4,l3,tot1,tot2,n4,n3,mint1rt2l,imp);
    l2[i]->addToNew(l3,l4,tot1,tot2,n3,n4,mint1rt2r,imp);
    l2[i]->replace();
  }
  for(j=0;j<tot2;j++)
    c2[j]->replace();
  return 0;
}

// called by the main program to return the optimal order
// of the tree leaves
int *Tree::returnOrder(float *bestDist,int imp) {
  int start = compDist(imp);
  LeafPair *best;
  LeafDist **myDist = allLeafs[start]->giveList();
  (*bestDist) = myDist[0]->dist;
  best = myDist[0]->best;
  int *res = new int[numLeafs];
  // used to find the correct order of the leaves 
  // of the tree
  compTree(best->preLeft,res,0,best->n1-1,leftTree);
  compTree(best->preRight,res,best->n1,numLeafs-1,rightTree);
  return res;
}

// computes the new order of the leaves, based on the optimal
// ordering computation (using the 'best' struct).       
void Tree::compTree(LeafPair *best,int *res,int start,int last,int l) {
  if(start == last) {
    res[start] = best->leftLeaf;
    return;
  }
  if(l == leftTree) {
    compTree(best->preLeft,res,start,start+best->n1-1,leftTree);
    compTree(best->preRight,res,start+best->n1,last,rightTree);
  }
  if(l == rightTree) {
    compTree(best->preLeft,res,start+best->n2,last,rightTree);
    compTree(best->preRight,res,start,start+best->n2-1,leftTree);
  }
}

// computes the sum of similarities in the current order 
// of the tree leaves  
float Tree::curDist(float **m) {
  if(numLeafs == 1)
    return 0;
  float d1 = left->curDist(m);
  float d2 = right->curDist(m);
  int lCorner = left->findRight();
  int rCorner = right->findLeft();
  return(d1+d2+m[lCorner][rCorner]);
}

// find the rightmost leaf
int Tree::findRight() {
  if(numLeafs == 1)
    return allLeafs[0]->giveIndex();
  return right->findRight();
}

int Tree::findLeft() {
  if(numLeafs == 1)
    return allLeafs[0]->giveIndex();
  return left->findLeft();
}

// finds the initial order ofthe tree leaves
int *Tree::initOrder() {
  int *res = new int[numLeafs+1];
  fillArray(res,0,numLeafs-1);
  return res;
}

void Tree::fillArray(int *array,int s,int l) {
  if(numLeafs == 1) {
    array[s] = allLeafs[0]->giveIndex();
    return;
  }
  int n1 = left->giveNumLeafs();
  left->fillArray(array,s,s+n1-1);
  right->fillArray(array,s+n1,l);
  return;
}
  
