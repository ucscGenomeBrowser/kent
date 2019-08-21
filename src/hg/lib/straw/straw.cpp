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
#include <cstring>
#include <iostream>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <streambuf>
//#include <curl/curl.h>
#include "zlib.h"
#include "straw.h"
#include "udcWrapper.h"
using namespace std;

/* Simple exception wrapper class */
class strawException: public std::runtime_error {
  public:
  strawException(const std::string& error):
    std::runtime_error(error) {
  }
};


/*
  Straw: fast C++ implementation of dump. Not as fully featured as the
  Java version. Reads the .hic file, finds the appropriate matrix and slice
  of data, and outputs as text in sparse upper triangular format.

  Currently only supporting matrices.

  Usage: straw <NONE/VC/VC_SQRT/KR> <hicFile(s)> <chr1>[:x1:x2] <chr2>[:y1:y2] <BP/FRAG> <binsize> 
 */
// this is for creating a stream from a byte array for ease of use
struct membuf : std::streambuf
{
  membuf(char* begin, char* end) {
    this->setg(begin, begin, end);
  }
};

// version number
static int version;

// map of block numbers to pointers
map <int, indexEntry> blockMap;

long total_bytes;


// get a buffer that can be used as an input stream from the URL
char* getHttpData(char *url, long position, long chunksize) {
  char *buffer = (char*) malloc(chunksize);
  if (buffer == NULL) {
    throw strawException("Out of memory (malloc returned NULL)");
  }
  struct udcFile *udc = udcFileMayOpen(url, NULL);
  if (udc == NULL)
    {
    throw strawException("Error: unable to open udcFile for resource " + string(url));
    }
  total_bytes = udcFileSize(url);
  udcSeek(udc, position);
  udcRead(udc, (void*)buffer, chunksize);
  udcFileClose(&udc);
  return buffer;
}


// returns whether or not this is valid HiC file
bool readMagicString(istream& fin) {
  string str;
  getline(fin, str, '\0' );
  return str[0]=='H' && str[1]=='I' && str[2]=='C';
}

// reads the header, storing the positions of the normalization vectors and returning the master pointer
long readHeader(istream& fin, string chr1, string chr2, int &c1pos1, int &c1pos2, int &c2pos1, int &c2pos2, int &chr1ind, int &chr2ind) {
  if (!readMagicString(fin)) {
    throw strawException("Hi-C magic string is missing, does not appear to be a hic file");
  }

  fin.read((char*)&version, sizeof(int));
  if (version < 6) {
    throw strawException("Version " + to_string(version) + " no longer supported");
  }
  long master;
  fin.read((char*)&master, sizeof(long));
  string genome;
  getline(fin, genome, '\0' );
  int nattributes;
  fin.read((char*)&nattributes, sizeof(int));

  // reading and ignoring attribute-value dictionary
  for (int i=0; i<nattributes; i++) {
    string key, value;
    getline(fin, key, '\0');
    getline(fin, value, '\0');
  }
  int nChrs;
  fin.read((char*)&nChrs, sizeof(int));
  // chromosome map for finding matrix
  bool found1 = false;
  bool found2 = false;
  for (int i=0; i<nChrs; i++) {
    string name;
    int length;
    getline(fin, name, '\0');
    fin.read((char*)&length, sizeof(int));
    if (name==chr1) {
      found1=true;
      chr1ind = i;
      if (c1pos1 == -100) {
	c1pos1 = 0;
	c1pos2 = length;
      }
    }
    if (name==chr2) {
      found2=true;
      chr2ind = i;
      if (c2pos1 == -100) {
	c2pos1 = 0;
	c2pos2 = length;
      }
    }
  }
  if (!found1 || !found2) {
    if (chr1==chr2)
        throw strawException("Chromosome " + chr1 + " wasn't found in the file. Check that the chromosome name matches the genome.");
    else
        throw strawException("Chromosomes " + chr1 + " and/or " + chr2 + " weren't found in the file. Check that the chromosome names match the genome.");
  }
  return master;
}

