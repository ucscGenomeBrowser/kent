/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

//----------------------------------------------------
//                   readFile.hh
// reads the input file. 
// -----------------------------------------------------
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "readFile.hh"

using namespace std;
const int MAXSIZE = 200000;
const float missing = 0.75;

// reads the file and genrates the expression matrix and 
// retrieves gene names and experiment names.
readFile::readFile(char *fileName,int orfL, int descL) {
  FILE *from=fopen(fileName,"r");  
  char findName[5];
  int isName=0;
  int lineLen = orfL+descL+MAXSIZE;
  char *buffer = new char[lineLen];
  char geneVal[MAXSIZE];
  char **lines,**temp;
  int i, j, k,num, len,numT,place;
  char *tok, *newtok;
  int curLines = 100; // initial guess for number of genes
  int nlines=0; // actual number of lines
  int numMiss,removeGenes = 0;
  int redI = 0;
  lines = new char*[curLines];
  for(i=0;i<curLines;i++) {
    lines[i] = new char[lineLen];
  }
  while(fgets(buffer,lineLen,from)) {
    strcpy(lines[nlines],buffer);
    nlines++;
    if(nlines == curLines) { // overflow, add new lines
      curLines = curLines * 2;
      temp = new char*[nlines];
      for(i=0;i<nlines;i++) {
	temp[i] = new char[lineLen];
	strcpy(temp[i],lines[i]);
	delete []lines[i];
      }
      delete []lines;
      lines = new char*[curLines];
      for(i=0;i<curLines;i++) {
	lines[i] = new char[lineLen]; 
        if(i < nlines) {
	  strcpy(lines[i],temp[i]); // copy all existing lines
	  delete []temp[i];
	}
      }
      delete []temp;
    }
  }
  /* add '\n' at the end of last line if needed */
  if(!strchr(lines[nlines-1],'\n')) {
    len=strlen(lines[nlines-1]);
    lines[nlines-1][len]='\n';
    lines[nlines-1][len+1]=0;
  }
  
  // compute the number of values in the first line, and find if 'name' was
  // used
  strcpy(findName,"name");
  
  len=strlen(lines[0]);
  j = 0;
  numT = 0;
  while(j < len && numT == 0) {
    if(lines[0][j]=='\t') 
      numT++;
    j++;
  }
  i = 0;
  // check if the 'name' column is present in the input file
  while(j < len && numT == 1) {
    buffer[i] = lines[0][j];
    if(lines[0][j]=='\t') 
      numT++;   
    j++;
    i++;
  }
  buffer[i-1] = '\0';
  if(strcmp(buffer,findName) == 0) 
    isName = 1;
  numT = 0;
  j=0;
  while(lines[0][j]!='\n') {
    if(lines[0][j]=='\t')
      numT++;
    j++;
  }
  numT++; // end of line
  numGenes = nlines - 1;
  expNum = numT - 1- isName;
  geneNames = new char*[numGenes];
  geneDesc = new char*[numGenes];
  vals = new float*[numGenes];
  expNames = new char*[expNum];
  for(i=0;i<numGenes;i++) {
    geneNames[i] = new char[orfL];
    geneDesc[i] = new char[descL];
    vals[i] = new float[expNum];
  }
  for(i=0;i<expNum;i++) {
    expNames[i] = new char[descL];
  } 
  j = 0;
  numT = 0;
  while(j<len && numT < (1+isName)) { 
     if(isspace(lines[0][j])) {
      numT++;
    }
    j++;
  }
  place = 0;
  // find experiment names
  for(i=0;i<expNum;i++) {
    while(isspace(lines[0][j]) == 0) {
      expNames[i][place] = lines[0][j];
      j++;
      place++;
    }
    j++;
    expNames[i][place] = '\0';
    place = 0;
  }
  // get the gene values
  for(i=0;i<numGenes;i++) {
    j=0;
    place = 0;
     while(isspace(lines[i+1][j]) == 0) {
      geneNames[i-redI][place] = lines[i+1][j];
      place++;
      j++;
    }
    geneNames[i-redI][place] = '\0';
    place = 0;
    j++;
    if(isName) {
       while(isspace(lines[i+1][j]) == 0) {
	 geneDesc[i-redI][place] = lines[i+1][j];
	 place++;
	 j++;
       }
       geneDesc[i-redI][place] = '\0';
    }
    else {
      strcpy(geneDesc[i-redI],geneNames[i-redI]);
    }
    place = 0;
    j++;
    numMiss = 0; // the number of missing values
    for(k=0;k<expNum;k++) {
      place = 0;
      while(isspace(lines[i+1][j]) == 0) {
	geneVal[place] = lines[i+1][j];
	place++;
	j++;
      }
      geneVal[place] = '\0';
      if(geneVal[0] == '\0') {
        vals[i-redI][k] = 110; // missing values are replaced by a number
                          // greater than 100, however when writing the 
                          // output file, they are restored.
        numMiss++;
      }
      else
	vals[i-redI][k] = atof(geneVal); 
      j++;
    }
    if(numMiss > (int)ceil(missing*expNum) || (numMiss + 1) == expNum) { // too many missing values
      removeGenes++;
      redI++;
    }
  }
  for(i=0;i<curLines;i++) {
    delete []lines[i];
  }
  delete []lines;
  numGenes = numGenes - removeGenes;  
  cerr<<"removed "<<removeGenes<<" genes due to missing values"<<'\n';
}

readFile::readFile( int inumGenes, int iexpNum, float **ivals, char **igeneNames,char **igeneDesc,char **iexpNames)
{
numGenes = inumGenes;
expNum = iexpNum;
vals = ivals;
geneNames = igeneNames;
geneDesc = igeneDesc;
expNames = expNames;
}
