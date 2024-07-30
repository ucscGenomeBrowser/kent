/* cStraw - a wrapper header file for making C++ functions associated with retrieving
 * data from .hic files available to C libraries.  The underlying functions come
 * from the Straw library by the Aiden Lab at the Broad Institute (see
 * kent/src/hg/lib/straw/).
*/
#ifndef CSTRAW_H
#define CSTRAW_H

typedef struct Straw Straw;

char *cStrawOpen (char *fname, Straw **p);
/* Create a Straw object based on the hic file at the provided path and set *p to point to it.
 * On error, set *p = NULL and return a non-null string describing the error. */

Straw *cStrawClose (Straw **hicFile);
/* Close out a Straw record, freeing the object */

char* cStraw (Straw *hicFile, char *norm, int binsize, char *chr1loc, char *chr2loc, char *unit, int **xActual, int **yActual, double **counts, int *numRecords);
/* Wrapper function to retrieve a data chunk from a .hic file, for use by C libraries.
 * norm is probably one of NONE/VC/VC_SQRT/KR/SCALE, but it depends on the file.
 * binsize is one of the supported bin sizes as determined by cStrawHeader.
 * chr1loc and chr2loc are the two positions to retrieve interaction data for, specified as chr:start-end.
 * unit is one of BP/FRAG.
 * Values are returned in newly allocated arrays in xActual, yActual, and counts, with the number of
 * returned records in numRecords.
 * The function returns NULL unless an error was encountered, in which case the return value points
 * to a character string explaining the error. */

char* cStrawHeader (Straw *hicFile, char **genome, char ***chromNames, int **chromSizes, int *nChroms,
        char ***bpResolutions, int *nBpRes, char ***fragResolutions, int *nFragRes,
        char ***attributes, int *nAttributes, char ***normOptions, int *nNormOptions);
/* Wrapper function to retrieve header fields from a .hic file, for use by C libraries.
 * This retrieves the assembly name, list of chromosome names, list of available binsize resolutions,
 * the list of available fragment resolutions in the specific .hic file, and a list of available
 * normalization options.  Technically the normalization options are supposed to be resolution-specific,
 * but we're not ready for a redesign in that direction just yet.
 * The function returns NULL unless an error was encountered, in which case the return value points
 * to a character string explaining the error. */
#endif