// reads the footer from the master pointer location. takes in the chromosomes,
// norm, unit (BP or FRAG) and resolution or binsize, and sets the file 
// position of the matrix and the normalization vectors for those chromosomes 
// at the given normalization and resolution
void readFooter(istream& fin, long master, int c1, int c2, string norm, string unit, int resolution, long &myFilePos, indexEntry &c1NormEntry, indexEntry &c2NormEntry) {
  int nBytes;
  fin.read((char*)&nBytes, sizeof(int));
  stringstream ss;
  ss << c1 << "_" << c2;
  string key = ss.str();
  
  int nEntries;
  fin.read((char*)&nEntries, sizeof(int));
  bool found = false;
  for (int i=0; i<nEntries; i++) {
    string str;
    getline(fin, str, '\0');
    long fpos;
    fin.read((char*)&fpos, sizeof(long));
    int sizeinbytes;
    fin.read((char*)&sizeinbytes, sizeof(int));
    if (str == key) {
      myFilePos = fpos;
      found=true;
    }
  }
  if (!found) {
    throw strawException("File doesn't have the given chr_chr map");
  }

  if (norm=="NONE") return; // no need to read norm vector index
 
  // read in and ignore expected value maps; don't store; reading these to 
  // get to norm vector index
  int nExpectedValues;
  fin.read((char*)&nExpectedValues, sizeof(int));
  for (int i=0; i<nExpectedValues; i++) {
    string str;
    getline(fin, str, '\0'); //unit
    int binSize;
    fin.read((char*)&binSize, sizeof(int));

    int nValues;
    fin.read((char*)&nValues, sizeof(int));
    for (int j=0; j<nValues; j++) {
      double v;
      fin.read((char*)&v, sizeof(double));
    }

    int nNormalizationFactors;
    fin.read((char*)&nNormalizationFactors, sizeof(int));
    for (int j=0; j<nNormalizationFactors; j++) {
      int chrIdx;
      fin.read((char*)&chrIdx, sizeof(int));
      double v;
      fin.read((char*)&v, sizeof(double));
    }
  }
  fin.read((char*)&nExpectedValues, sizeof(int));
  for (int i=0; i<nExpectedValues; i++) {
    string str;
    getline(fin, str, '\0'); //typeString
    getline(fin, str, '\0'); //unit
    int binSize;
    fin.read((char*)&binSize, sizeof(int));

    int nValues;
    fin.read((char*)&nValues, sizeof(int));
    for (int j=0; j<nValues; j++) {
      double v;
      fin.read((char*)&v, sizeof(double));
    }
    int nNormalizationFactors;
    fin.read((char*)&nNormalizationFactors, sizeof(int));
    for (int j=0; j<nNormalizationFactors; j++) {
      int chrIdx;
      fin.read((char*)&chrIdx, sizeof(int));
      double v;
      fin.read((char*)&v, sizeof(double));
    }
  }
  // Index of normalization vectors
  fin.read((char*)&nEntries, sizeof(int));
  bool found1 = false;
  bool found2 = false;
  for (int i = 0; i < nEntries; i++) {
    string normtype;
    getline(fin, normtype, '\0'); //normalization type
    int chrIdx;
    fin.read((char*)&chrIdx, sizeof(int));
    string unit1;
    getline(fin, unit1, '\0'); //unit
    int resolution1;
    fin.read((char*)&resolution1, sizeof(int));
    long filePosition;
    fin.read((char*)&filePosition, sizeof(long));
    int sizeInBytes;
    fin.read((char*)&sizeInBytes, sizeof(int));
    if (chrIdx == c1 && normtype == norm && unit1 == unit && resolution1 == resolution) {
      c1NormEntry.position=filePosition;
      c1NormEntry.size=sizeInBytes;
      found1 = true;
    }
    if (chrIdx == c2 && normtype == norm && unit1 == unit && resolution1 == resolution) {
      c2NormEntry.position=filePosition;
      c2NormEntry.size=sizeInBytes;
      found2 = true;
    }
  }
  if (!found1 || !found2) {
    throw strawException("File did not contain " + norm + " normalization vectors for one or both chromosomes at "
            + to_string(resolution) + " " + unit);
  }
}

