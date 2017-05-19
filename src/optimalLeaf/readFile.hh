/* This code provided by Ziv Bar-Joseph with the explicit understanding that 
 * no licensing is required for it to be used in the UCSC Genome Browser
 * as long as reference is made to this paper:
 * https://www.ncbi.nlm.nih.gov/pubmed/12801867
 */

//----------------------------------------------------
//                   readFile.hh
// reads the input file. See README for details on how this 
// file should look like.
// -----------------------------------------------------
#ifndef READFILE_HH
#define READFILE_HH



class readFile {
 private:
  int numGenes,expNum;
  float **vals;
  char **geneNames,**geneDesc,**expNames;
 public:
  readFile(char *fileName,int orfL, int descL);
  readFile( int numGenes, int expNum, float **vals, char **geneNames,char **geneDesc,char **expNames);
  int getNumGenes() {return numGenes;}
  int getNumExp() {return expNum;}
  float** getVals() {return vals;}
  char ** getNames() {return geneNames;}
  char ** getDesc() {return geneDesc;}
  char ** getExp() {return expNames;}
};
#endif
