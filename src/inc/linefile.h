/* lineFile - stuff to rapidly read text files and parse them into
 * lines.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef LINEFILE_H
#define LINEFILE_H

#include "dystring.h"

#ifdef USE_TABIX
#include "tabix.h"
#endif

enum nlType {
 nlt_undet, /* undetermined */
 nlt_unix,  /* lf   */
 nlt_dos,   /* crlf */
 nlt_mac    /* cr   */
};

struct metaOutput
/* struct to store list of file handles to output meta data to
 * meta data is text after # */
    {
    struct metaOutput *next;    /* next file handle */
    FILE *metaFile;             /* file to write metadata to */
    };

struct lineFile
/* Structure to handle fast, line oriented
 * fileIo. */
    {
    struct lineFile *next;	/* Might need to be on a list. */
    char *fileName;		/* Name of file. */
    int fd;			/* File handle.  -1 for 'memory' files. */
    int bufSize;		/* Size of buffer. */
    off_t bufOffsetInFile;	/* Offset in file of first buffer byte. */
    int bytesInBuf;		/* Bytes read into buffer. */
    int reserved;		/* Reserved (zero for now). */
    int lineIx;			/* Current line. */
    int lineStart;		/* Offset of line in buffer. */
    int lineEnd;		/* End of line in buffer. */
    bool zTerm;			/* Replace '\n' with zero? */
    enum nlType nlType;         /* type of line endings: dos, unix, mac or undet */
    bool reuse;			/* Set if reusing input. */
    char *buf;			/* Buffer. */
    struct pipeline *pl;        /* pipeline if reading compressed */
    struct metaOutput *metaOutput;   /* list of FILE handles to write metaData to */
    bool isMetaUnique;          /* if set, do not repeat comments in output */
    struct hash *metaLines;     /* save lines to suppress repetition */
#ifdef USE_TABIX
    tabix_t *tabix;		/* A tabix-compressed file and its binary index file (.tbi) */
    ti_iter_t tabixIter;	/* An iterator to get decompressed indexed lines of text */
#endif
    struct dyString *fullLine;  // Filled with full line when a lineFileNextFull is called
    struct dyString *rawLines;  // Filled with raw lines used to create the full line
    boolean fullLineReuse;      // If TRUE, next call to lineFileNextFull will get already built fullLine

    void *dataForCallBack;                                 // ptr to data needed for callbacks
    void(*checkSupport)(struct lineFile *lf, char *where); // check if operation supported 
    boolean(*nextCallBack)(struct lineFile *lf, char **retStart, int *retSize); // next line callback
    void(*closeCallBack)(struct lineFile *lf);             // close callback
    };

char *getFileNameFromHdrSig(char *m);
/* Check if header has signature of supported compression stream,
   and return a phoney filename for it, or NULL if no sig found. */

struct lineFile *lineFileDecompressFd(char *name, bool zTerm, int fd);
/* open a linefile with decompression from a file or socket descriptor */

struct lineFile *lineFileDecompressMem(bool zTerm, char *mem, long size);
/* open a linefile with decompression from a memory stream */

struct lineFile *lineFileMayOpen(char *fileName, bool zTerm);
/* Try and open up a lineFile. If fileName ends in .gz, .Z, or .bz2,
 * it will be read from a decompress pipeline. */

struct lineFile *lineFileOpen(char *fileName, bool zTerm);
/* Open up a lineFile or die trying If fileName ends in .gz, .Z, or .bz2,
 * it will be read from a decompress pipeline.. */

struct lineFile *lineFileAttach(char *fileName, bool zTerm, int fd);
/* Wrap a line file around an open'd file. */

struct lineFile *lineFileStdin(bool zTerm);
/* Wrap a line file around stdin. */

struct lineFile *lineFileOnString(char *name, bool zTerm, char *s);
/* Wrap a line file object around string in memory. This buffer
 * have zeroes written into it if zTerm is non-zero.  It will
 * be freed when the line file is closed. */

struct lineFile *lineFileOnBigBed(char *bigBedFileName);
/* Wrap a line file object around a BigBed. */

void lineFileClose(struct lineFile **pLf);
/* Close up a line file. */

void lineFileCloseList(struct lineFile **pList);
/* Close up a list of line files. */