// reads the raw binned contact matrix at specified resolution, setting the block bin count and block column count 
bool readMatrixZoomData(istream& fin, string myunit, int mybinsize, int &myBlockBinCount, int &myBlockColumnCount) {
  string unit;
  getline(fin, unit, '\0' ); // unit
  int tmp;
  fin.read((char*)&tmp, sizeof(int)); // Old "zoom" index -- not used
  float tmp2;
  fin.read((char*)&tmp2, sizeof(float)); // sumCounts
  fin.read((char*)&tmp2, sizeof(float)); // occupiedCellCount
  fin.read((char*)&tmp2, sizeof(float)); // stdDev
  fin.read((char*)&tmp2, sizeof(float)); // percent95
  int binSize;
  fin.read((char*)&binSize, sizeof(int));
  int blockBinCount;
  fin.read((char*)&blockBinCount, sizeof(int));
  int blockColumnCount;
  fin.read((char*)&blockColumnCount, sizeof(int));
  
  bool storeBlockData = false;
  if (myunit==unit && mybinsize==binSize) {
    myBlockBinCount = blockBinCount;
    myBlockColumnCount = blockColumnCount;
    storeBlockData = true;
  }
  
  int nBlocks;
  fin.read((char*)&nBlocks, sizeof(int));

  for (int b = 0; b < nBlocks; b++) {
    int blockNumber;
    fin.read((char*)&blockNumber, sizeof(int));
    long filePosition;
    fin.read((char*)&filePosition, sizeof(long));
    int blockSizeInBytes;
    fin.read((char*)&blockSizeInBytes, sizeof(int));
    indexEntry entry;
    entry.size = blockSizeInBytes;
    entry.position = filePosition;
    if (storeBlockData) blockMap[blockNumber] = entry;
  }
  return storeBlockData;
}

// reads the raw binned contact matrix at specified resolution, setting the block bin count and block column count 
bool readMatrixZoomDataHttp(char *url, long &myFilePosition, string myunit, int mybinsize, int &myBlockBinCount, int &myBlockColumnCount) {
  char* buffer;
  int header_size = 5*sizeof(int)+4*sizeof(float);
  char* first;
  first = getHttpData(url, myFilePosition, 1);
  if (first[0]=='B') {
    header_size+=3;
  }
  else if (first[0]=='F') {
    header_size+=5;
  }
  else {
    throw strawException("Unit not understood - must be one of <BP/FRAG>");
  }
  buffer = getHttpData(url, myFilePosition, header_size);
  membuf sbuf(buffer, buffer + header_size);
  istream fin(&sbuf);

  string unit;
  getline(fin, unit, '\0' ); // unit
  int tmp;
  fin.read((char*)&tmp, sizeof(int)); // Old "zoom" index -- not used
  float tmp2;
  fin.read((char*)&tmp2, sizeof(float)); // sumCounts
  fin.read((char*)&tmp2, sizeof(float)); // occupiedCellCount
  fin.read((char*)&tmp2, sizeof(float)); // stdDev
  fin.read((char*)&tmp2, sizeof(float)); // percent95
  int binSize;
  fin.read((char*)&binSize, sizeof(int));
  int blockBinCount;
  fin.read((char*)&blockBinCount, sizeof(int));
  int blockColumnCount;
  fin.read((char*)&blockColumnCount, sizeof(int));

  bool storeBlockData = false;
  if (myunit==unit && mybinsize==binSize) {
    myBlockBinCount = blockBinCount;
    myBlockColumnCount = blockColumnCount;
    storeBlockData = true;
  }
  
  int nBlocks;
  fin.read((char*)&nBlocks, sizeof(int));

  if (storeBlockData) {
    buffer = getHttpData(url, myFilePosition+header_size, nBlocks*(sizeof(int)+sizeof(long)+sizeof(int)));
    membuf sbuf2(buffer, buffer + nBlocks*(sizeof(int)+sizeof(long)+sizeof(int)));
    istream fin2(&sbuf2);
    for (int b = 0; b < nBlocks; b++) {
      int blockNumber;
      fin2.read((char*)&blockNumber, sizeof(int));
      long filePosition;
      fin2.read((char*)&filePosition, sizeof(long));
      int blockSizeInBytes;
      fin2.read((char*)&blockSizeInBytes, sizeof(int));
      indexEntry entry;
      entry.size = blockSizeInBytes;
      entry.position = filePosition;
      blockMap[blockNumber] = entry;
    }
  }
  else {
    myFilePosition = myFilePosition+header_size+(nBlocks*(sizeof(int)+sizeof(long)+sizeof(int)));
  }
  delete buffer;
  return storeBlockData;
}

// goes to the specified file pointer in http and finds the raw contact matrix at specified resolution, calling readMatrixZoomData.
// sets blockbincount and blockcolumncount
void readMatrixHttp(char *url, long myFilePosition, string unit, int resolution, int &myBlockBinCount, int &myBlockColumnCount) {
  char * buffer;
  int size = sizeof(int)*3;
  buffer = getHttpData(url, myFilePosition, size);
  membuf sbuf(buffer, buffer + size);
  istream bufin(&sbuf);

  int c1,c2;
  bufin.read((char*)&c1, sizeof(int)); //chr1
  bufin.read((char*)&c2, sizeof(int)); //chr2
  int nRes;
  bufin.read((char*)&nRes, sizeof(int));
  int i=0;
  bool found=false;
  myFilePosition=myFilePosition+size;
  delete buffer;

  while (i<nRes && !found) {
    // myFilePosition gets updated within call
    found = readMatrixZoomDataHttp(url, myFilePosition, unit, resolution, myBlockBinCount, myBlockColumnCount);
    i++;
  }
  if (!found) {
    throw strawException("Error finding block data");
  }
}

