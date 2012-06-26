/* lineFile - stuff to rapidly read text files and parse them into
 * lines.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include <fcntl.h>
#include <signal.h>
#include "dystring.h"
#include "errabort.h"
#include "linefile.h"
#include "pipeline.h"
#include "localmem.h"

char *getFileNameFromHdrSig(char *m)
/* Check if header has signature of supported compression stream,
   and return a phoney filename for it, or NULL if no sig found. */
{
char buf[20];
char *ext=NULL;
if (startsWith("\x1f\x8b",m)) ext = "gz";
else if (startsWith("\x1f\x9d\x90",m)) ext = "Z";
else if (startsWith("BZ",m)) ext = "bz2";
else if (startsWith("PK\x03\x04",m)) ext = "zip";
if (ext==NULL)
    return NULL;
safef(buf, sizeof(buf), "somefile.%s", ext);
return cloneString(buf);
}

static char **getDecompressor(char *fileName)
/* if a file is compressed, return the command to decompress the
 * approriate format, otherwise return NULL */
{
static char *GZ_READ[] = {"gzip", "-dc", NULL};
static char *Z_READ[] = {"gzip", "-dc", NULL};
static char *BZ2_READ[] = {"bzip2", "-dc", NULL};
static char *ZIP_READ[] = {"gzip", "-dc", NULL};

if (endsWith(fileName, ".gz"))
    return GZ_READ;
else if (endsWith(fileName, ".Z"))
    return Z_READ;
else if (endsWith(fileName, ".bz2"))
    return BZ2_READ;
else if (endsWith(fileName, ".zip"))
    return ZIP_READ;
else
    return NULL;
}

static void metaDataAdd(struct lineFile *lf, char *line)
/* write a line of metaData to output file
 * internal function called by lineFileNext */
{
struct metaOutput *meta = NULL;

if (lf->isMetaUnique)
    {
    /* suppress repetition of comments */
    if (hashLookup(lf->metaLines, line))
        {
        return;
        }
    hashAdd(lf->metaLines, line, NULL);
    }
for (meta = lf->metaOutput ; meta != NULL ; meta = meta->next)
    if (line != NULL && meta->metaFile != NULL)
        fprintf(meta->metaFile,"%s\n", line);
}

static void metaDataFree(struct lineFile *lf)
/* free saved comments */
{
if (lf->isMetaUnique && lf->metaLines)
    freeHash(&lf->metaLines);
}

void lineFileSetMetaDataOutput(struct lineFile *lf, FILE *f)
/* set file to write meta data to,
 * should be called before reading from input file */
{
struct metaOutput *meta = NULL;
if (lf == NULL)
    return;
AllocVar(meta);
meta->next = NULL;
meta->metaFile = f;
slAddHead(&lf->metaOutput, meta);
}

void lineFileSetUniqueMetaData(struct lineFile *lf)
/* suppress duplicate lines in metadata */
{
lf->isMetaUnique = TRUE;
lf->metaLines = hashNew(8);
}

static char * headerBytes(char *fileName, int numbytes)
/* Return specified number of header bytes from file
 * if file exists as a string which should be freed. */
{
int fd,bytesread=0;
char *result = NULL;
if ((fd = open(fileName, O_RDONLY)) >= 0)
    {
    result=needMem(numbytes+1);
    if ((bytesread=read(fd,result,numbytes)) < numbytes)
	freez(&result);  /* file too short? can read numbytes */
    else
	result[numbytes]=0;
    close(fd);
    }
return result;
}


struct lineFile *lineFileDecompress(char *fileName, bool zTerm)
/* open a linefile with decompression */
{
struct pipeline *pl;
struct lineFile *lf;
char *testName = NULL;
char *testbytes = NULL;    /* the header signatures for .gz, .bz2, .Z,
			    * .zip are all 2-4 bytes only */
if (fileName==NULL)
  return NULL;
testbytes=headerBytes(fileName,4);
if (!testbytes)
    return NULL;  /* avoid error from pipeline */
testName=getFileNameFromHdrSig(testbytes);
freez(&testbytes);
if (!testName)
    return NULL;  /* avoid error from pipeline */
pl = pipelineOpen1(getDecompressor(fileName), pipelineRead, fileName, NULL);
lf = lineFileAttach(fileName, zTerm, pipelineFd(pl));
lf->pl = pl;
return lf;
}