boolean lineFileNext(struct lineFile *lf, char **retStart, int *retSize);
/* Fetch next line from file. */

boolean lineFileNextFull(struct lineFile *lf, char **retFull, int *retFullSize,
                        char **retRaw, int *retRawSize);
// Fetch next line from file joining up any that are continued by ending '\'
// If requested, and was joined, the unjoined raw lines are also returned
// NOTE: comment lines can't be continued!  ("# comment \ \n more comment" is 2 lines.)

boolean lineFileNextReal(struct lineFile *lf, char **retStart);
/* Fetch next line from file that is not blank and
 * does not start with a '#'. */

boolean lineFileNextFullReal(struct lineFile *lf, char **retStart);
// Fetch next line from file that is not blank and does not start with a '#'.
// Continuation lines (ending in '\') are joined into a single line.

void lineFileNeedNext(struct lineFile *lf, char **retStart, int *retSize);
/* Fetch next line from file.  Squawk and die if it's not there. */

void lineFileReuse(struct lineFile *lf);
/* Reuse current line. */

void lineFileReuseFull(struct lineFile *lf);
// Reuse last full line read.  Unlike lineFileReuse,
// lineFileReuseFull only works with previous lineFileNextFull call

#define lineFileString(lf) ((lf)->buf + (lf)->lineStart)
/* Current string in line file. */

#define lineFileTell(lf) ((lf)->bufOffsetInFile + (lf)->lineStart)
/* Current offset (of string start) in file. */

void lineFileSeek(struct lineFile *lf, off_t offset, int whence);
/* Seek to read next line from given position. */

void lineFileRewind(struct lineFile *lf);
/* Return lineFile to start. */

void lineFileAbort(struct lineFile *lf, char *format, ...)
/* Print file name, line number, and error message, and abort. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

void lineFileVaAbort(struct lineFile *lf, char *format, va_list args);
/* Print file name, line number, and error message, and abort. */

void lineFileUnexpectedEnd(struct lineFile *lf);
/* Complain about unexpected end of file. */

void lineFileExpectWords(struct lineFile *lf, int expecting, int got);
/* Check line has right number of words. */

void lineFileExpectAtLeast(struct lineFile *lf, int expecting, int got);
/* Check line has right number of words. */

void lineFileShort(struct lineFile *lf);
/* Complain that line is too short. */

boolean lineFileNextRow(struct lineFile *lf, char *words[], int wordCount);
/* Return next non-blank line that doesn't start with '#' chopped into words.
 * Returns FALSE at EOF.  Aborts on error. */

#define lineFileRow(lf, words) lineFileNextRow(lf, words, ArraySize(words))
/* Read in line chopped into fixed size word array. */

boolean lineFileNextCharRow(struct lineFile *lf, char sep, char *words[], int wordCount);
/* Return next non-blank line that doesn't start with '#' chopped into words
 * delimited by sep. Returns FALSE at EOF.  Aborts on error. */

boolean lineFileNextRowTab(struct lineFile *lf, char *words[], int wordCount);
/* Return next non-blank line that doesn't start with '#' chopped into words
 * at tabs. Returns FALSE at EOF.  Aborts on error. */

#define lineFileRowTab(lf, words) \
	lineFileNextRowTab(lf, words, ArraySize(words))
/* Read in line chopped by tab into fixed size word array. */

int lineFileChopNext(struct lineFile *lf, char *words[], int maxWords);
/* Return next non-blank line that doesn't start with '#' chopped into words. */

#define lineFileChop(lf, words) lineFileChopNext(lf, words, ArraySize(words))
/* Ease-of-usef macro for lineFileChopNext above. */

int lineFileChopCharNext(struct lineFile *lf, char sep, char *words[], int maxWords);
/* Return next non-blank line that doesn't start with '#' chopped into
   words delimited by sep. */

int lineFileChopNextTab(struct lineFile *lf, char *words[], int maxWords);
/* Return next non-blank line that doesn't start with '#' chopped into words
 * on tabs */

#define lineFileChopTab(lf, words) lineFileChopNextTab(lf, words, ArraySize(words))
/* Ease-of-usef macro for lineFileChopNext above. */