// goes to the specified file pointer and finds the raw contact matrix at specified resolution, calling readMatrixZoomData.
// sets blockbincount and blockcolumncount
void readMatrix(istream& fin, long myFilePosition, string unit, int resolution, int &myBlockBinCount, int &myBlockColumnCount) {
  fin.seekg(myFilePosition, ios::beg);
  int c1,c2;
  fin.read((char*)&c1, sizeof(int)); //chr1
  fin.read((char*)&c2, sizeof(int)); //chr2
  int nRes;
  fin.read((char*)&nRes, sizeof(int));
  int i=0;
  bool found=false;
  while (i<nRes && !found) {
    found = readMatrixZoomData(fin, unit, resolution, myBlockBinCount, myBlockColumnCount);
    i++;
  }
  if (!found) {
    throw strawException("Error finding block data\nWas looking for " + unit + " at resolution " + to_string(resolution));
  }
}
// gets the blocks that need to be read for this slice of the data.  needs blockbincount, blockcolumncount, and whether
// or not this is intrachromosomal.
set<int> getBlockNumbersForRegionFromBinPosition(int* regionIndices, int blockBinCount, int blockColumnCount, bool intra) {
   int col1 = regionIndices[0] / blockBinCount;
   int col2 = (regionIndices[1] + 1) / blockBinCount;
   int row1 = regionIndices[2] / blockBinCount;
   int row2 = (regionIndices[3] + 1) / blockBinCount;
   
   set<int> blocksSet;
   // first check the upper triangular matrix
   for (int r = row1; r <= row2; r++) {
     for (int c = col1; c <= col2; c++) {
       int blockNumber = r * blockColumnCount + c;
       blocksSet.insert(blockNumber);
     }
   }
   // check region part that overlaps with lower left triangle
   // but only if intrachromosomal
   if (intra) {
     for (int r = col1; r <= col2; r++) {
       for (int c = row1; c <= row2; c++) {
	 int blockNumber = r * blockColumnCount + c;
	 blocksSet.insert(blockNumber);
       }
     }
   }

   return blocksSet;
}

