#ifndef HG_REF_ALIGN_WEBB_H
#define HG_REF_ALIGN_WEBB_H

/*
 * Routines for reading Webb Miller format alignments.
 */

struct refAlign* parseWebbFile(char* fname);
/* Read and parse a file, return head of list */

struct refAlign* parseWebbFiles(int nfiles, char** fnames);
/* Read and parse multiple files, return head of list */
#endif
