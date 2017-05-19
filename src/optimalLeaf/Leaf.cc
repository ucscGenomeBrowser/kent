/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

// --------------------------------------------------------------------
//                         Leaf.cc
// This is where the optimal ordering for two subtrees 
// is comnputed. Uses the known optimal ordering for 
// each of the subtrees to generate the new ordering of the combined tree.
// ---------------------------------------------------------------------
//#include <iostream.h>
#include <stdlib.h>
#include "Leaf.hh"
#include "general.hh"

int compDist(const void *d1,const void *d2);

// Pair for best tree construction
LeafPair::LeafPair(int l,int r,LeafPair *pl,LeafPair *pr,int t1,int t2) {
  leftLeaf = l;
  rightLeaf = r;
  preLeft = pl;
  preRight = pr;
  n1 = t1;
  n2 = t2;
}

// Each gene is assigned a leaf, initially it does not have 
// any pairing gene.
Leaf::Leaf(int num,float **mat) {
  index = num;
  distMat = mat; 
  LeafPair *oneLeaf = new LeafPair(index,-1,0,0,1,0); 
  curDist = new LeafDist*[1];
  curDist[0] = new LeafDist(index,0,oneLeaf);
  listSize = 1;
  newDist = 0;
  newSize = 0;
  bestNew = maxInf;
}

// replace previous pair list (from previous subtree) with 
// a new one from the current subtree
void Leaf::replace() {

  int i;
  // delete previous distance list
  for(i=0;i<listSize;i++) 
    delete curDist[i];
  delete []curDist;

  listSize = newSize;
  // bestNew becomes the maximum distance that may help in future
  // trees.
  bestNew += maxAdd;
  curDist = new LeafDist*[listSize];
  for(i=0;i<newSize;i++) {
    curDist[i] = newDist[i];
  }
  delete []newDist;
  qsort(curDist,listSize,sizeof(LeafDist*),compDist);
  newSize = 0;
  bestNew = maxInf;
}

// Adds corner to list of corners, and finds the best distance
// with corner on the other side.
void Leaf::addToNew(Leaf **corner1,Leaf **corner2,int n1,int n2,int c1,int c2,float max1,int tot) {
 
  int fromNum,fromIndex;
  LeafDist **fDist;
  Leaf *curLeaf;
  int i,j;
  float *bestC = new float[tot+1];
  float curVal,bestD;
  float bestPos;
  int *bForC = new int[tot+1];
  int cForD,dPlace;
  float maxAC = maxInf;
  for(j=0;j<c1;j++) {
    fromIndex = corner1[j]->giveIndex();
    bestC[fromIndex] = maxInf;
    for(i=0;i<listSize;i++) {
      bestPos = curDist[i]->dist + max1;
      if(bestPos > bestC[fromIndex]) // optimization
	i = listSize;
      else {
	curVal = curDist[i]->dist + distMat[curDist[i]->n][fromIndex];
	if(curVal < bestC[fromIndex]) {
	  bestC[fromIndex] = curVal;
	  bForC[fromIndex] = i;
	}
      }
    }
    if(bestC[fromIndex] < maxAC)
      maxAC = bestC[fromIndex];
  }

  for(j=0;j<c2;j++) {
    bestD = maxInf;
    curLeaf = corner2[j];
    fromNum = curLeaf->giveSize();
    fromIndex = curLeaf->giveIndex();
    fDist = curLeaf->giveList();
    for(i=0;i<fromNum;i++) {
      bestPos = curLeaf->curDist[i]->dist + maxAC;
      if(bestPos > bestD) 
	i = fromNum; // optimization
      else { 
	curVal = bestC[fDist[i]->n] + curLeaf->curDist[i]->dist;
	if(curVal < bestD) {
	  bestD = curVal;
	  cForD = curLeaf->curDist[i]->n;
	  dPlace = i;
	}
      }
    }
    LeafPair *newPair = new LeafPair(index,fromIndex,
            curDist[bForC[cForD]]->best,fDist[dPlace]->best,n1,n2);
    newDist[newSize] = new LeafDist(fromIndex,bestD,newPair);    
    newSize++;
    LeafPair *cornerPair = new LeafPair(fromIndex,index,
		      fDist[dPlace]->best,curDist[bForC[cForD]]->best,n2,n1);
    curLeaf->addNewDist(index,bestD,cornerPair);
  }
  delete []bForC;
  delete []bestC;
}

// adds a new pair to the dist list
void Leaf::addNewDist(int n,float dist,LeafPair *p) {
  
  newDist[newSize] = new LeafDist(n,dist,p);    
  newSize++;
}

// find the minimum distance pair (since the list are ordered,
// this is always the first pair on the list).
LeafPair *Leaf::findMin(float *min) {
  
  (*min) = curDist[0]->dist;
  return curDist[0]->best;
}

// used for sorting the lists
int compDist(const void *d1,const void *d2) {
  LeafDist **l1 = (LeafDist**)d1;
  LeafDist **l2 = (LeafDist**)d2;
  return((*l1)->dist > (*l2)->dist);
}

// the optimzation for the last two subtrees, no need 
// to compute distance to all leaves, the best will suffice (see paper).
int Leaf::findLast(Leaf **corner1,Leaf **corner2,int n1,int n2,int tot) {
 
  LeafDist **myDist;
  Leaf *curLeaf;
  int i,j;
  float curVal,best = maxInf;
  float myVal;
  int myInd,bestIndl,bestIndr,mBestl,mBestr;
  LeafPair *lpre,*rpre;
  LeafDist ***fDist = new LeafDist**[n2];
  for(i=0;i<n2;i++) {
    fDist[i] = corner2[i]->giveList();
  }
  for(j=0;j<n1;j++) {
    curLeaf = corner1[j];
    myDist = curLeaf->giveList();
    myVal = myDist[0]->dist;
    myInd = curLeaf->giveIndex();
    for(i=0;i<n2;i++) {
      curVal = myVal + fDist[i][0]->dist + distMat[myInd][corner2[i]->giveIndex()];
      if(best > curVal) {
	best = curVal;
	bestIndl = myDist[0]->n;
	bestIndr = fDist[i][0]->n;
	mBestl = myInd;
	mBestr = corner2[i]->giveIndex();
      }
    }
  }
  LeafPair *newPair;
  int place,size;
  for(i=0;i<n2;i++) {
    if(corner2[i]->giveIndex() == bestIndr) {
      size = corner2[i]->giveSize();
      for(j=0;j<size;j++) {
	if(fDist[i][j]->n == mBestr) {
	  rpre = fDist[i][j]->best;
	  j = size;
	}
      }
      i = n2;
    }
  }
 	  
  for(i=0;i<n1;i++) {
    if(corner1[i]->giveIndex() == bestIndl) {
      size = corner1[i]->giveSize();
      myDist = corner1[i]->giveList();
      for(j=0;j<size;j++) {
	if(myDist[j]->n == mBestl) {
	  lpre = myDist[j]->best;
	  j = size;
	}
      }
      newPair = new LeafPair(bestIndl,bestIndr,lpre,rpre,n1,n2);
      place = i;
      corner1[i]->initNewSize(1);
      corner1[i]->addNewDist(bestIndr,best,newPair);
      corner1[i]->replace();
      i = n1;
    }
  }
  delete []fDist;
  return place;
}