// this is the meat of reading the data.  takes in the block number and returns the set of contact records corresponding to
// that block.  the block data is compressed and must be decompressed using the zlib library functions
vector<contactRecord> readBlock(istream& fin, char *url, bool isHttp, int blockNumber) {
  indexEntry idx = blockMap[blockNumber];
  if (idx.size == 0) {
    vector<contactRecord> v;
    return v;
  }
  char* compressedBytes = new char[idx.size];
  char* uncompressedBytes = new char[idx.size*10]; //biggest seen so far is 3

  if (isHttp) {
    compressedBytes = getHttpData(url, idx.position, idx.size);    
  }
  else {
    fin.seekg(idx.position, ios::beg);
    fin.read(compressedBytes, idx.size);
  }
  // Decompress the block
  // zlib struct
  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  infstream.avail_in = (uInt)(idx.size); // size of input
  infstream.next_in = (Bytef *)compressedBytes; // input char array
  infstream.avail_out = (uInt)idx.size*10; // size of output
  infstream.next_out = (Bytef *)uncompressedBytes; // output char array
  // the actual decompression work.
  inflateInit(&infstream);
  inflate(&infstream, Z_NO_FLUSH);
  inflateEnd(&infstream);
  int uncompressedSize=infstream.total_out;

  // create stream from buffer for ease of use
  membuf sbuf(uncompressedBytes, uncompressedBytes + uncompressedSize);
  istream bufferin(&sbuf);
  int nRecords;
  bufferin.read((char*)&nRecords, sizeof(int));
  vector<contactRecord> v(nRecords);
  // different versions have different specific formats
  if (version < 7) {
    for (int i = 0; i < nRecords; i++) {
      int binX, binY;
      bufferin.read((char*)&binX, sizeof(int));
      bufferin.read((char*)&binY, sizeof(int));
      float counts;
      bufferin.read((char*)&counts, sizeof(float));
      contactRecord record;
      record.binX = binX;
      record.binY = binY;
      record.counts = counts;
      v[i] = record;
    }
  } 
  else {
    int binXOffset, binYOffset;
    bufferin.read((char*)&binXOffset, sizeof(int));
    bufferin.read((char*)&binYOffset, sizeof(int));
    char useShort;
    bufferin.read((char*)&useShort, sizeof(char));
    char type;
    bufferin.read((char*)&type, sizeof(char));
    int index=0;
    if (type == 1) {
      // List-of-rows representation
      short rowCount;
      bufferin.read((char*)&rowCount, sizeof(short));
      for (int i = 0; i < rowCount; i++) {
	short y;
	bufferin.read((char*)&y, sizeof(short));
	int binY = y + binYOffset;
	short colCount;
	bufferin.read((char*)&colCount, sizeof(short));
	for (int j = 0; j < colCount; j++) {
	  short x;
	  bufferin.read((char*)&x, sizeof(short));
	  int binX = binXOffset + x;
	  float counts;
	  if (useShort == 0) { // yes this is opposite of usual
	    short c;
	    bufferin.read((char*)&c, sizeof(short));
	    counts = c;
	  } 
	  else {
	    bufferin.read((char*)&counts, sizeof(float));
	  }
	  contactRecord record;
	  record.binX = binX;
	  record.binY = binY;
	  record.counts = counts;
	  v[index]=record;
	  index++;
	}
      }
    }
    else if (type == 2) { // have yet to find test file where this is true, possibly entirely deprecated
      int nPts;
      bufferin.read((char*)&nPts, sizeof(int));
      short w;
      bufferin.read((char*)&w, sizeof(short));

      for (int i = 0; i < nPts; i++) {
	//int idx = (p.y - binOffset2) * w + (p.x - binOffset1);
	int row = i / w;
	int col = i - row * w;
	int bin1 = binXOffset + col;
	int bin2 = binYOffset + row;

	float counts;
	if (useShort == 0) { // yes this is opposite of the usual
	  short c;
	  bufferin.read((char*)&c, sizeof(short));
	  if (c != -32768) {
	    contactRecord record;
	    record.binX = bin1;
	    record.binY = bin2;
	    record.counts = c;
	    v[index]=record;
	    index++;
	  }
	} 
	else {
	  bufferin.read((char*)&counts, sizeof(float));
	  if (counts != 0x7fc00000) { // not sure this works
	    //	  if (!Float.isNaN(counts)) {
	    contactRecord record;
	    record.binX = bin1;
	    record.binY = bin2;
	    record.counts = counts;
	    v[index]=record;
	    index++;
	  }
	}
      }
    }
  }
  delete[] compressedBytes;
  delete[] uncompressedBytes; // don't forget to delete your heap arrays in C++!
  return v;
}

// reads the normalization vector from the file at the specified location
vector<double> readNormalizationVector(istream& bufferin) {
  int nValues;
  bufferin.read((char*)&nValues, sizeof(int));
  vector<double> values(nValues);
  //  bool allNaN = true;

  for (int i = 0; i < nValues; i++) {
    double d;
    bufferin.read((char*)&d, sizeof(double));
    values[i] = d;
    /* if (!Double.isNaN(values[i])) {
      allNaN = false;
      }*/
  }
  //  if (allNaN) return null;
  return values;
}

