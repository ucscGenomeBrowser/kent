/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* lineFile - stuff to rapidly read text files and parse them into
 * lines. */

#include "common.h"
#include <fcntl.h>
#include "linefile.h"

struct lineFile *lineFileAttatch(char *fileName, bool zTerm, int fd)
/* Wrap a line file around an open'd file. */
{
struct lineFile *lf;
AllocVar(lf);
lf->fileName = cloneString(fileName);
lf->fd = fd;
lf->bufSize = 256*1024;
lf->zTerm = zTerm;
lf->buf = needMem(lf->bufSize+1);
return lf;
}

void lineFileExpandBuf(struct lineFile *lf, int newSize)
/* Expand line file buffer. */
{
assert(newSize > lf->bufSize);
lf->buf = needMoreMem(lf->buf, lf->bytesInBuf, newSize);
lf->bufSize = newSize;
}


struct lineFile *lineFileStdin(bool zTerm)
/* Wrap a line file around stdin. */
{
return lineFileAttatch("stdin", zTerm, fileno(stdin));
}

struct lineFile *lineFileMayOpen(char *fileName, bool zTerm)
/* Try and open up a lineFile. */
{
int fd;

if (sameString(fileName, "stdin"))
    return lineFileStdin(zTerm);
fd = open(fileName, O_RDONLY);
if (fd == -1)
    return NULL;
return lineFileAttatch(fileName, zTerm, fd);
}

struct lineFile *lineFileOpen(char *fileName, bool zTerm)
/* Open up a lineFile or die trying. */
{
struct lineFile *lf = lineFileMayOpen(fileName, zTerm);
if (lf == NULL)
    errAbort("Couldn't open %s", fileName);
return lf;
}

void lineFileReuse(struct lineFile *lf)
/* Reuse current line. */
{
lf->reuse = TRUE;
}

int lineFileLongNetRead(int fd, char *buf, int size)
/* Keep reading until either get no new characters or
 * have read size */
{
int oneSize, totalRead = 0;

while (size > 0)
    {
    oneSize = read(fd, buf, size);
    if (oneSize <= 0)
        break;
    totalRead += oneSize;
    buf += oneSize;
    size -= oneSize;
    }
return totalRead;
}

boolean lineFileNext(struct lineFile *lf, char **retStart, int *retSize)
/* Fetch next line from file. */
{
char *buf = lf->buf;
int bytesInBuf = lf->bytesInBuf;
int endIx;
boolean gotLf = FALSE;
int newStart;

if (lf->reuse)
    {
    lf->reuse = FALSE;
    if (retSize != NULL)
	*retSize = lf->lineEnd - lf->lineStart;
    *retStart = buf + lf->lineStart;
    return TRUE;
    }
/* Find next end of line in buffer. */
for (endIx = lf->lineEnd; endIx < bytesInBuf; ++endIx)
    {
    if (buf[endIx] == '\n')
	{
	gotLf = TRUE;
	endIx += 1;
	break;
	}
    }

/* If not in buffer read in a new buffer's worth. */
while (!gotLf)
    {
    int oldEnd = lf->lineEnd;
    int sizeLeft = bytesInBuf - oldEnd;
    int bufSize = lf->bufSize;
    int readSize = bufSize - sizeLeft;

    if (oldEnd > 0 && sizeLeft > 0)
	memmove(buf, buf+oldEnd, sizeLeft);
    lf->bufOffsetInFile += oldEnd;
    readSize = lineFileLongNetRead(lf->fd, buf+sizeLeft, readSize);
    if (readSize + sizeLeft <= 0)
	{
	lf->bytesInBuf = lf->lineStart = lf->lineEnd = 0;
	return FALSE;
	}
    bytesInBuf = lf->bytesInBuf = readSize + sizeLeft;
    lf->lineEnd = 0;

    /* Look for next end of line.  */
    for (endIx = sizeLeft; endIx <bytesInBuf; ++endIx)
	{
	if (buf[endIx] == '\n')
	    {
	    endIx += 1;
	    gotLf = TRUE;
	    break;
	    }
	}
    if (!gotLf && bytesInBuf == lf->bufSize)
        {
	if (bufSize > 1024*1024)
	    {
	    errAbort("Line too long (more than %d chars) line %d of %s",
		lf->bufSize, lf->lineIx+1, lf->fileName);
	    }
	else
	    {
	    lineFileExpandBuf(lf, bufSize*2);
	    buf = lf->buf;
	    }
	}
    }

if (lf->zTerm)
    {
    buf[endIx-1] = 0;
    }
lf->lineStart = newStart = lf->lineEnd;
lf->lineEnd = endIx;
++lf->lineIx;
if (retSize != NULL)
    *retSize = endIx - newStart;
*retStart = buf + newStart;
return TRUE;
}

void lineFileNeedNext(struct lineFile *lf, char **retStart, int *retSize)
/* Fetch next line from file.  Squawk and die if it's not there. */
{
if (!lineFileNext(lf, retStart, retSize))
    errAbort("Unexpected end of file in %s", lf->fileName);
}

void lineFileClose(struct lineFile **pLf)
/* Close up a line file. */
{
struct lineFile *lf;
if ((lf = *pLf) != NULL)
    {
    if (lf->fd > 0 && lf->fd != fileno(stdin))
	close(lf->fd);
    freeMem(lf->fileName);
    freeMem(lf->buf);
    freez(pLf);
    }
}

void lineFileExpectWords(struct lineFile *lf, int expecting, int got)
/* Check line has right number of words. */
{
if (expecting != got)
    errAbort("Expecting %d words line %d of %s got %d", 
	    expecting, lf->lineIx, lf->fileName, got);
}

int lineFileChopNext(struct lineFile *lf, char *words[], int maxWords)
/* Return next non-blank line that doesn't start with '#' chopped into words. */
{
int lineSize, wordCount;
char *line;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopByWhite(line, words, maxWords);
    if (wordCount != 0)
        return wordCount;
    }
return 0;
}

boolean lineFileNextRow(struct lineFile *lf, char *words[], int wordCount)
/* Return next non-blank line that doesn't start with '#' chopped into words.
 * Returns FALSE at EOF.  Aborts on error. */
{
int wordsRead;
wordsRead = lineFileChopNext(lf, words, wordCount);
if (wordsRead == 0)
    return FALSE;
if (wordsRead < wordCount)
    lineFileExpectWords(lf, wordCount, wordsRead);
return TRUE;
}

int lineFileNeedNum(struct lineFile *lf, char *words[], int wordIx)
/* Make sure that words[wordIx] is an ascii integer, and return
 * binary representation of it. */
{
char *ascii = words[wordIx];
char c = ascii[0];
if (c != '-' && !isdigit(c))
    errAbort("Expecting number field %d line %d of %s, got %s", 
    	wordIx+1, lf->lineIx, lf->fileName, ascii);
return atoi(ascii);
}

void lineFileSkip(struct lineFile *lf, int lineCount)
/* Skip a number of lines. */
{
int i, lineSize;
char *line;

for (i=0; i<lineCount; ++i)
    {
    if (!lineFileNext(lf, &line, &lineSize))
        errAbort("Premature end of file in %s", lf->fileName);
    }
}