int lineFileCheckAllIntsNoAbort(char *s, void *val, 
    boolean isSigned, int byteCount, char *typeString, boolean noNeg, 
    char *errMsg, int errMsgSize);
/* Convert string to (signed) integer of the size specified.  
 * Unlike atol assumes all of string is number, no trailing trash allowed.
 * Returns 0 if conversion possible, and value is returned in 'val'
 * Otherwise 1 for empty string or trailing chars, and 2 for numeric overflow,
 * and 3 for (-) sign in unsigned number.
 * Error messages if any are written into the provided buffer.
 * Pass NULL val if you only want validation.
 * Use noNeg if negative values are not allowed despite the type being signed,
 * returns 4. */

void lineFileAllInts(struct lineFile *lf, char *words[], int wordIx, void *val,
  boolean isSigned,  int byteCount, char *typeString, boolean noNeg);
/* Returns long long integer from converting the input string. Aborts on error. */

int lineFileAllIntsArray(struct lineFile *lf, char *words[], int wordIx, void *array, int arraySize,
  boolean isSigned,  int byteCount, char *typeString, boolean noNeg);
/* Convert comma separated list of numbers to an array.  Pass in
 * array and max size of array. Aborts on error. Returns number of elements in parsed array. */

int lineFileNeedNum(struct lineFile *lf, char *words[], int wordIx);
/* Make sure that words[wordIx] is an ascii integer, and return
 * binary representation of it. */

int lineFileNeedFullNum(struct lineFile *lf, char *words[], int wordIx);
/* Make sure that words[wordIx] is an ascii integer, and return
 * binary representation of it. Require all chars in word to be digits.*/

double lineFileNeedDouble(struct lineFile *lf, char *words[], int wordIx);
/* Make sure that words[wordIx] is an ascii double value, and return
 * binary representation of it. */

void lineFileSkip(struct lineFile *lf, int lineCount);
/* Skip a number of lines. */

char *lineFileSkipToLineStartingWith(struct lineFile *lf, char *start, int maxCount);
/* Skip to next line that starts with given string.  Return NULL
 * if no such line found, otherwise return the line. */

char *lineFileReadAll(struct lineFile *lf);
/* Read remainder of lineFile and return it as a string. */

boolean lineFileParseHttpHeader(struct lineFile *lf, char **hdr,
				boolean *chunked, int *contentLength);
/* Extract HTTP response header from lf into hdr, tell if it's
 * "Transfer-Encoding: chunked" or if it has a contentLength. */

struct dyString *lineFileSlurpHttpBody(struct lineFile *lf,
				       boolean chunked, int contentLength);
/* Return a dyString that contains the http response body in lf.  Handle
 * chunk-encoding and content-length. */

void lineFileSetMetaDataOutput(struct lineFile *lf, FILE *f);
/* set file to write meta data to,
 * should be called before reading from input file */

void lineFileSetUniqueMetaData(struct lineFile *lf);
/* suppress duplicate lines in metadata */

void lineFileExpandBuf(struct lineFile *lf, int newSize);
/* Expand line file buffer. */

void lineFileRemoveInitialCustomTrackLines(struct lineFile *lf);
/* remove initial browser and track lines */

/*----- Optionally-compiled wrapper on tabix (compression + indexing): -----*/

#define COMPILE_WITH_TABIX "%s: Sorry, this functionality is available only when\n" \
    "you have installed the tabix library from\n" \
     "http://samtools.sourceforge.net/ and rebuilt kent/src with USE_TABIX=1\n" \
     "(see http://genomewiki.ucsc.edu/index.php/Build_Environment_Variables)."

struct lineFile *lineFileTabixMayOpen(char *fileOrUrl, bool zTerm);
/* Wrap a line file around a data file that has been compressed and indexed
 * by the tabix command line program.  The index file <fileName>.tbi must be
 * readable in addition to fileName. If there's a problem, warn & return NULL.
 * This works only if kent/src has been compiled with USE_TABIX=1 and linked
 * with the tabix C library. */

boolean lineFileSetTabixRegion(struct lineFile *lf, char *seqName, int start, int end);
/* Assuming lf was created by lineFileTabixMayOpen, tell tabix to seek to the specified region
 * and return TRUE (or if there are no items in region, return FALSE). */

#endif /* LINEFILE_H */