struct lineFile *lineFileDecompressFd(char *name, bool zTerm, int fd)
/* open a linefile with decompression from a file or socket descriptor */
{
struct pipeline *pl;
struct lineFile *lf;
pl = pipelineOpenFd1(getDecompressor(name), pipelineRead, fd, STDERR_FILENO);
lf = lineFileAttach(name, zTerm, pipelineFd(pl));
lf->pl = pl;
return lf;
}



struct lineFile *lineFileDecompressMem(bool zTerm, char *mem, long size)
/* open a linefile with decompression from a memory stream */
{
struct pipeline *pl;
struct lineFile *lf;
char *fileName = getFileNameFromHdrSig(mem);
if (fileName==NULL)
  return NULL;
pl = pipelineOpenMem1(getDecompressor(fileName), pipelineRead, mem, size, STDERR_FILENO);
lf = lineFileAttach(fileName, zTerm, pipelineFd(pl));
lf->pl = pl;
return lf;
}



struct lineFile *lineFileAttach(char *fileName, bool zTerm, int fd)
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

struct lineFile *lineFileOnString(char *name, bool zTerm, char *s)
/* Wrap a line file object around string in memory. This buffer
 * have zeroes written into it and be freed when the line file
 * is closed. */
{
struct lineFile *lf;
AllocVar(lf);
lf->fileName = cloneString(name);
lf->fd = -1;
lf->bufSize = lf->bytesInBuf = strlen(s);
lf->zTerm = zTerm;
lf->buf = s;
return lf;
}

struct lineFile *lineFileTabixMayOpen(char *fileOrUrl, bool zTerm)
/* Wrap a line file around a data file that has been compressed and indexed
 * by the tabix command line program.  The index file <fileOrUrl>.tbi must be
 * readable in addition to fileOrUrl. If there's a problem, warn & return NULL.
 * This works only if kent/src has been compiled with USE_TABIX=1 and linked
 * with the tabix C library. */
{
#ifdef USE_TABIX
int tbiNameSize = strlen(fileOrUrl) + strlen(".tbi") + 1;
char *tbiName = needMem(tbiNameSize);
safef(tbiName, tbiNameSize, "%s.tbi", fileOrUrl);
tabix_t *tabix = ti_open(fileOrUrl, tbiName);
if (tabix == NULL)
    {
    warn("Unable to open \"%s\"", fileOrUrl);
    freez(&tbiName);
    return NULL;
    }
if ((tabix->idx = ti_index_load(tbiName)) == NULL)
    {
    warn("Unable to load tabix index from \"%s\"", tbiName);
    ti_close(tabix);
    tabix = NULL;
    freez(&tbiName);
    return NULL;
    }
struct lineFile *lf = needMem(sizeof(struct lineFile));
lf->fileName = cloneString(fileOrUrl);
lf->fd = -1;
lf->bufSize = 64 * 1024;
lf->buf = needMem(lf->bufSize);
lf->zTerm = zTerm;
lf->tabix = tabix;
lf->tabixIter = ti_iter_first();
freez(&tbiName);
return lf;
#else // no USE_TABIX
warn(COMPILE_WITH_TABIX, "lineFileTabixMayOpen");
return NULL;
#endif // no USE_TABIX
}

boolean lineFileSetTabixRegion(struct lineFile *lf, char *seqName, int start, int end)
/* Assuming lf was created by lineFileTabixMayOpen, tell tabix to seek to the specified region
 * and return TRUE (or if there are no items in region, return FALSE). */
{
#ifdef USE_TABIX
if (lf->tabix == NULL)
    errAbort("lineFileSetTabixRegion: lf->tabix is NULL.  Did you open lf with lineFileTabixMayOpen?");
if (seqName == NULL)
    return FALSE;
int tabixSeqId = ti_get_tid(lf->tabix->idx, seqName);
if (tabixSeqId < 0 && startsWith("chr", seqName))
    // We will get some files that have chr-less Ensembl chromosome names:
    tabixSeqId = ti_get_tid(lf->tabix->idx, seqName+strlen("chr"));
if (tabixSeqId < 0)
    return FALSE;
ti_iter_t iter = ti_queryi(lf->tabix, tabixSeqId, start, end);
if (iter == NULL)
    return FALSE;
if (lf->tabixIter != NULL)
    ti_iter_destroy(lf->tabixIter);
lf->tabixIter = iter;
lf->bufOffsetInFile = ti_bgzf_tell(lf->tabix->fp);
lf->bytesInBuf = 0;
lf->lineIx = -1;
lf->lineStart = 0;
lf->lineEnd = 0;
return TRUE;
#else // no USE_TABIX
warn(COMPILE_WITH_TABIX, "lineFileSetTabixRegion");
return FALSE;
#endif // no USE_TABIX
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
return lineFileAttach("stdin", zTerm, fileno(stdin));
}