void straw(string norm, string fname, int binsize, string chr1loc, string chr2loc, string unit, vector<int> &xActual, vector<int> &yActual, vector<float> &counts)
{
  int earlyexit=1;
  if (!(norm=="NONE"||norm=="VC"||norm=="VC_SQRT"||norm=="KR")) {
    throw strawException("Invalid Norm value - must be one of <NONE/VC/VC_SQRT/KR>");
  }
  if (!(unit=="BP"||unit=="FRAG")) {
    throw strawException("Invalid Unit value - must be one of <BP/FRAG>");
  }

  // parse chromosome positions
  stringstream ss(chr1loc);
  string chr1, chr2, x, y;
  int c1pos1=-100, c1pos2=-100, c2pos1=-100, c2pos2=-100;
  getline(ss, chr1, ':');
  if (getline(ss, x, ':') && getline(ss, y, ':')) {
    c1pos1 = stoi(x);
    c1pos2 = stoi(y);
  }
  stringstream ss1(chr2loc);
  getline(ss1, chr2, ':');
  if (getline(ss1, x, ':') && getline(ss1, y, ':')) {
    c2pos1 = stoi(x);
    c2pos2 = stoi(y);
  }  
  int chr1ind, chr2ind;

  // HTTP code
  string prefix="http";
  bool isHttp = false;
  ifstream fin;

  // read header into buffer; 100K should be sufficient
  char *url = NULL;

  long master;
  if (std::strncmp(fname.c_str(), prefix.c_str(), prefix.size()) == 0) {
    isHttp = true;
    char * buffer;
    url = (char*) fname.c_str();
    buffer = getHttpData(url, 0, 100000);    
    membuf sbuf(buffer, buffer + 100000); 
    istream bufin(&sbuf);  
    master = readHeader(bufin, chr1, chr2, c1pos1, c1pos2, c2pos1, c2pos2, chr1ind, chr2ind);
    delete buffer;
  }
  else {
    fin.open(fname, fstream::in);
    if (!fin) {
      cerr << "File " << fname << " cannot be opened for reading" << endl;
      return;
    }
    master = readHeader(fin, chr1, chr2, c1pos1, c1pos2, c2pos1, c2pos2, chr1ind, chr2ind);
  }

  // from header have size of chromosomes, set region to read
  int c1=min(chr1ind,chr2ind);
  int c2=max(chr1ind,chr2ind);
  int origRegionIndices[4]; // as given by user
  int regionIndices[4]; // used to find the blocks we need to access
  // reverse order if necessary
  if (chr1ind > chr2ind) {
    origRegionIndices[0] = c2pos1;
    origRegionIndices[1] = c2pos2;
    origRegionIndices[2] = c1pos1;
    origRegionIndices[3] = c1pos2;
    regionIndices[0] = c2pos1 / binsize;
    regionIndices[1] = c2pos2 / binsize;
    regionIndices[2] = c1pos1 / binsize;
    regionIndices[3] = c1pos2 / binsize;
  }
  else {
    origRegionIndices[0] = c1pos1;
    origRegionIndices[1] = c1pos2;
    origRegionIndices[2] = c2pos1;
    origRegionIndices[3] = c2pos2;
    regionIndices[0] = c1pos1 / binsize;
    regionIndices[1] = c1pos2 / binsize;
    regionIndices[2] = c2pos1 / binsize;
    regionIndices[3] = c2pos2 / binsize;
  }

  indexEntry c1NormEntry, c2NormEntry;
  long myFilePos;

  unsigned long bytes_to_read = total_bytes - master;

  if (isHttp) {
    char* buffer2;
    buffer2 = getHttpData(url, master, bytes_to_read);    
    membuf sbuf2(buffer2, buffer2 + bytes_to_read);
    istream bufin2(&sbuf2);
    readFooter(bufin2, master, c1, c2, norm, unit, binsize, myFilePos, c1NormEntry, c2NormEntry); 
    delete buffer2;
  }
  else { 
    fin.seekg(master, ios::beg);
    readFooter(fin, master, c1, c2, norm, unit, binsize, myFilePos, c1NormEntry, c2NormEntry); 
  }
  // readFooter will assign the above variables


  vector<double> c1Norm;
  vector<double> c2Norm;

  if (norm != "NONE") {
    char* buffer3;
    if (isHttp) {
      buffer3 = getHttpData(url, c1NormEntry.position, c1NormEntry.size);
    }
    else {
      buffer3 = new char[c1NormEntry.size];
      fin.seekg(c1NormEntry.position, ios::beg);
      fin.read(buffer3, c1NormEntry.size);
    }
    membuf sbuf3(buffer3, buffer3 + c1NormEntry.size);
    istream bufferin(&sbuf3);
    c1Norm = readNormalizationVector(bufferin);

    char* buffer4;
    if (isHttp) {
      buffer4 = getHttpData(url, c2NormEntry.position, c2NormEntry.size);
    }
    else {
      buffer4 = new char[c2NormEntry.size];
      fin.seekg(c2NormEntry.position, ios::beg);
      fin.read(buffer4, c2NormEntry.size);
    }
    membuf sbuf4(buffer4, buffer4 + c2NormEntry.size);
    istream bufferin2(&sbuf4);
    c2Norm = readNormalizationVector(bufferin2);
    delete buffer3;
    delete buffer4;
  }

  int blockBinCount, blockColumnCount;
  if (isHttp) {
    // readMatrix will assign blockBinCount and blockColumnCount
    readMatrixHttp(url, myFilePos, unit, binsize, blockBinCount, blockColumnCount); 
  }
  else {
    // readMatrix will assign blockBinCount and blockColumnCount
    readMatrix(fin, myFilePos, unit, binsize, blockBinCount, blockColumnCount); 
  }
  set<int> blockNumbers = getBlockNumbersForRegionFromBinPosition(regionIndices, blockBinCount, blockColumnCount, c1==c2); 

  // getBlockIndices
  vector<contactRecord> records;
  for (set<int>::iterator it=blockNumbers.begin(); it!=blockNumbers.end(); ++it) {
    // get contacts in this block
    records = readBlock(fin, url, isHttp, *it);
    for (vector<contactRecord>::iterator it2=records.begin(); it2!=records.end(); ++it2) {
      contactRecord rec = *it2;
      
      int x = rec.binX * binsize;
      int y = rec.binY * binsize;
      float c = rec.counts;
      if (norm != "NONE") {
	c = c / (c1Norm[rec.binX] * c2Norm[rec.binY]);
      }

      if ((x >= origRegionIndices[0] && x <= origRegionIndices[1] &&
	   y >= origRegionIndices[2] && y <= origRegionIndices[3]) ||
	  // or check regions that overlap with lower left
	  ((c1==c2) && y >= origRegionIndices[0] && y <= origRegionIndices[1] && x >= origRegionIndices[2] && x <= origRegionIndices[3])) {
	xActual.push_back(x);
	yActual.push_back(y);
	counts.push_back(c);
	//printf("%d\t%d\t%.14g\n", x, y, c);
      }
    }
  }
}


