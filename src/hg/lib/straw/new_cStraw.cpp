
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
#include "new_straw.h"
using namespace std;

// Supplementary functions for invoking Straw in C

struct Straw {
    string *fileName;
    string *genome;
    vector<string> chromNames;
    vector<int> chromSizes;
    int nChroms;
    vector<int> bpResolutions;
    int nBpRes;
    vector<int> fragResolutions;
    int nFragRes;
    vector<string> attributes;
    int nAttributes;
};


extern "C" char *cStrawOpen(char *fname, Straw **p)
/* Create a Straw object based on the hic file at the provided path and set *p to point to it.
 * On error, set *p = NULL and return a non-null string describing the error. */
{
    *p = (Straw*) calloc (1, sizeof(struct Straw));

    (*p)->fileName = new string(fname);
    (*p)->genome = new string();
    string genome;
    try {
        getHeaderFields(fname, genome, (*p)->chromNames, (*p)->chromSizes, (*p)->bpResolutions,
                (*p)->fragResolutions, (*p)->attributes);
    } catch (strawException& err) {
      char *errMsg = (char*) calloc((size_t) strlen(err.what())+1, sizeof(char));
      strcpy(errMsg, err.what());
      free(*p);
      *p = NULL;
      return errMsg;
    }
    (*p)->genome->assign(genome);
    (*p)->nChroms = (*p)->chromNames.size();
    (*p)->nBpRes = (*p)->bpResolutions.size();
    (*p)->nFragRes = (*p)->fragResolutions.size();
    (*p)->nAttributes = (*p)->attributes.size();
    if ((*p)->nBpRes > 0)
        return NULL;
    char *errString = (char*) malloc (strlen("Unable to retrieve header data from file") + 1);
    strcpy(errString, "Unable to retrieve header data from file");
    return errString;
}

extern "C" void cStrawClose(Straw **hicFile)
/* Free up a straw object created with cStrawOpen() */
{
    if (*hicFile != NULL)
    {
        delete (*hicFile)->genome;
        (*hicFile)->chromNames.clear();
        (*hicFile)->chromSizes.clear();
        (*hicFile)->bpResolutions.clear();
        (*hicFile)->fragResolutions.clear();
        (*hicFile)->attributes.clear();
    }
    free(*hicFile);
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
    vector<contactRecord> strawRecords;
    try {
    strawRecords = straw("observed",  thisnorm, *(hicFile->fileName),
            thischr1loc, thischr2loc, unit, binsize);
    } catch (strawException& err) {
      char *errMsg = (char*) calloc((size_t) strlen(err.what())+1, sizeof(char));
      strcpy(errMsg, err.what());
      return errMsg;
    }
    *numRecords = strawRecords.size();
    *xActual = (int*) calloc((size_t) *numRecords, sizeof(int));
    *yActual = (int*) calloc((size_t) *numRecords, sizeof(int));
    *counts = (double*) calloc((size_t) *numRecords, sizeof(double));
    for (int i=0; i<*numRecords; i++)
        {
        (*xActual)[i] = strawRecords[i].binX;
        (*yActual)[i] = strawRecords[i].binY;
        (*counts)[i] = (double) strawRecords[i].counts;
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
    if (genome != NULL)
    {
        *genome = (char*) malloc((hicFile->genome->length()+1)*sizeof(char));
        strcpy(*genome, hicFile->genome->c_str());
    }
    if (nChroms != NULL)
    {
        *nChroms = hicFile->nChroms;
    }
    if (chromNames != NULL)
    {
        *chromNames = (char**) calloc((size_t) hicFile->chromNames.size(), sizeof(char*));
        for (int i=0; i<hicFile->chromNames.size(); i++)
        {
            (*chromNames)[i] = (char*) malloc((hicFile->chromNames[i].length()+1)*sizeof(char));
            strcpy((*chromNames)[i], hicFile->chromNames[i].c_str());
        }
    }
    if (chromSizes != NULL)
    {
        *chromSizes = (int*) calloc((size_t) hicFile->chromSizes.size(), sizeof(int));
        for (int i=0; i<hicFile->chromSizes.size(); i++)
        {
            (*chromSizes)[i] = hicFile->chromSizes[i];
        }
    }
    if (nBpRes != NULL)
    {
        *nBpRes = hicFile->nBpRes;
    }
    if (bpResolutions != NULL)
    {
        *bpResolutions = (char**) calloc((size_t) hicFile->bpResolutions.size(), sizeof(char*));
        for (int i=0; i<hicFile->bpResolutions.size(); i++)
        {
            (*bpResolutions)[i] = (char*) malloc((to_string(hicFile->bpResolutions[i]).length()+1)*sizeof(char));
            strcpy((*bpResolutions)[i], to_string(hicFile->bpResolutions[i]).c_str());
        }
    }
    // skipping fragment stuff for now
    if (nFragRes != NULL && false)
    {
        *nFragRes = hicFile->nFragRes;
    }
    // skipping fragment stuff for now
    if (fragResolutions != NULL && false)
    {
        *fragResolutions = (char**) calloc((size_t) hicFile->fragResolutions.size(), sizeof(char*));
        for (int i=0; i<hicFile->fragResolutions.size(); i++)
        {
            (*fragResolutions)[i] = (char*) malloc((to_string(hicFile->fragResolutions[i]).length()+1)*sizeof(char));
            strcpy((*fragResolutions)[i], to_string(hicFile->fragResolutions[i]).c_str());
        }
    }
    if (nAttributes != NULL)
    {
        *nAttributes = hicFile->nAttributes;
    }
    if (attributes != NULL)
    {
        *attributes = (char**) calloc((size_t) hicFile->attributes.size(), sizeof(char*));
        for (int i=0; i<hicFile->attributes.size(); i++)
        {
            (*attributes)[i] = (char*) malloc((hicFile->attributes[i].length()+1)*sizeof(char));
            strcpy((*attributes)[i], hicFile->attributes[i].c_str());
        }
    }
    return NULL;
}