struct lineFile *lineFileMayOpen(char *fileName, bool zTerm)
/* Try and open up a lineFile. */
{
if (sameString(fileName, "stdin"))
    return lineFileStdin(zTerm);
else if (getDecompressor(fileName) != NULL)
    return lineFileDecompress(fileName, zTerm);
else
    {
    int fd = open(fileName, O_RDONLY);
    if (fd == -1)
        return NULL;
    return lineFileAttach(fileName, zTerm, fd);
    }
}

struct lineFile *lineFileOpen(char *fileName, bool zTerm)
/* Open up a lineFile or die trying. */
{
struct lineFile *lf = lineFileMayOpen(fileName, zTerm);
if (lf == NULL)
    errAbort("Couldn't open %s , %s", fileName, strerror(errno));
return lf;
}

void lineFileReuse(struct lineFile *lf)
/* Reuse current line. */
{
lf->reuse = TRUE;
}


INLINE void noTabixSupport(struct lineFile *lf, char *where)
{
#ifdef USE_TABIX
if (lf->tabix != NULL)
    lineFileAbort(lf, "%s: not implemented for lineFile opened with lineFileTabixMayOpen.", where);
#endif // USE_TABIX
}

void lineFileSeek(struct lineFile *lf, off_t offset, int whence)
/* Seek to read next line from given position. */
{
noTabixSupport(lf, "lineFileSeek");
if (lf->checkSupport)
    lf->checkSupport(lf, "lineFileSeek");
if (lf->pl != NULL)
    errnoAbort("Can't lineFileSeek on a compressed file: %s", lf->fileName);
lf->reuse = FALSE;
if (whence == SEEK_SET && offset >= lf->bufOffsetInFile
	&& offset < lf->bufOffsetInFile + lf->bytesInBuf)
    {
    lf->lineStart = lf->lineEnd = offset - lf->bufOffsetInFile;
    }
else
    {
    lf->lineStart = lf->lineEnd = lf->bytesInBuf = 0;
    if ((lf->bufOffsetInFile = lseek(lf->fd, offset, whence)) == -1)
	errnoAbort("Couldn't lineFileSeek %s", lf->fileName);
    }
}

