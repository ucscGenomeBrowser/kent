/* lineFile - stuff to rapidly read text files and parse them into
 * lines. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include <fcntl.h>
#include "dystring.h"
#include "errabort.h"
#include "linefile.h"

struct lineFile *lineFileAttatch(char *fileName, bool zTerm, int fd)
/* Wrap a line file around an open'd file. */
{
struct lineFile *lf;
AllocVar(lf);
lf->fileName = cloneString(fileName);
lf->fd = fd;
lf->bufSize = 64*1024;
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

void lineFileSeek(struct lineFile *lf, off_t offset, int whence)
/* Seek to read next line from given position. */
{
lf->reuse = FALSE;
lf->lineStart = lf->lineEnd = lf->bytesInBuf;
if (lseek(lf->fd, offset, whence) == -1)
    errnoAbort("Couldn't lineFileSeek %s", lf->fileName);
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
    if (readSize <= 0)
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
	if (bufSize >= 64*1024*1024)
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

void lineFileExpectAtLeast(struct lineFile *lf, int expecting, int got)
/* Check line has right number of words. */
{
if (got < expecting)
    errAbort("Expecting at least %d words line %d of %s got %d", 
	    expecting, lf->lineIx, lf->fileName, got);
}


boolean lineFileNextReal(struct lineFile *lf, char **retStart)
/* Fetch next line from file that is not blank and 
 * does not start with a '#'. */
{
char *s, c;
while (lineFileNext(lf, retStart, NULL))
    {
    s = skipLeadingSpaces(*retStart);
    c = s[0];
    if (c != 0 && c != '#')
	return TRUE;
    }
return FALSE;
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

boolean lineFileParseHttpHeader(struct lineFile *lf, char **hdr,
				boolean *chunked, int *contentLength)
/* Extract HTTP response header from lf into hdr, tell if it's 
 * "Transfer-Encoding: chunked" or if it has a contentLength. */
{
  struct dyString *header = newDyString(1024);
  char *line;
  int lineSize;

  if (chunked != NULL)
    *chunked = FALSE;
  if (contentLength != NULL)
    *contentLength = -1;
  dyStringClear(header);
  if (lineFileNext(lf, &line, &lineSize))
    {
      if (startsWith("HTTP/", line))
	{
	char *version, *code;
	dyStringAppendN(header, line, lineSize-1);
	dyStringAppendC(header, '\n');
	version = nextWord(&line);
	code = nextWord(&line);
	if (code == NULL)
	    {
	    warn("%s: Expecting HTTP/<version> <code> header line, got this: %s\n", lf->fileName, header->string);
	    *hdr = cloneString(header->string);
	    dyStringFree(&header);
	    return FALSE;
	    }
	if (!sameString(code, "200"))
	    {
	    warn("%s: Errored HTTP response header: %s %s %s\n", lf->fileName, version, code, line);
	    *hdr = cloneString(header->string);
	    dyStringFree(&header);
	    return FALSE;
	    }
	while (lineFileNext(lf, &line, &lineSize))
	    {
	    /* blank line means end of HTTP header */
	    if ((line[0] == '\r' && line[1] == 0) || line[0] == 0)
	        break;
	    if (strstr(line, "Transfer-Encoding: chunked") && chunked != NULL)
	        *chunked = TRUE;
	    dyStringAppendN(header, line, lineSize-1);
	    dyStringAppendC(header, '\n');
	    if (strstr(line, "Content-Length:"))
	      {
		code = nextWord(&line);
		code = nextWord(&line);
		if (contentLength != NULL)
		    *contentLength = atoi(code);
	      }
	    }
	}
      else
	{
	  /* put the line back, don't put it in header/hdr */
	  lineFileReuse(lf);
	  warn("%s: Expecting HTTP/<version> <code> header line, got this: %s\n", lf->fileName, header->string);
	  *hdr = cloneString(header->string);
	  dyStringFree(&header);
	  return FALSE;
	}
    }
  else
    {
      *hdr = cloneString(header->string);
      dyStringFree(&header);
      return FALSE;
    }

  *hdr = cloneString(header->string);
  dyStringFree(&header);
  return TRUE;
} /* lineFileParseHttpHeader */

struct dyString *lineFileSlurpHttpBody(struct lineFile *lf,
				       boolean chunked, int contentLength)
/* Return a dyString that contains the http response body in lf.  Handle 
 * chunk-encoding and content-length. */
{
  struct dyString *body = newDyString(64*1024);
  char *line;
  int lineSize;

  dyStringClear(body);
  if (chunked)
    {
      /* Handle "Transfer-Encoding: chunked" body */
      /* Procedure from RFC2068 section 19.4.6 */
      char *csword;
      unsigned chunkSize = 0;
      unsigned size;
      do
	{
	  /* Read line that has chunk size (in hex) as first word. */
	  if (lineFileNext(lf, &line, NULL))
	    csword = nextWord(&line);
	  else break;
	  if (sscanf(csword, "%x", &chunkSize) < 1)
	    {
	      warn("%s: chunked transfer-encoding chunk size parse error.\n",
		   lf->fileName);
	      break;
	    }
	  /* If chunk size is 0, read in a blank line & then we're done. */
	  if (chunkSize == 0)
	    {
	      lineFileNext(lf, &line, NULL);
	      if (line == NULL || (line[0] != '\r' && line[0] != 0))
		warn("%s: chunked transfer-encoding: expected blank line, got %s\n",
		     lf->fileName, line);
	      
	      break;
	    }
	  /* Read (and save) lines until we have read in chunk. */
	  for (size = 0;  size < chunkSize;  size += lineSize)
	    {
	      if (! lineFileNext(lf, &line, &lineSize))
		break;
	      dyStringAppendN(body, line, lineSize-1);
	      dyStringAppendC(body, '\n');
	    }
	  /* Read blank line - or extra CRLF inserted in the middle of the 
	   * current line, in which case we need to trim it. */
	  if (size > chunkSize)
	    {
	      body->stringSize -= (size - chunkSize);
	      body->string[body->stringSize] = 0;
	    }
	  else if (size == chunkSize)
	    {
	      lineFileNext(lf, &line, NULL);
	      if (line == NULL || (line[0] != '\r' && line[0] != 0))
		warn("%s: chunked transfer-encoding: expected blank line, got %s\n",
		     lf->fileName, line);
	    }
	} while (chunkSize > 0);
      /* Try to read in next line.  If it's an HTTP header, put it back. */
      /* If there is a next line but it's not an HTTP header, it's a footer. */
      if (lineFileNext(lf, &line, NULL))
	{
	  if (startsWith("HTTP/", line))
	    lineFileReuse(lf);
	  else
	    {
	      /* Got a footer -- keep reading until blank line */
	      warn("%s: chunked transfer-encoding: got footer %s, discarding it.\n",
		   lf->fileName, line);
	      while (lineFileNext(lf, &line, NULL))
		{
		  if ((line[0] == '\r' && line[1] == 0) || line[0] == 0)
		    break;
		  warn("discarding footer line: %s\n", line);
		}
	    }
	}
    }
  else if (contentLength >= 0)
    {
      /* Read in known length */
      int size;
      for (size = 0;  size < contentLength;  size += lineSize)
	{
	  if (! lineFileNext(lf, &line, &lineSize))
	    break;
	  dyStringAppendN(body, line, lineSize-1);
	  dyStringAppendC(body, '\n');
	}
    }
  else
    {
      /* Read in to end of file (assume it's not a persistent connection) */
      while (lineFileNext(lf, &line, &lineSize))
	{
	  dyStringAppendN(body, line, lineSize-1);
	  dyStringAppendC(body, '\n');
	}
    }

  return(body);
} /* lineFileSlurpHttpBody */

