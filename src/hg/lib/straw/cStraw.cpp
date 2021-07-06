
#include <cstring>
#include <iostream>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <streambuf>
#include "zlib.h"
#include "straw.h"
using namespace std;

// Supplementary functions for invoking Straw in C


void getHeaderFields(Straw *hicFile, string &genome, vector<string> &chromNames, vector<int> &chromSizes, vector<int> &bpResolutions, vector<int> &fragResolutions, vector<string> &attributes)
/* Retrieve .hic header fields from the supplied filename and return them in the supplied variables. */
{
  hicFile->loadHeader();
  genome = hicFile->genome;
  chromNames = hicFile->chrNames;
  chromSizes = hicFile->chrSizes;
  bpResolutions = hicFile->bpResolutions;
  fragResolutions = hicFile->fragResolutions;
  map<std::string,std::string>::iterator loop;
  for (loop=hicFile->attributes.begin(); loop!=hicFile->attributes.end(); loop++) {
    attributes.insert(attributes.end(), loop->first);
    attributes.insert(attributes.end(), loop->second);
  }
}


extern "C" Straw *cStrawOpen(char *fname) {
    return new Straw(fname);
}

extern "C" void cStrawClose(Straw **hicFile) {
    delete *hicFile;
    *hicFile = NULL;
}

extern "C" char *cStraw (Straw *hicFile, char *norm, int binsize, char *chr1loc, char *chr2loc, char *unit, int **xActual, int **yActual, double **counts, int *numRecords)
/* Wrapper function to retrieve a data chunk from a .hic file, for use by C libraries.
 * norm is one of NONE/VC/VC_SQRT/KR.
 * binsize is one of the supported bin sizes as determined by cStrawHeader.
 * chr1loc and chr2loc are the two positions to retrieve interaction data for, specified as chr:start-end.
 * unit is one of BP/FRAG.
 * Values are returned in newly allocated arrays in xActual, yActual, and counts, with the number of
 * returned records in numRecords.
 * The function returns NULL unless an error was encountered, in which case the return value points
 * to a character string explaining the error. */
{
    string thisnorm(norm);
    string thischr1loc(chr1loc);
    string thischr2loc(chr2loc);
    string thisunit(unit);
    vector<int> thisx;
    vector<int> thisy;
    vector<float> thiscounts;
    try {
      hicFile->straw(thisnorm, binsize, thischr1loc, thischr2loc, unit, thisx, thisy, thiscounts);
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


extern "C" char *cStrawHeader (Straw *hicFile, char **genome, char ***chromNames, int **chromSizes, int *nChroms, char ***bpResolutions, int *nBpRes, char ***fragResolutions, int *nFragRes, char ***attributes, int *nAttributes)
/* Wrapper function to retrieve header fields from a .hic file, for use by C libraries.
 * This retrieves the assembly name, list of chromosome names, list of available binsize resolutions,
 * and list of available fragment resolutions in the specific .hic file.
 * The function returns NULL unless an error was encountered, in which case the return value points
 * to a character string explaining the error. */
{
  string genomeString;
  vector<string> chromNameVector;
  vector<int> chromSizeVector;
  vector<int> bpResolutionVector;
  vector<int> fragResolutionVector;
  vector<string> attributeVector;
  try {
    getHeaderFields(hicFile, genomeString, chromNameVector, chromSizeVector, bpResolutionVector, fragResolutionVector, attributeVector);
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
  if (chromSizes != NULL)
  {
    *chromSizes = (int*) calloc((size_t) chromSizeVector.size(), sizeof(int));
    for (int i=0; i<chromSizeVector.size(); i++)
    {
      (*chromSizes)[i] = chromSizeVector[i];
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
  if (nAttributes != NULL)
  {
    *nAttributes = attributeVector.size();
  }
  if (attributes != NULL)
  {
    *attributes = (char**) calloc((size_t) attributeVector.size(), sizeof(char*));
    for (int i=0; i<attributeVector.size(); i++)
    {
      (*attributes)[i] = (char*) malloc((attributeVector[i].length()+1)*sizeof(char));
      strcpy((*attributes)[i], attributeVector[i].c_str());
    }
  }
  return NULL;
}
