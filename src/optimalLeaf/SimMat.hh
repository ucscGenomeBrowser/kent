/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

// --------------------------------------------------------
//                     SimMat.hh
// Computes the similarity matrix, and writes ordered trees 
// to a .CDT files.
// --------------------------------------------------------
#ifndef SIMMAT_HH
#define SIMMAT_HH



class SimMat {
 private:
  float **mat,**cMat; // similarity matrix
  int genes,exp; // number of genes and experiments
  int *order,meanZ;
  float **in,*means,*std;
  char **geneNames,**geneDesc,**expNames;
 public:
  SimMat( int igenes, int iexp, int imeanZ, float **iin, char **igeneNames, char **igeneDesc, char **iexpNames);
  SimMat(char *filename,int orfL,int descL,int mZ);
  //SimMat(int genes,int exp);
  void generateMat();
  void generateCols(char *name); // generating similarity matrix for columns
  float **giveMat() {return mat;}
  float **giveCols() {return cMat;}
  int giveGenes() {return genes;}
  int giveExp() {return exp;}
  float *compMean(int tot,int c,int isGene);
  float *compStd(int tot,int c,int isGene);
  float compSim(int g1,int g2,int c,int isGene);
  void printOrder(int *arr,int *arrC,char *name);
};
#endif
