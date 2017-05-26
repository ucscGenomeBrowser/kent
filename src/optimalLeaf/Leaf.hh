/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

// --------------------------------------------------------------------
//                         Leaf.hh
// This is where the optimal ordering for two subtrees 
// is comnputed. Uses the known optimal ordering for 
// each of the subtrees to generate the new ordering of the combined tree.
// ---------------------------------------------------------------------
#ifndef LEAF_HH
#define LEAF_HH

// used for every possible pair of rightmost and leftmost leaves
// to find the best leaves in the intersection for this pair 
class LeafPair {
public:
  LeafPair(int l,int r,LeafPair *pl,LeafPair *pr,int t1,int t2);
  int leftLeaf;
  int rightLeaf;
  LeafPair *preLeft;
  LeafPair *preRight;
  int n1,n2;
};

// store the distance for this LeafPair
struct LeafDist {
  LeafDist(int to,float d,LeafPair *p) {n=to; dist = d;best = p;}
  int n;
  float dist;
  LeafPair *best;
};

class Leaf {
 private:
  int index;
  LeafDist **curDist,**newDist;
  float **distMat;
  int listSize,newSize;
  LeafPair *bestPair;
 public:
  Leaf(int num, float **mat);
  int giveIndex() {return index;}
  void setSize(int size) {listSize = size;}
  int giveSize() {return listSize;}
  LeafDist **giveList() {return curDist;}
  void replace();
  void initNewSize(int nSize) {newDist = new LeafDist*[nSize];}
  void initNewDist() {newDist = 0;}
  void addToNew(Leaf **corner1,Leaf **corner2,int n1,int n2,int c1,int c2,float max1,int tot);
  void addNewDist(int n,float dist,LeafPair *p);
  LeafPair *findMin(float *min);
  float bestNew;
  int findLast(Leaf **corner1,Leaf **corner2,int n1,int n2,int tot);
};
#endif




