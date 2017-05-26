/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

// --------------------------------------------------------------------
//             Tree.hh
// data structure for the hierarchical clustering tree.
// Used to compute initial and optimal similarities, and to manipulate
// (based on the optimal ordering) the ordering the tree nodes descendants.
// -------------------------------------------------------------------------
#ifndef TREE_HH
#define TREE_HH

#include "Leaf.hh"

class Tree {
 private:
  Tree *left,*right; // left and right subtrees
  int numLeafs;
  Leaf **allLeafs;
  float **mat;  // similarity matrix
  int nodeNum; // id for this subtree
 public:
  Tree(int index,float **m);
  Tree(Tree *t1,Tree *t2,int nNum);
  float **giveMat() {return mat;}
  int giveNumLeafs() {return numLeafs;}
  Leaf **giveLeafs() {return allLeafs;}
  int compDist(int imp);
  int compDist(int imp,int tot1,int tot2);
  int *returnOrder(float *bestDist,int imp);
  float curDist(float **m); 
  int findRight();
  int findLeft();
  int *initOrder();
  void fillArray(int *array,int s,int l);
  void compTree(LeafPair *best,int *res,int start,int last,int l);
  int isLeaf() {return(numLeafs == 1);}
  int giveIndex() {return nodeNum;}
  int lastTree(int imp,int n1,int n2);
};
#endif
