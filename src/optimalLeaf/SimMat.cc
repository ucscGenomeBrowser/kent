/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

// --------------------------------------------------------
//                     SimMat.cc
// Computes the similarity matrix, and writes ordered trees 
// to a .CDT files.
// --------------------------------------------------------
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <math.h>
#include "SimMat.hh"
#include "readFile.hh"

using namespace std;
// read the input file
SimMat::SimMat(char *filename,int orfL,int descL,int mZ) {

  meanZ = mZ;
  readFile myRead(filename,orfL,descL);
  genes = myRead.getNumGenes();
  exp = myRead.getNumExp();
  in = myRead.getVals();
  geneNames = myRead.getNames();
  geneDesc = myRead.getDesc();
  expNames = myRead.getExp();
  generateMat();
}

SimMat::SimMat( int igenes, int iexp, int imeanZ, float **iin,
  char **igeneNames, char **igeneDesc, char **iexpNames) {

  genes = igenes;
  exp = iexp;
  meanZ = imeanZ;
  in = iin;
  geneNames = igeneNames;
  geneDesc = igeneDesc;
  expNames = iexpNames;

  generateMat();
}

// compute the siimlarity matrix
void SimMat::generateMat() {
  
  int i,j;
  mat = new float*[genes+1];
  means = compMean(genes,exp,1);
  std = compStd(genes,exp,1);
  for(i=1;i<genes+1;i++) {
    mat[i] = new float[genes+1];
    mat[i][i] = 0;
  }
  for(i=1;i<genes;i++) {
    for(j=i+1;j<genes+1;j++) {
      mat[i][j] = -1 * compSim(i-1,j-1,exp,1);
      mat[j][i] = mat[i][j];
    }
  } 
}

// compute the columns siimlarity matrix
void SimMat::generateCols(char *name) {
  
  int i,j;
  delete []means;
  delete []std;
  float simVal;
  ofstream toSim(name);
  cMat = new float*[exp+1];
  means = compMean(exp,genes,0);
  std = compStd(exp,genes,0);
  for(i=1;i<exp+1;i++) {
    cMat[i] = new float[exp+1];
    cMat[i][i] = 0;
  }
  for(i=1;i<exp;i++) {
    for(j=i+1;j<exp+1;j++) {
      simVal = compSim(i-1,j-1,genes,0);
      cMat[i][j] = -1 * simVal;
      cMat[j][i] = cMat[i][j];
    }
  } 
}
// computes average expressions of genes
float *SimMat::compMean(int tot,int c,int isGene) {
  int i,j;
  float *res = new float[tot];
  float sum;
  int num;
  for(i=0;i<tot;i++) {
    sum = 0;
    num = 0;
    for(j=0;j<c;j++) {
      if(isGene == 1) {
	if(in[i][j] < 100) {
	  sum += in[i][j];
	  num++;
	}
      }
      else {
        if(in[j][i] < 100) {
	  sum += in[j][i];
	  num++;
	}
      }
    }
    res[i] = sum/(float)num;
    if(meanZ)
      res[i] = 0;
  }
  return res;
}
// computes standard devaition of genes
float *SimMat::compStd(int tot,int c, int isGene) {

  int i,j;
  float *res = new float[tot];
  float sum;
  int num;
  for(i=0;i<tot;i++) {
    sum = 0;
    num = 0;
    for(j=0;j<c;j++) {
      if(isGene == 1) {
	if(in[i][j] < 100) {
	  sum += pow((in[i][j]-means[i]),2);
	  num++;
	}
      }
      else {
        if(in[j][i] < 100) {
	  sum += pow((in[j][i]-means[i]),2);
	  num++;
	}
      }
      
    }
    sum = sum/(float)num;
    if(sum == 0) {
      sum = 1;
    }
    res[i] = sqrt(sum);
  }
  return res;
}

float SimMat::compSim(int g1,int g2,int c,int isGene) {
  int i;
  float sum = 0;
  int num = 0;
  for(i=0;i<c;i++) {
    if(isGene == 1) {
      if(in[g1][i] < 100 && in[g2][i] < 100) {
	sum += ((in[g1][i]-means[g1])/std[g1])*((in[g2][i]-means[g2])/std[g2]);
	num++;
      }
    }
    else {
      if(in[i][g1] < 100 && in[i][g2] < 100) {
	sum += ((in[i][g1]-means[g1])/std[g1])*((in[i][g2]-means[g2])/std[g2]);
	num++;
      }
    }   
  }
  if(num == 0) 
    sum = -1;
  else
    sum = sum/(float)num;
  return sum;
}

// gets an array representing genes, and writes it to a .CDT file 
void SimMat::printOrder(int *arr,int *arrC,char *name) {

  ofstream to(name);
  int i,j;
  to<<"GID"<<'\t'<<"gene"<<'\t'<<"NAME"<<'\t'<<"GWEIGHT";
  for(i=0;i<exp;i++) {
    to<<'\t'<<expNames[arrC[i]-1]<<"-"<<arrC[i];
  }
  to<<'\n';
  to<<"AID"<<'\t'<<'\t'<<'\t';
  for(i=0;i<exp;i++) {
    to<<'\t'<<"ARRY"<<arrC[i]<<"X";
  }
  to<<'\n';
  to<<"EWEIGHT"<<'\t'<<'\t'<<'\t';
  for(i=1;i<exp+1;i++) {
    to<<'\t'<<"1";
  }
  to<<'\n';
  for(i=0;i<genes;i++) {
    to<<"GENE"<<arr[i]<<"X"<<'\t'<<geneNames[arr[i]-1]<<'\t'<<geneDesc[arr[i]-1]<<'\t'<<"1";
    for(j=0;j<exp;j++) {
      if(in[arr[i]-1][arrC[j]-1] < 100)
	to<<'\t'<<in[arr[i]-1][arrC[j]-1];
      else
	to<<'\t';
    }
    to<<'\n';
  }
}
      













