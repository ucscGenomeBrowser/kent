
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
    set<string> normOptions;
};


extern "C" char *cStrawOpen(char *fname, Straw **p)
/* Create a Straw object based on the hic file at the provided path and set *p to point to it.
 * On error, set *p = NULL and return a non-null string describing the error. */
{
    *p = (Straw*) calloc (1, sizeof(struct Straw));

    (*p)->fileName = new string(fname);
    (*p)->genome = new string();
    try {
        getHeaderFields(fname, *((*p)->genome), (*p)->chromNames, (*p)->chromSizes, (*p)->bpResolutions,
                (*p)->fragResolutions, (*p)->attributes);
    } catch (strawException& err) {
      char *errMsg = (char*) calloc((size_t) strlen(err.what())+1, sizeof(char));
      strcpy(errMsg, err.what());
      free(*p);
      *p = NULL;
      return errMsg;
    }
    (*p)->nChroms = (*p)->chromNames.size();
    (*p)->nBpRes = (*p)->bpResolutions.size();
    (*p)->nFragRes = (*p)->fragResolutions.size();
    (*p)->nAttributes = (*p)->attributes.size();

    // I seem to recall situations where getHeaderFields doesn't throw an error, but the data are
    // bad regardless (e.g. when the structure of a header changed between .hic versions).  This
    // should help catch those.
    if ((*p)->nBpRes == 0)
    {
        char *errString = (char*) malloc (strlen("Unable to retrieve header data from file") + 1);
        strcpy(errString, "Unable to retrieve header data from file");
        return errString;
    }

    // Time to get the normalization options.  As a hack, we get these by making a dummy data request,
    // having that set up a global variable with the options seen, then retrieving that.  This will
    // miss situations where different normalization options are available at different resolutions.
    // That is a thing that can happen, but I haven't tested the performace hit of running through every
    // available resolution for the possible options just yet (let alone restructuring the browser side
    // of the code to support that kind of dependency).  In the interim, this method is still better
    // than the previous strategy of hard-coding in options that sometimes aren't available (and missing
    // ones that are).
 
    vector<contactRecord> strawRecords;
    try {
        // using chrom[1] because [0] is always "All", which doesn't seem to play well because it
        // may have a different set of resolution options
        string chrPos = (*p)->chromNames[1] + ":1:1";
        // Intentionally feed straw an empty normalization option.  This will cause an error (which we trap),
        // but it's the easiest way to make the library load and compare the available options (the library
        // short-circuits out early if "NONE" is provided).
        strawRecords = straw("observed", "", string(fname),
            chrPos, chrPos, "BP", (*p)->bpResolutions[0]);
    } catch (strawException& err) {
        // Do nothing - we're intentionally feeding it a bad norm option just so it'll go through the list
        // and realize it can't find it, populating a list along the way.
    }

    // Now the list that getNormOptions() depends on should be populated
    (*p)->normOptions = getNormOptions();

    return NULL;
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
        (*hicFile)->normOptions.clear();
    }
    free(*hicFile);
    *hicFile = NULL;
}

extern "C" char *cStraw (Straw *hicFile, char *norm, int binsize, char *chr1loc, char *chr2loc, char *unit, int **xActual, int **yActual, double **counts, int *numRecords)
/* Wrapper function to retrieve a data chunk from a .hic file, for use by C libraries.
 * norm is probably one of NONE/VC/VC_SQRT/KR/SCALE, but it depends on the file.
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


extern "C" char *cStrawHeader (Straw *hicFile, char **genome, char ***chromNames, int **chromSizes, int *nChroms,
        char ***bpResolutions, int *nBpRes, char ***fragResolutions, int *nFragRes, char ***attributes,
        int *nAttributes, char ***normOptions, int* nNormOptions)
/* Wrapper function to retrieve header fields from a .hic file, for use by C libraries.
 * This retrieves the assembly name, list of chromosome names, list of available binsize resolutions,
 * the list of available fragment resolutions in the specific .hic file, and a list of available
 * normalization options.  Technically the normalization options are supposed to be resolution-specific,
 * but we're not ready for a redesign in that direction just yet.
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
    // skipping fragment stuff for now; we don't use it
    if (nFragRes != NULL && false)
    {
        *nFragRes = hicFile->nFragRes;
    }
    // skipping fragment stuff for now; we don't use it
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
    if (nNormOptions != NULL)
    {
        // Include one extra for "NONE", which doesn't appear in normOptions
        *nNormOptions = hicFile->normOptions.size()+1;
    }
    if (normOptions != NULL)
    {
        *normOptions = (char**) calloc((size_t) hicFile->normOptions.size()+1, sizeof(char*));
        (*normOptions)[0] = (char*) "NONE";
        int i = 1;
        std::set<string>::iterator it = hicFile->normOptions.begin();
        for (; it != hicFile->normOptions.end(); it++, i++)
        {
            (*normOptions)[i] = (char*) malloc(((*it).length()+1)*sizeof(char));
            strcpy((*normOptions)[i], (*it).c_str());
        }
    }
    return NULL;
}