void parseHeaderFields(istream& fin, string &genome, vector<string> &chromNames, vector<int> &bpResolutions, vector<int> &fragResolutions)
/* Parse out header fields from the supplied data buffer, assumed to come from the beginning of
 * a .hic file. */
{
  if (!readMagicString(fin)) {
    throw strawException("Hi-C magic string is missing, does not appear to be a hic file");
  }

  fin.read((char*)&version, sizeof(int));
  if (version < 6) {
    throw strawException("Version " + to_string(version) + " no longer supported");
  }
  long master;
  fin.read((char*)&master, sizeof(long));
  getline(fin, genome, '\0' );
  int nattributes;
  fin.read((char*)&nattributes, sizeof(int));

  // reading and ignoring attribute-value dictionary
  // Should expand this to save these to another structure
  for (int i=0; i<nattributes; i++) {
    string key, value;
    getline(fin, key, '\0');
    getline(fin, value, '\0');
  }
  int nChrs;
  fin.read((char*)&nChrs, sizeof(int));
  // chromosome map for finding matrix
  for (int i=0; i<nChrs; i++) {
    string name;
    getline(fin, name, '\0');
    chromNames.insert(chromNames.end(), name);
    // ignoring chromosome sizes
    int length;
    fin.read((char*)&length, sizeof(int));
  }

  int nBpResolutions;
  fin.read((char*)&nBpResolutions, sizeof(int));
  for (int i=0; i<nBpResolutions; i++) {
    int res;
    fin.read((char*)&res, sizeof(int));
    bpResolutions.insert(bpResolutions.end(), res);
  }

  int nFragResolutions;
  fin.read((char*)&nFragResolutions, sizeof(int));
  for (int i=0; i<nFragResolutions; i++) {
    int res;
    fin.read((char*)&res, sizeof(int));
    fragResolutions.insert(fragResolutions.end(), res);
  }
}


void getHeaderFields(string fname, string &genome, vector<string> &chromNames, vector<int> &bpResolutions, vector<int> &fragResolutions)
/* Retrieve .hic header fields from the supplied filename and return them in the supplied variables. */
{
  // HTTP code
  string prefix="http";
  bool isHttp = false;

  // read header into buffer; 100K should be sufficient
  char * buffer = NULL;
  char *url = NULL;

  if (std::strncmp(fname.c_str(), prefix.c_str(), prefix.size()) == 0) {
    isHttp = true;
    url = (char*) fname.c_str();
    buffer = getHttpData(url, 0, 100000);
    membuf sbuf(buffer, buffer + 100000);
    istream bufin(&sbuf);  
    parseHeaderFields(bufin, genome, chromNames, bpResolutions, fragResolutions);
    delete buffer;
  }
  else {
    fstream fin;
    fin.open(fname, fstream::in);
    if (!fin) {
      throw strawException("File " + fname + " cannot be opened for reading");
    }
    parseHeaderFields(fin, genome, chromNames, bpResolutions, fragResolutions);
    fin.close();
  }
}



