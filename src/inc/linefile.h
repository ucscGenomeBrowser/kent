/* lineFile - stuff to rapidly read text files and parse them into
 * lines. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef LINEFILE_H
#define LINEFILE_H

struct lineFile
/* Structure to handle fast, line oriented
 * fileIo. */
    {
    struct lineFile *next;	/* Might need to be on a list. */
    char *fileName;		/* Name of file. */
    int fd;			/* File handle. */
    int bufSize;		/* Size of buffer. */
    off_t bufOffsetInFile;	/* Offset in file of first buffer byte. */
    int bytesInBuf;		/* Bytes read into buffer. */
    int lastReadSize;		/* Size of last read. */
    int lineIx;			/* Current line. */
    int lineStart;		/* Offset of line in buffer. */
    int lineEnd;		/* End of line in buffer. */
    bool zTerm;			/* Replace '\n' with zero? */
    bool reuse;			/* Set if reusing input. */
    char *buf;			/* Buffer. */
    };

struct lineFile *lineFileMayOpen(char *fileName, bool zTerm);
/* Try and open up a lineFile. */

struct lineFile *lineFileOpen(char *fileName, bool zTerm);
/* Open up a lineFile or die trying. */

struct lineFile *lineFileAttatch(char *fileName, bool zTerm, int fd);
/* Wrap a line file around an open'd file. */
#define lineFileAttach lineFileAttatch  /* I'm a bad speller, oops. */

struct lineFile *lineFileStdin(bool zTerm);
/* Wrap a line file around stdin. */

void lineFileClose(struct lineFile **pLf);
/* Close up a line file. */

boolean lineFileNext(struct lineFile *lf, char **retStart, int *retSize);
/* Fetch next line from file. */

boolean lineFileNextReal(struct lineFile *lf, char **retStart);
/* Fetch next line from file that is not blank and 
 * does not start with a '#'. */

void lineFileNeedNext(struct lineFile *lf, char **retStart, int *retSize);
/* Fetch next line from file.  Squawk and die if it's not there. */

void lineFileReuse(struct lineFile *lf);
/* Reuse current line. */

#define lineFileString(lf) ((lf)->buf + (lf)->lineStart)
/* Current string in line file. */

#define lineFileTell(lf) ((lf)->bufOffsetInFile + (lf)->lineStart)
/* Current offset (of string start) in file. */

void lineFileSeek(struct lineFile *lf, off_t offset, int whence);
/* Seek to read next line from given position. */
 
void lineFileExpectWords(struct lineFile *lf, int expecting, int got);
/* Check line has right number of words. */

void lineFileExpectAtLeast(struct lineFile *lf, int expecting, int got);
/* Check line has right number of words. */

boolean lineFileNextRow(struct lineFile *lf, char *words[], int wordCount);
/* Return next non-blank line that doesn't start with '#' chopped into words.
 * Returns FALSE at EOF.  Aborts on error. */

#define lineFileRow(lf, words) lineFileNextRow(lf, words, ArraySize(words))
/* Read in line chopped into fixed size word array. */

boolean lineFileNextRowTab(struct lineFile *lf, char *words[], int wordCount);
/* Return next non-blank line that doesn't start with '#' chopped into words
 * at tabs. Returns FALSE at EOF.  Aborts on error. */

int lineFileChopNext(struct lineFile *lf, char *words[], int maxWords);
/* Return next non-blank line that doesn't start with '#' chopped into words. */

#define lineFileChop(lf, words) lineFileChopNext(lf, words, ArraySize(words))
/* Ease-of-usef macro for lineFileChopNext above. */

int lineFileChopNextTab(struct lineFile *lf, char *words[], int maxWords);
/* Return next non-blank line that doesn't start with '#' chopped into words
 * on tabs */

int lineFileNeedNum(struct lineFile *lf, char *words[], int wordIx);
/* Make sure that words[wordIx] is an ascii integer, and return
 * binary representation of it. */

void lineFileSkip(struct lineFile *lf, int lineCount);
/* Skip a number of lines. */

boolean lineFileParseHttpHeader(struct lineFile *lf, char **hdr,
				boolean *chunked, int *contentLength);
/* Extract HTTP response header from lf into hdr, tell if it's 
 * "Transfer-Encoding: chunked" or if it has a contentLength. */

struct dyString *lineFileSlurpHttpBody(struct lineFile *lf,
				       boolean chunked, int contentLength);
/* Return a dyString that contains the http response body in lf.  Handle 
 * chunk-encoding and content-length. */

#endif /* LINEFILE_H */


