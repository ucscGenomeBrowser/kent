/* Cstraw - a wrapper header file for making C++ functions associated with retrieving
 * data from .hic files available to C libraries.  The underlying functions come
 * from the Straw library by the Aiden Lab at the Broad Institute (see
 * kent/src/hg/lib/straw/).
*/
#ifndef CSTRAW_H
#define CSTRAW_H

char* Cstraw (char *norm, char *fname, int binsize, char *chr1loc, char *chr2loc, char *unit, int **xActual, int **yActual, double **counts, int *numRecords);
/* Wrapper function to retrieve a data chunk from a .hic file, for use by C libraries.
 * norm is one of NONE/VC/VC_SQRT/KR.
 * binsize is one of the supported bin sizes as determined by CstrawHeader.
 * chr1loc and chr2loc are the two positions to retrieve interaction data for, specified as chr:start-end.
 * unit is one of BP/FRAG.
 * Values are returned in newly allocated arrays in xActual, yActual, and counts, with the number of
 * returned records in numRecords.
 * The function returns NULL unless an error was encountered, in which case the return value points
 * to a character string explaining the error. */

char* CstrawHeader (char *filename, char **genome, char ***chromNames, int *nChroms, char ***bpResolutions, int *nBpRes, char ***fragResolutions, int *nFragRes);
/* Wrapper function to retrieve header fields from a .hic file, for use by C libraries.
 * This retrieves the assembly name, list of chromosome names, list of available binsize resolutions,
 * and list of available fragment resolutions in the specific .hic file.
 * The function returns NULL unless an error was encountered, in which case the return value points
 * to a character string explaining the error. */
#endif
