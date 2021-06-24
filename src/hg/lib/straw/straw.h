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
#include <stdexcept>

/* Simple exception wrapper class */
class strawException : public std::runtime_error {
  public:
  strawException(const std::string& error):
    std::runtime_error(error) {
  }
};


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

// The combination of chrom index, normalization, unit, and resolution that serves as an index
// into the block map.
class normVector {
  public:
  int chrIdx;
  std::string normtype;
  std::string unit;
  int resolution;

  normVector(int c, std::string n, std::string u, int r) {
    chrIdx = c;
    normtype = n;
    unit = u;
    resolution = r;
  }
  bool operator <(const normVector &other) const {
    if (chrIdx != other.chrIdx)
      return chrIdx < other.chrIdx;
    else if (normtype != other.normtype)
      return normtype < other.normtype;
    else if (unit != other.unit)
      return unit < other.unit;
    else
      return resolution < other.resolution;
  }
};

class Straw {
    private:
    // map of block numbers to pointers
    std::map <int, indexEntry> blockMap;
    bool haveReadFooter;
    std::map <std::string, long> chrchrMap;
    std::map <normVector, std::pair<long,int>> normVectors;

    long master;
    std::string fileName;

    int fileIsOpen();
    bool readMagicString();

    public:
    // version number
    int version;
    long total_bytes;
    std::string genome;
    int nChrs;
    std::vector<std::string> chrNames;
    std::vector<int> chrSizes;
    std::map <std::string, std::string> attributes;
    std::vector<int> bpResolutions;
    std::vector<int> fragResolutions;


    void loadHeader();
    bool readMatrixZoomData(std::istream& fin, std::string myunit, int mybinsize, int &myBlockBinCount, int &myBlockColumnCount);
    bool readMatrixZoomDataHttp(char *url, long &myFilePosition, std::string myunit, int mybinsize, int &myBlockBinCount, int &myBlockColumnCount);
    void readMatrixHttp(char *url, long myFilePosition, std::string unit, int resolution, int &myBlockBinCount, int &myBlockColumnCount);
    void readMatrix(std::istream& fin, long myFilePosition, std::string unit, int resolution, int &myBlockBinCount, int &myBlockColumnCount);
    std::set<int> getBlockNumbersForRegionFromBinPosition(int* regionIndices, int blockBinCount, int blockColumnCount, bool intra);
    std::vector<contactRecord> readBlock(std::istream& fin, char *url, bool isHttp, int blockNumber);
    std::vector<double> readNormalizationVector(std::istream& bufferin);

    Straw(std::string fname) {
        master = 0;
        fileName = "";
        genome = "";
        chrNames.clear();
        chrSizes.clear();
        attributes.clear();
        blockMap.clear();
        haveReadFooter = false;
        open(fname);
    }

    ~Straw() {
    }

    int open(std::string fname);
    void close();
    void getChrInfo(std::string chr1, std::string chr2, int &c1pos1, int &c1pos2, int &c2pos1, int &c2pos2, int &chr1ind, int &chr2ind);
    void readFooter(std::istream& fin, long master, int c1, int c2, std::string norm, std::string unit, int resolution, long &myFilePos, indexEntry &c1NormEntry, indexEntry &c2NormEntry);
    void straw(std::string norm, int binsize, std::string chr1loc, std::string chr2loc, std::string unit, std::vector<int> &xActual, std::vector<int> &yActual, std::vector<float> &counts);

}; // Cstraw class

#endif