extern "C" char *Cstraw (char *norm, char *fname, int binsize, char *chr1loc, char *chr2loc, char *unit, int **xActual, int **yActual, double **counts, int *numRecords)
/* Wrapper function to retrieve a data chunk from a .hic file, for use by C libraries.
 * norm is one of NONE/VC/VC_SQRT/KR.
 * binsize is one of the supported bin sizes as determined by CstrawHeader.
 * chr1loc and chr2loc are the two positions to retrieve interaction data for, specified as chr:start-end.
 * unit is one of BP/FRAG.
 * Values are returned in newly allocated arrays in xActual, yActual, and counts, with the number of
 * returned records in numRecords.
 * The function returns NULL unless an error was encountered, in which case the return value points
 * to a character string explaining the error. */
{
    string thisnorm(norm);
    string thisfname(fname);
    string thischr1loc(chr1loc);
    string thischr2loc(chr2loc);
    string thisunit(unit);
    vector<int> thisx;
    vector<int> thisy;
    vector<float> thiscounts;
    try {
      straw(thisnorm, thisfname, binsize, thischr1loc, thischr2loc, unit, thisx, thisy, thiscounts);
    } catch (strawException& err) {
      char *errMsg = (char*) calloc((size_t) strlen(err.what())+1, sizeof(char));
      strcpy(errMsg, err.what());
      return errMsg;
    }
    *numRecords = thisx.size();
    *xActual = (int*) calloc((size_t) *numRecords, sizeof(int));
    *yActual = (int*) calloc((size_t) *numRecords, sizeof(int));
    *counts = (double*) calloc((size_t) *numRecords, sizeof(double));
    for (int i=0; i<*numRecords; i++)
        {
        (*xActual)[i] = thisx[i];
        (*yActual)[i] = thisy[i];
        (*counts)[i] = (double) thiscounts[i];
        }
    return NULL;
}


extern "C" char *CstrawHeader (char *filename, char **genome, char ***chromNames, int *nChroms, char ***bpResolutions, int *nBpRes, char ***fragResolutions, int *nFragRes)
/* Wrapper function to retrieve header fields from a .hic file, for use by C libraries.
 * This retrieves the assembly name, list of chromosome names, list of available binsize resolutions,
 * and list of available fragment resolutions in the specific .hic file.
 * The function returns NULL unless an error was encountered, in which case the return value points
 * to a character string explaining the error. */
{
  string filenameString(filename);
  string genomeString;
  vector<string> chromNameVector;
  vector<int> bpResolutionVector;
  vector<int> fragResolutionVector;
  try {
    getHeaderFields(filenameString, genomeString, chromNameVector, bpResolutionVector, fragResolutionVector);
  } catch (strawException& err) {
    char *errMsg = (char*) calloc((size_t) strlen(err.what())+1, sizeof(char));
    strcpy(errMsg, err.what());
    return errMsg;
  }
  if (genome != NULL)
  {
    *genome = (char*) malloc((genomeString.length()+1)*sizeof(char));
    strcpy(*genome, genomeString.c_str());
  }
  if (nChroms != NULL)
  {
    *nChroms = chromNameVector.size();
  }
  if (chromNames != NULL)
  {
    *chromNames = (char**) calloc((size_t) chromNameVector.size(), sizeof(char*));
    for (int i=0; i<chromNameVector.size(); i++)
    {
      (*chromNames)[i] = (char*) malloc((chromNameVector[i].length()+1)*sizeof(char));
      strcpy((*chromNames)[i], chromNameVector[i].c_str());
    }
  }
  if (nBpRes != NULL)
  {
    *nBpRes = bpResolutionVector.size();
  }
  if (bpResolutions != NULL)
  {
    *bpResolutions = (char**) calloc((size_t) bpResolutionVector.size(), sizeof(char*));
    for (int i=0; i<bpResolutionVector.size(); i++)
    {
      (*bpResolutions)[i] = (char*) malloc((to_string(bpResolutionVector[i]).length()+1)*sizeof(char));
      strcpy((*bpResolutions)[i], to_string(bpResolutionVector[i]).c_str());
    }
  }
  if (nFragRes != NULL)
  {
    *nFragRes = fragResolutionVector.size();
  }
  if (fragResolutions != NULL)
  {
    *fragResolutions = (char**) calloc((size_t) fragResolutionVector.size(), sizeof(char*));
    for (int i=0; i<fragResolutionVector.size(); i++)
    {
      (*fragResolutions)[i] = (char*) malloc((to_string(fragResolutionVector[i]).length()+1)*sizeof(char));
      strcpy((*fragResolutions)[i], to_string(fragResolutionVector[i]).c_str());
    }
  }
  return NULL;
}
