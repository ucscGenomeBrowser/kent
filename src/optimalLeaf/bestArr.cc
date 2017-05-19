/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

//-----------------------------------------------------------
//                 Bestarrange.cc
// Builds an hierarchical clustering tree, and finds its optimal
// leaf ordering. The program gets as an input an expression 
// file (see README) and generates four files as output.
// The first two (for an input file named foo.txt, these are called fooIn.GTR 
// and fooIn.CDT) are the hierarchical clustering reuslts. The other two
// files (foo.CDT and foo.GTR) are the optima leaf ordering results.
// Both these results can be viewed using Michael Eisen's TreeView 
// software. 
//----------------------------------------------------------

#include <stdlib.h>
#include <iostream>
#include <fstream>
using namespace std;
#include <string.h>
#include <stdio.h>
#include "general.hh"
#include "Tree.hh"
#include "SimMat.hh"

extern "C"
{
#include "optimalLeaf.h"
}

const int BUFSIZE = 100;
Tree *createTree(int num,float **m,char *str);
char *nameFile(char *name,char *e,char *m);
void copyFile(char *from, char *to);

#ifdef NOTNOW
main(int argc,char** argv) {

  int i,len = strlen(argv[1]);
  char *fullName = new char[len+6];
  strcpy(fullName,argv[1]);
  strcat(fullName,".txt");
  fullName[len+5] = '\0';
  int newD = descLen; // initial maximum length of description
  int newO = orfLen; // initial maximum length of orf name
  int meanZ = 0;
  int mult = 0;
  if(argc>2) {
    for(i=3;i<=argc;i++) {
       if(argv[i-1][0] == '-' && argv[i-1][1] == 'd')
         newD = atoi(argv[i-1]+2);
       if(argv[i-1][0] == '-' && argv[i-1][1] == 'o')
         newO = atoi(argv[i-1]+2);
       if(argv[i-1][0] == '-' && argv[i-1][1] == 'c')
         meanZ = 1;
       if(argv[i-1][0] == '-' && argv[i-1][1] == 'm')
         mult = 1;
    }
  }
  SimMat inputData(fullName,newO,newD,meanZ);
  // output files names
  char *cdtFile = nameFile(argv[1],".CDT",0);
  char *gtrFile = nameFile(argv[1],".GTR",0);
  char *inCdtFile = nameFile(argv[1],".CDT","In");
  char *inGtrFile = nameFile(argv[1],".GTR","In");
  char *atrFile = nameFile(argv[1],".ATR",0);
  float **mat = inputData.giveMat(); // similarity matrix
  int num = inputData.giveGenes();
  Tree *t = createTree(num,mat,gtrFile,"GENE"); // hierarchical clustering tree
  copyFile(gtrFile,inGtrFile);
  float init = -1*t->curDist(mat); // initial ordering score
  cerr<<"Initial sum of similarities "<<init<<'\n';
  int *order = t->initOrder();
  float min;
  int *arr = t->returnOrder(&min,num); // the optimal leaf algorithm call
  min = -1*min;
  cerr<<"Optimal sum of similarities: "<<min<<'\n';
  cerr<<"Improvement "<<(min-init)/(init)*100<<"%"<<'\n';
  int numC = inputData.giveExp();
  int *arrC = new int[numC+1];
  for(i=0;i<numC;i++)
    arrC[i] = i+1;
  inputData.printOrder(order,arrC,inCdtFile); // writes initial order to fooIn.CDT
  float **cMat;
  if(mult == 1) {
    inputData.generateCols(argv[2]);
    cMat = inputData.giveCols(); // similarity matrix
    Tree *tc = createTree(numC,cMat,atrFile,"ARRY"); // hierarchical clustering tree
    float initC = -1*tc->curDist(cMat); // initial ordering score
    int *orderC = tc->initOrder();
    float minC;
    delete []arrC;
    arrC = tc->returnOrder(&minC,numC); // the optimal leaf algorithm call
    minC = -1*minC;
    cerr<<"Initial column similarity: "<<initC<<'\n';
    cerr<<"Optimal column similarity: "<<minC<<'\n';
    cerr<<"Improvement "<<(minC-initC)/(initC)*100<<"%"<<'\n';
  }  
  inputData.printOrder(arr,arrC,cdtFile); // write optimal order to foo.CDT
}
#endif

  
// Performs the hierarchical clustering, and generates the tree
// corresponding to this clustering
Tree *createTree(int num,float **m) {
  int i,j,k;
  float **newM = new float*[num+1];
  //ofstream to(name); // .GTR file
  Tree **allTrees = new Tree*[num+1];
  // copy similarity matrix
  for(i=1;i<num+1;i++) {
    newM[i] = new float[num+1];
    for(j=1;j<num+1;j++) {
      newM[i][j] = m[i][j];
    }
  }
  // initially each gene is assigned to its own cluster
  for(i=1;i<num+1;i++) {
    allTrees[i] = new Tree(i,m);
  }
  float max;
  int r,l;
  float lSize,rSize;
  Tree *temp;
  for(i=1;i<num;i++) {
    max = minInf;
    // find minimum entry in (converted) similarity matrix
    for(k=1;k<num;k++) {
      if(allTrees[k] != 0) {
	for(j=k+1;j<num+1;j++) {
	  if(allTrees[j] != 0) {
	    if(max < (-1* newM[j][k])) {
	      max = (-1 * newM[j][k]);
	      l = k;
	      r = j;
	    }
	  }
	}
      }
    }
    rSize = allTrees[r]->giveNumLeafs();
    lSize = allTrees[l]->giveNumLeafs();
#ifdef NOTNOW
    to<<"NODE"<<i<<"X"<<'\t';
    if(allTrees[l]->isLeaf()) 
      to<<str<<allTrees[l]->giveIndex()<<"X"<<'\t';
    else
      to<<"NODE"<<allTrees[l]->giveIndex()<<"X"<<'\t';
    if(allTrees[r]->isLeaf()) 
      to<<str<<allTrees[r]->giveIndex()<<"X"<<'\t';
    else
      to<<"NODE"<<allTrees[r]->giveIndex()<<"X"<<'\t';
    to<<max<<'\n';
#endif

    temp = allTrees[l];
    allTrees[l] = 0;
    // combine the two clusters
    allTrees[l] = new Tree(temp,allTrees[r],i);
    allTrees[r] = 0;
    // update similarity matrix
    for(j=1;j<num+1;j++) {
      if(allTrees[j] != 0 && j != l) {
	newM[j][l] = (lSize*newM[j][l] + rSize*newM[j][r])/(lSize+rSize);
	newM[l][j] = newM[j][l];
      }
    }
  }
  Tree *res;
  // find root of the tree
  for(i=1;i<num+1;i++) {
    if(allTrees[i] != 0) { 
      res = allTrees[i];
    }
  }
  for(i=1;i<num+1;i++) {
    delete []newM[i];
  }
  delete []newM;
  return res;
}  

