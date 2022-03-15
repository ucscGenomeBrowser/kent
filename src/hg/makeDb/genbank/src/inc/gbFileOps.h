/*
 * Various file operations used by the genbank update code.  Also a bit of a
 * dumping ground for a few other routines.
 */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBFILEOPS_H
#define GBFILEOPS_H

#include "common.h"
#include "linefile.h"
#include <stdarg.h>
#include <stdio.h>

void allowedRNABasesInit();
/* initialize allowed RNA bases */

int numAllowedRNABases(char *rna);
/* count the number of invalid base characters in a sequence. */

boolean gbIsReadable(char* path);
/* Test if a file exists and is readable by the user. */

void gbMakeDirs(char* path);
/* make a directory, including parent directories */

void gbMakeFileDirs(char* path);
/* make all of the directories for a file */

char** gbGetCompressor(char *fileName, char *mode);
/* Get two element array with compression program based on the file name and
 * flags flags to pass to compressor.  Returns NULL if not compressed.
 */

void gbGetOutputTmp(char *path, char *tmpPath);
/* generate the tmp path name, moving the compress extension if needed */

FILE* gbMustOpenOutput(char* path);
/* Open an output file for atomic file creation.  Create the directory if it
 * doesn't exist.  If the path has one of the compress extensions (.gz, .Z,
 * .bz2) the output is compressed. File is opened under a tmp name until
 * installed. */

void gbOutputRename(char* path, FILE** fhPtr);
/* Install an output file create by gbOutputOpen.  If fh is not NULL, the file
 * is also closed.  If closed separately, must use gzClose. */

void gbOutputRemove(char* path, FILE** fhPtr);
/* Remove an output file instead of installing it. If fh is not NULL, the file
 * is also closed. */

unsigned gbParseUnsigned(struct lineFile* lf, char* numStr);
/* parse a unsigned that was take from a lineFile column  */

int gbParseInt(struct lineFile* lf, char* numStr);
/* parse an int that was take from a lineFile column  */

off_t gbParseFileOff(struct lineFile* lf, char* numStr);
/* parse a file offset that was take from a lineFile column  */

time_t gbParseChkDate(char* dateStr, boolean* isOk);
/* parse a date, in YYYY-MM-DD format, converting it to a unix time (as
 * returned by time()).  Sets isOk to false if an error, does not change
 * if ok */

time_t gbParseDate(struct lineFile* lf, char* dateStr);
/* parse a date, in YYYY-MM-DD format, converting it to a unix time
 * (as returned by time()).  If lf is not null, the file information will
 * be included in error message. */

char* gbFormatDate(time_t date);
/* format a unix date, in YYYY-MM-DD format.
 * Warning: return is a static buffer, however rotated between 4 buffers to
 * make it easy to use in print statements. */

time_t gbParseTimeStamp(char *col);
/* Parse a time stamp, in YYYYMMDDHHMMSS format, converting it to a unix time
 * (as returned by time()). */

struct lineFile* gzLineFileOpen(char *fileName);
/* Open a line file, checking the file name to determine if the file is
 * compressed.  If it ends in .gz, .Z, or .bz2 it will be piped through the
 * approriate decompression program. Must use gzLineFileClose() to close the
 * file. */

void gzLineFileClose(struct lineFile** lfPtr);
/* Close a lineFile and process opened by gzLineFileOpen().  Abort if
 * process exits non-zero */

FILE* gzMustOpen(char* fileName, char *mode);
/* Open a file for read or write access.  If it ends in .gz, .Z, or .bz2 it
 * will be piped through the approriate decompression program. Must use
 * gzClose() to close the file. */

void gzClose(FILE** fhPtr);
/* Close a file opened by gzMustOpen().  Abort if compress process exits
 * non-zero or there was an error writing the file. */

off_t gbTell(FILE* fh);
/* get offset in file, or abort on error */

/* macro to return the supplied value if not NULL or an empty string if
 * NULL */
#define gbValueOrEmpty(val) ((val == NULL) ? "" : val)

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
