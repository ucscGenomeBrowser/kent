/*
  The straw library (straw.cpp, straw.h) is used with permission of the Aiden
  Lab at the Broad Institute.  It is included below, along with modifications
  to work in the environment of the UCSC Genome Browser, under the following
  license:

  The MIT License (MIT)
 
  Copyright (c) 2011-2016 Broad Institute, Aiden Lab
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/
#ifndef STRAW_H
#define STRAW_H

#include <fstream>
#include <set>
#include <vector>

// pointer structure for reading blocks or matrices, holds the size and position 
struct indexEntry {
  int size;
  long position;
};

// sparse matrix entry
struct contactRecord {
  int binX;
  int binY;
  float counts;
};

bool readMagicString(std::ifstream& fin);
long readHeader(std::istream& fin, std::string chr1, std::string chr2, int &c1pos1, int &c1pos2, int &c2pos1, int &c2pos2, int &chr1ind, int &chr2ind);
void readFooter(std::istream& fin, long master, int c1, int c2, std::string norm, std::string unit, int resolution, long &myFilePos, indexEntry &c1NormEntry, indexEntry &c2NormEntry);
bool readMatrixZoomData(std::istream& fin, std::string myunit, int mybinsize, int &myBlockBinCount, int &myBlockColumnCount);
void readMatrix(std::istream& fin, int myFilePosition, std::string unit, int resolution, int &myBlockBinCount, int &myBlockColumnCount);
std::set<int> getBlockNumbersForRegionFromBinPosition(int* regionIndices, int blockBinCount, int blockColumnCount, bool intra);
std::vector<contactRecord> readBlock(std::istream& fin, int blockNumber);
std::vector<double> readNormalizationVector(std::istream& fin, indexEntry entry);
void straw(std::string norm, std::string fname, int binsize, std::string chr1loc, std::string chr2loc, std::string unit, std::vector<int>& xActual, std::vector<int>& yActual, std::vector<float>& counts);

extern "C" char* Cstraw (char *norm, char *fname, int binsize, char *chr1loc, char *chr2loc, char *unit, int **xActual, int **yActual, double **counts, int *numRecords);

extern "C" char *CstrawHeader (char *filename, char **genome, char ***chromNames, int *nChroms, char ***bpResolutions, int *nBpRes, char ***fragResolutions, int *nFragRes);
#endif