// genearte file names with appropriate extension
char *nameFile(char *name,char *e,char *m) {

  int len = strlen(name) + strlen(e);
  if(m != 0)
    len = len + strlen(m);
  char *res = new char[len + 4];
  strcpy(res,name);
  if(m != 0)
    strcat(res,m);
  strcat(res,e);
  return res;
}

// copy one file to the other
void copyFile(char *from, char *to) {

  FILE *readFrom=fopen(from,"r");
  FILE *writeTo=fopen(to,"w");
  int size;
  char buffer[BUFSIZE];
  while((size = fread(buffer,sizeof(char),BUFSIZE,readFrom)) == BUFSIZE)
    fwrite(buffer,sizeof(char),BUFSIZE,writeTo);
  fwrite(buffer,sizeof(char),size,writeTo);
  fclose(readFrom);
  fclose(writeTo);
}

extern "C"
{
int *optimalLeafOrder(int inumGenes, int iexpNum, int imeanZ, float **ivals, char **igeneNames,char **igeneDesc,char **iexpNames)
{
//int foo;
//SimMat inputData( inumGenes, iexpNum, ivals, igeneNames,igeneDesc,iexpNames);
  //char *fullName;
  //int newO, newD, meanZ, num;
//int inumGenes; int iexpNum; float **ivals; char **igeneNames;char **igeneDesc;char **iexpNames;
  //SimMat inputData(fullName,newO,newD,meanZ);
    SimMat inputData(inumGenes, iexpNum, imeanZ, ivals, igeneNames,igeneDesc,iexpNames);
  float **mat = inputData.giveMat(); // similarity matrix
  int num = inputData.giveGenes();
  Tree *t = createTree(num,mat); // hierarchical clustering tree
  int *orderC = t->initOrder();
    float min;
      int *arr = t->returnOrder(&min,num); // the optimal leaf algorithm call

return arr;
}

}