void lineFileRewind(struct lineFile *lf)
/* Return lineFile to start. */
{
lineFileSeek(lf, 0, SEEK_SET);
lf->lineIx = 0;
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

static void determineNlType(struct lineFile *lf, char *buf, int bufSize)
/* determine type of newline used for the file, assumes buffer not empty */
{
char *c = buf;
if (bufSize==0) return;
if (lf->nlType != nlt_undet) return;  /* if already determined just exit */
lf->nlType = nlt_unix;  /* start with default of unix lf type */
while (c < buf+bufSize)
    {
    if (*c=='\r')
	{
    	lf->nlType = nlt_mac;
	if (++c < buf+bufSize)
    	    if (*c == '\n')
    		lf->nlType = nlt_dos;
	return;
	}
    if (*(c++) == '\n')
	{
	return;
	}
    }
}

boolean lineFileNext(struct lineFile *lf, char **retStart, int *retSize)
/* Fetch next line from file. */
{
char *buf = lf->buf;
int bytesInBuf = lf->bytesInBuf;
int endIx = lf->lineEnd;
boolean gotLf = FALSE;
int newStart;

if (lf->reuse)
    {
    lf->reuse = FALSE;
    if (retSize != NULL)
	*retSize = lf->lineEnd - lf->lineStart;
    *retStart = buf + lf->lineStart;
    if (lf->metaOutput && *retStart[0] == '#')
        metaDataAdd(lf, *retStart);
    return TRUE;
    }

if (lf->nextCallBack)
    return lf->nextCallBack(lf, retStart, retSize);


#ifdef USE_TABIX
if (lf->tabix != NULL && lf->tabixIter != NULL)
    {
    // Just use line-oriented ti_read:
    int lineSize = 0;
    const char *line = ti_read(lf->tabix, lf->tabixIter, &lineSize);
    if (line == NULL)
	return FALSE;
    lf->bufOffsetInFile = -1;
    lf->bytesInBuf = lineSize;
    lf->lineIx = -1;
    lf->lineStart = 0;
    lf->lineEnd = lineSize;
    if (lineSize > lf->bufSize)
	// shouldn't be!  but just in case:
	lineFileExpandBuf(lf, lineSize * 2);
    safecpy(lf->buf, lf->bufSize, line);
    *retStart = lf->buf;
    if (retSize != NULL)
	*retSize = lineSize;
    return TRUE;
    }
#endif // USE_TABIX

determineNlType(lf, buf+endIx, bytesInBuf);

/* Find next end of line in buffer. */
switch(lf->nlType)
    {
    case nlt_unix:
    case nlt_dos:
	for (endIx = lf->lineEnd; endIx < bytesInBuf; ++endIx)
	    {
	    if (buf[endIx] == '\n')
		{
		gotLf = TRUE;
		endIx += 1;
		break;
		}
	    }
	break;
    case nlt_mac:
	for (endIx = lf->lineEnd; endIx < bytesInBuf; ++endIx)
	    {
	    if (buf[endIx] == '\r')
		{
		gotLf = TRUE;
		endIx += 1;
		break;
		}
	    }
	break;
    case nlt_undet:
	break;
    }

/* If not in buffer read in a new buffer's worth. */
while (!gotLf)
    {
    int oldEnd = lf->lineEnd;
    int sizeLeft = bytesInBuf - oldEnd;
    int bufSize = lf->bufSize;
    int readSize = bufSize - sizeLeft;

    if (oldEnd > 0 && sizeLeft > 0)
	{
	memmove(buf, buf+oldEnd, sizeLeft);
	}
    lf->bufOffsetInFile += oldEnd;
    if (lf->fd >= 0)
	readSize = lineFileLongNetRead(lf->fd, buf+sizeLeft, readSize);
#ifdef USE_TABIX
    else if (lf->tabix != NULL && readSize > 0)
	{
	readSize = ti_bgzf_read(lf->tabix->fp, buf+sizeLeft, readSize);
	if (readSize < 1)
	    return FALSE;
	}
#endif // USE_TABIX
    else
        readSize = 0;

    if ((readSize == 0) && (endIx > oldEnd))
	{
	endIx = sizeLeft;
	buf[endIx] = 0;
	lf->bytesInBuf = newStart = lf->lineStart = 0;
	lf->lineEnd = endIx;
	++lf->lineIx;
	if (retSize != NULL)
	    *retSize = endIx - newStart;
	*retStart = buf + newStart;
        if (*retStart[0] == '#')
            metaDataAdd(lf, *retStart);
	return TRUE;
	}
    else if (readSize <= 0)
	{
	lf->bytesInBuf = lf->lineStart = lf->lineEnd = 0;
	return FALSE;
	}
    bytesInBuf = lf->bytesInBuf = readSize + sizeLeft;
    lf->lineEnd = 0;

    determineNlType(lf, buf+endIx, bytesInBuf);

    /* Look for next end of line.  */
    switch(lf->nlType)
	{
    	case nlt_unix:
	case nlt_dos:
	    for (endIx = sizeLeft; endIx <bytesInBuf; ++endIx)
		{
		if (buf[endIx] == '\n')
		    {
		    endIx += 1;
		    gotLf = TRUE;
		    break;
		    }
		}
	    break;
	case nlt_mac:
	    for (endIx = sizeLeft; endIx <bytesInBuf; ++endIx)
		{
		if (buf[endIx] == '\r')
		    {
		    endIx += 1;
		    gotLf = TRUE;
		    break;
		    }
		}
	    break;
	case nlt_undet:
	    break;
	}
    if (!gotLf && bytesInBuf == lf->bufSize)
        {
	if (bufSize >= 512*1024*1024)
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
    if ((lf->nlType == nlt_dos) && (buf[endIx-2]=='\r'))
	{
	buf[endIx-2] = 0;
	}
    }

lf->lineStart = newStart = lf->lineEnd;
lf->lineEnd = endIx;
++lf->lineIx;
if (retSize != NULL)
    *retSize = endIx - newStart;
*retStart = buf + newStart;
if (*retStart[0] == '#')
    metaDataAdd(lf, *retStart);
return TRUE;
}

void lineFileVaAbort(struct lineFile *lf, char *format, va_list args)
/* Print file name, line number, and error message, and abort. */
{
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy,  "Error line %d of %s: ", lf->lineIx, lf->fileName);
dyStringVaPrintf(dy, format, args);
errAbort("%s", dy->string);
dyStringFree(&dy);
}

void lineFileAbort(struct lineFile *lf, char *format, ...)
/* Print file name, line number, and error message, and abort. */
{
va_list args;
va_start(args, format);
lineFileVaAbort(lf, format, args);
va_end(args);
}

void lineFileUnexpectedEnd(struct lineFile *lf)
/* Complain about unexpected end of file. */
{
errAbort("Unexpected end of file in %s", lf->fileName);
}

void lineFileNeedNext(struct lineFile *lf, char **retStart, int *retSize)
/* Fetch next line from file.  Squawk and die if it's not there. */
{
if (!lineFileNext(lf, retStart, retSize))
    lineFileUnexpectedEnd(lf);
}

void lineFileClose(struct lineFile **pLf)
/* Close up a line file. */
{
struct lineFile *lf;
if ((lf = *pLf) != NULL)
    {
    if (lf->pl != NULL)
        {
        pipelineWait(lf->pl);
        pipelineFree(&lf->pl);
        }
    else if (lf->fd > 0 && lf->fd != fileno(stdin))
	{
	close(lf->fd);
	freeMem(lf->buf);
	}
#ifdef USE_TABIX
    else if (lf->tabix != NULL)
	{
	if (lf->tabixIter != NULL)
	    ti_iter_destroy(lf->tabixIter);
	ti_close(lf->tabix);
	}
#endif // USE_TABIX
    if (lf->closeCallBack)
        lf->closeCallBack(lf);
    freeMem(lf->fileName);
    metaDataFree(lf);
    freez(pLf);
    }
}

void lineFileCloseList(struct lineFile **pList)
/* Close up a list of line files. */
{
struct lineFile *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    lineFileClose(&el);
    }
*pList = NULL;
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

void lineFileShort(struct lineFile *lf)
/* Complain that line is too short. */
{
errAbort("Short line %d of %s", lf->lineIx, lf->fileName);
}

void lineFileReuseFull(struct lineFile *lf)
// Reuse last full line read.  Unlike lineFileReuse,
// lineFileReuseFull only works with previous lineFileNextFull call
{
assert(lf->fullLine != NULL);
lf->fullLineReuse = TRUE;
}


boolean lineFileNextFull(struct lineFile *lf, char **retFull, int *retFullSize,
                        char **retRaw, int *retRawSize)
// Fetch next line from file joining up any that are continued by ending '\'
// If requested, and was joined, the unjoined raw lines are also returned
// NOTE: comment lines can't be continued!  ("# comment \ \n more comment" is 2 lines.)
{
// May have requested reusing the last full line.
if (lf->fullLineReuse)
    {
    lf->fullLineReuse = FALSE;
    assert(lf->fullLine != NULL);
    *retFull = dyStringContents(lf->fullLine);
    if (retFullSize)
        *retFullSize = dyStringLen(lf->fullLine);
    if (retRaw != NULL)
        {
        assert(lf->rawLines != NULL);
        *retRaw = dyStringContents(lf->rawLines);
        if (retRawSize)
            *retRawSize = dyStringLen(lf->rawLines);
        }
    return TRUE;
    }

// Empty pointers
*retFull = NULL;
if (retRaw != NULL)
    *retRaw = NULL;

// Prepare lf buffers
if (lf->fullLine == NULL)
    {
    lf->fullLine = dyStringNew(1024);
    lf->rawLines = dyStringNew(1024); // Better to always create it than test every time
    }
else
    {
    dyStringClear(lf->fullLine);
    dyStringClear(lf->rawLines);
    }

char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *start = skipLeadingSpaces(line);

    // Will the next line continue this one?
    char *end = start;
    if (*start == '#')  // Comment lines can't be continued!
        end = start + strlen(start);
    else
        {
        while (*end != '\0')  // walking forward for efficiency (avoid strlens())
            {
            for (;*end != '\0' && *end != '\\'; end++) ; // Tight loop to find '\'
            if (*end == '\0')
                break;

            // This could be a continuation
            char *slash = end;
            if (*(++end) == '\\')  // escaped
                continue;
            end = skipLeadingSpaces(end);

            if (*end == '\0') // Just whitespace after '\', so true continuation mark
                {
                if (retRaw != NULL) // Only if actually requested.
                    {
                    dyStringAppendN(lf->rawLines,line,(end - line));
                    dyStringAppendC(lf->rawLines,'\n'); // New lines delimit raw lines.
                    }
                end = slash; // Don't need to zero, because of appending by length
                break;
                }
            }
        }

    // Stitch together full lines
    if (dyStringLen(lf->fullLine) == 0)
        dyStringAppendN(lf->fullLine,line,(end - line)); // includes first line's whitespace
    else if (start < end)             // don't include continued line's leading spaces
        dyStringAppendN(lf->fullLine,start,(end - start));

    if (*end == '\\')
        continue;

    // Got a full line now!
    *retFull = dyStringContents(lf->fullLine);
    if (retFullSize)
        *retFullSize = dyStringLen(lf->fullLine);

    if (retRaw != NULL && dyStringLen(lf->rawLines) > 0) // Only if actually requested & continued
        {
        // This is the final line which doesn't have a continuation char
        dyStringAppendN(lf->rawLines,line,(end - line));
        *retRaw = dyStringContents(lf->rawLines);
        if (retRawSize)
            *retRawSize = dyStringLen(lf->rawLines);
        }
    return TRUE;
    }
return FALSE;
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

boolean lineFileNextFullReal(struct lineFile *lf, char **retStart)
// Fetch next line from file that is not blank and does not start with a '#'.
// Continuation lines (ending in '\') are joined into a single line.
{
while (lineFileNextFull(lf, retStart, NULL, NULL, NULL))
    {
    char *clippedText = skipLeadingSpaces(*retStart);
    if (clippedText[0] != '\0' && clippedText[0] != '#')
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

int lineFileChopCharNext(struct lineFile *lf, char sep, char *words[], int maxWords)
/* Return next non-blank line that doesn't start with '#' chopped into
   words delimited by sep. */
{
int lineSize, wordCount;
char *line;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopByChar(line, sep, words, maxWords);
    if (wordCount != 0)
        return wordCount;
    }
return 0;
}

int lineFileChopNextTab(struct lineFile *lf, char *words[], int maxWords)
/* Return next non-blank line that doesn't start with '#' chopped into words
 * on tabs */
{
int lineSize, wordCount;
char *line;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopByChar(line, '\t', words, maxWords);
    if (wordCount != 0)
        return wordCount;
    }
return 0;
}

boolean lineFileNextCharRow(struct lineFile *lf, char sep, char *words[], int wordCount)
/* Return next non-blank line that doesn't start with '#' chopped into words
 * delimited by sep. Returns FALSE at EOF.  Aborts on error. */
{
int wordsRead;
wordsRead = lineFileChopCharNext(lf, sep, words, wordCount);
if (wordsRead == 0)
    return FALSE;
if (wordsRead < wordCount)
    lineFileExpectWords(lf, wordCount, wordsRead);
return TRUE;
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

boolean lineFileNextRowTab(struct lineFile *lf, char *words[], int wordCount)
/* Return next non-blank line that doesn't start with '#' chopped into words
 * at tabs. Returns FALSE at EOF.  Aborts on error. */
{
int wordsRead;
wordsRead = lineFileChopNextTab(lf, words, wordCount);
if (wordsRead == 0)
    return FALSE;
if (wordsRead < wordCount)
    lineFileExpectWords(lf, wordCount, wordsRead);
return TRUE;
}

int lineFileNeedFullNum(struct lineFile *lf, char *words[], int wordIx)
/* Make sure that words[wordIx] is an ascii integer, and return
 * binary representation of it. Require all chars in word to be digits.*/
{
char *c;
for (c = words[wordIx]; *c; c++)
    {
    if (*c == '-' || isdigit(*c))
        /* NOTE: embedded '-' will be caught by lineFileNeedNum */
        continue;
    errAbort("Expecting integer field %d line %d of %s, got %s",
            wordIx+1, lf->lineIx, lf->fileName, words[wordIx]);
    }
return lineFileNeedNum(lf, words, wordIx);
}

int lineFileNeedNum(struct lineFile *lf, char *words[], int wordIx)
/* Make sure that words[wordIx] is an ascii integer, and return
 * binary representation of it. Conversion stops at first non-digit char. */
{
char *ascii = words[wordIx];
char c = ascii[0];
if (c != '-' && !isdigit(c))
    errAbort("Expecting number field %d line %d of %s, got %s",
    	wordIx+1, lf->lineIx, lf->fileName, ascii);
return atoi(ascii);
}

int lineFileCheckAllIntsNoAbort(char *s, void *val, 
    boolean isSigned, int byteCount, char *typeString, boolean noNeg, 
    char *errMsg, int errMsgSize)
/* Convert string to (signed) integer of the size specified.  
 * Unlike atol assumes all of string is number, no trailing trash allowed.
 * Returns 0 if conversion possible, and value is returned in 'val'
 * Otherwise 1 for empty string or trailing chars, and 2 for numeric overflow,
 * and 3 for (-) sign in unsigned number.
 * Error messages if any are written into the provided buffer.
 * Pass NULL val if you only want validation.
 * Use noNeg if negative values are not allowed despite the type being signed,
 * returns 4. */
{
unsigned long long res = 0, oldRes = 0;
boolean isMinus = FALSE;

if ((byteCount != 1) 
 && (byteCount != 2)
 && (byteCount != 4)
 && (byteCount != 8))
    errAbort("Unexpected error: Invalid byte count for integer size in lineFileCheckAllIntsNoAbort, expected 1 2 4 or 8, got %d.", byteCount);

unsigned long long limit = 0xFFFFFFFFFFFFFFFFULL >> (8*(8-byteCount));

if (isSigned) 
    limit >>= 1;

char *p, *p0 = s;

if (*p0 == '-')
    {
    if (isSigned)
	{
	if (noNeg)
	    {
	    safef(errMsg, errMsgSize, "Negative value not allowed");
	    return 4; 
	    }
	p0++;
	++limit;
	isMinus = TRUE;
	}
    else
	{
	safef(errMsg, errMsgSize, "Unsigned %s may not begin with minus sign (-)", typeString);
	return 3; 
	}
    }
p = p0;
while ((*p >= '0') && (*p <= '9'))
    {
    res *= 10;
    if (res < oldRes)
	{
	safef(errMsg, errMsgSize, "%s%s overflowed", isSigned ? "signed ":"", typeString);
	return 2; 
	}
    oldRes = res;
    res += *p - '0';
    if (res < oldRes)
	{
	safef(errMsg, errMsgSize, "%s%s overflowed", isSigned ? "signed ":"", typeString);
	return 2; 
	}
    if (res > limit)
	{
	safef(errMsg, errMsgSize, "%s%s overflowed, limit=%s%llu", isSigned ? "signed ":"", typeString, isMinus ? "-" : "", limit);
	return 2; 
	}
    oldRes = res;
    p++;
    }
/* test for invalid character, empty, or just a minus */
if (*p != '\0')
    {
    safef(errMsg, errMsgSize, "Trailing characters parsing %s%s", isSigned ? "signed ":"", typeString);
    return 1;
    }
if (p == p0)
    {
    safef(errMsg, errMsgSize, "Empty string parsing %s%s", isSigned ? "signed ":"", typeString);
    return 1;
    }

if (!val)
    return 0;  // only validation required

switch (byteCount)
    {
    case 1:
	if (isSigned)
	    {
	    if (isMinus)
		*(char *)val = -res;
	    else
		*(char *)val = res;
	    }
	else
	    *(unsigned char *)val = res;
	break;
    case 2:
	if (isSigned)
	    {
	    if (isMinus)
		*(int *)val = -res;
	    else
		*(int *)val = res;
	    }
	else
	    *(unsigned *)val = res;
	break;
    case 4:
	if (isSigned)
	    {
	    if (isMinus)
		*(long long *)val = -res;
	    else
		*(long long *)val = res;
	    }
	else
	    *(unsigned long long *)val = res;
	break;
    case 8:
	if (isSigned)
	    {
	    if (isMinus)
		*(long long *)val = -res;
	    else
		*(long long *) val =res;
	    }
	else
	    *(unsigned long long *)val = res;
	break;
    }


return 0;
}

void lineFileAllInts(struct lineFile *lf, char *words[], int wordIx, void *val,
  boolean isSigned,  int byteCount, char *typeString, boolean noNeg)
/* Returns long long integer from converting the input string. Aborts on error. */
{
char *s = words[wordIx];
char errMsg[256];
int res = lineFileCheckAllIntsNoAbort(s, val, isSigned, byteCount, typeString, noNeg, errMsg, sizeof errMsg);
if (res > 0)
    {
    errAbort("%s in field %d line %d of %s, got %s",
	errMsg, wordIx+1, lf->lineIx, lf->fileName, s);
    }
}

int lineFileAllIntsArray(struct lineFile *lf, char *words[], int wordIx, void *array, int arraySize,
  boolean isSigned,  int byteCount, char *typeString, boolean noNeg)
/* Convert comma separated list of numbers to an array.  Pass in
 * array and max size of array. Aborts on error. Returns number of elements in parsed array. */
{
char *s = words[wordIx];
char errMsg[256];
unsigned count = 0;
char *cArray = array;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
        break;
    e = strchr(s, ',');
    if (e)
        *e = 0;
    int res = lineFileCheckAllIntsNoAbort(s, cArray, isSigned, byteCount, typeString, noNeg, errMsg, sizeof errMsg);
    if (res > 0)
	{
	errAbort("%s in column %d of array field %d line %d of %s, got %s",
	    errMsg, count, wordIx+1, lf->lineIx, lf->fileName, s);
	}
    if (cArray) // NULL means validation only.
	cArray += byteCount;  
    count++;
    if (e)  // restore input string
        *e++ = ',';
    s = e;
    }
return count;
}


double lineFileNeedDouble(struct lineFile *lf, char *words[], int wordIx)
/* Make sure that words[wordIx] is an ascii double value, and return
 * binary representation of it. */
{
char *valEnd;
char *val = words[wordIx];
double doubleValue;

doubleValue = strtod(val, &valEnd);
if ((*val == '\0') || (*valEnd != '\0'))
    errAbort("Expecting double field %d line %d of %s, got %s",
    	wordIx+1, lf->lineIx, lf->fileName, val);
return doubleValue;
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

char *lineFileSkipToLineStartingWith(struct lineFile *lf, char *start, int maxCount)
/* Skip to next line that starts with given string.  Return NULL
 * if no such line found, otherwise return the line. */
{
char *line;
while (lineFileNext(lf, &line, NULL) && --maxCount >= 0)
    {
    if (startsWith(start, line))
        return line;
    }
return NULL;
}

char *lineFileReadAll(struct lineFile *lf)
/* Read remainder of lineFile and return it as a string. */
{
struct dyString *dy = dyStringNew(1024*4);
lf->zTerm = 0;
int size;
char *line;
while (lineFileNext(lf, &line, &size))
    dyStringAppendN(dy, line, size);
return dyStringCannibalize(&dy);
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

void lineFileRemoveInitialCustomTrackLines(struct lineFile *lf)
/* remove initial browser and track lines */
{
char *line;
while (lineFileNextReal(lf, &line))
    {
    if (!(startsWith("browser", line) || startsWith("track", line) ))
        {
        verbose(2, "found line not browser or track: %s\n", line);
        lineFileReuse(lf);
        break;
        }
    verbose(2, "skipping %s\n", line);
    }
}

