#include "gbFileOps.h"
#include "common.h"
#include "errabort.h"
#include "portable.h"
#include "gbPipeline.h"
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

static char const rcsid[] = "$Id: gbFileOps.c,v 1.1 2003/06/03 01:27:46 markd Exp $";

boolean gbIsReadable(char* path)
/* Test if a file exists and is readable by the user. */
{
if (access(path, R_OK) < 0)
    return FALSE;
return TRUE;
}

void gbMakeDirs(char* path)
/* make a directory, including parent directories */
{
char pathBuf[PATH_LEN];
char* next = pathBuf;

strcpy(pathBuf, path);
if (*next == '/')
    next++;

while((*next != '\0')
      && (next = strchr(next, '/')) != NULL)
    {
    *next = '\0';
    makeDir(pathBuf);
    *next = '/';
    next++;
    }
makeDir(pathBuf);
}

void gbMakeFileDirs(char* path)
/* make all of the directories for a file */
{
char* last = strrchr(path, '/');
if (last != NULL)
    {
    *last = '\0';
    gbMakeDirs(path);
    *last = '/';
    }
}

void gbGetOutputTmp(char *path, char *tmpPath)
/* generate the tmp path name, moving the compress extension if needed */
{
char savedExt[32];
int len;
savedExt[0] = '\0';
strcpy(tmpPath, path);
if (gbGetCompressor(tmpPath, "r") != NULL)
    {
    char *last = strrchr(tmpPath, '.');
    strcpy(savedExt, last);
    *last = '\0';
    }
len = strlen(tmpPath);
safef(tmpPath+len, PATH_LEN-len, ".%d.%s.tmp%s", getpid(), getHost(),
      savedExt);
}

FILE* gbMustOpenOutput(char* path)
/* Open an output file for atomic file creation.  Create the directory if it
 * doesn't exist.  If the path has one of the compress extensions (.gz, .Z,
 * .bz2) the output is compressed. File is opened under a tmp name until
 * installed. */
{
char tmpPath[PATH_LEN];
gbGetOutputTmp(path, tmpPath);

gbMakeFileDirs(tmpPath);
return gzMustOpen(tmpPath, "w");
}

void gbOutputRename(char* path, FILE** fhPtr)
/* Install an output file create by gbOutputOpen.  If fh is not NULL, the file
 * is also closed.  If closed separately, must use gzClose. */
{
char tmpPath[PATH_LEN];
gbGetOutputTmp(path, tmpPath);

if (fhPtr != NULL)
    gzClose(fhPtr);
if (rename(tmpPath, path) < 0)
    errnoAbort("renaming %s to %s", tmpPath, path);
}

void gbOutputRemove(char* path, FILE** fhPtr)
/* Remove an output file instead of installing it. If fh is not NULL, the file
 * is also closed. */
{
char tmpPath[PATH_LEN];
gbGetOutputTmp(path, tmpPath);

if (fhPtr != NULL)
    gzClose(fhPtr);
unlink(tmpPath);    
}

unsigned gbParseUnsigned(struct lineFile* lf, char* numStr)
/* parse a unsigned that was take from a lineFile column  */
{
unsigned res = 0;
char *p = numStr;
char c;

while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
if (c != '\0')
    errAbort("invalid unsigned \"%s\" in %s line %d",
             numStr, lf->fileName, lf->lineIx);
return res;
}

int gbParseInt(struct lineFile* lf, char* numStr)
/* parse an int that was take from a lineFile column  */
{
int res = 0;
char *p = numStr;
char c;

if (*p == '-')
    p++;
while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
if (c != '\0')
    errAbort("invalid int \"%s\" in %s line %d",
             numStr, lf->fileName, lf->lineIx);
if (*numStr == '-')
    return -res;
else
    return res;
}

off_t gbParseFileOff(struct lineFile* lf, char* numStr)
/* parse a file offset that was take from a lineFile column  */
{
off_t res = 0;
char *p = numStr;
char c;

if (*p == '-')
    p++;
while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
if (c != '\0')
    errAbort("invalid file offset \"%s\" in %s line %d",
             numStr, lf->fileName, lf->lineIx);
if (*numStr == '-')
    return -res;
else
    return res;
}


#ifdef __GNUC__
inline
#endif
static unsigned parseUnsigned(char *buf, unsigned iStart, unsigned len,
                              int* isOk)
/* parse a number in a string that is not delimited by a newline, only
 * looking at the specified range.  Sets isOk to FALSE if the number is
 * invalid. */
{
char* end = buf+iStart+len;
char hold = *end;
unsigned res = 0;
char *p = buf+iStart;
char c;

*end = '\0';
while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
*end = hold;
if (c != '\0')
    *isOk = FALSE;
return res;
}

time_t gbParseChkDate(char* dateStr, boolean* isOk)
/* parse a date, in YYYY-MM-DD format, converting it to a unix time (as
 * returned by time()).  Sets isOk to false if an error, does not change
 * if ok */
{
struct tm tm;
time_t numTime;
ZeroVar(&tm);

*isOk = TRUE;

if (!((strlen(dateStr) == 10) && (dateStr[4] == '-') && (dateStr[7] == '-')))
    isOk = FALSE;
tm.tm_year = parseUnsigned(dateStr, 0, 4, isOk)-1900;  /* since 1900 */
tm.tm_mon = parseUnsigned(dateStr, 5, 2, isOk)-1;      /* since jan */
tm.tm_mday = parseUnsigned(dateStr, 8, 2, isOk);       /* 1-31 */

/* convert */
if ((numTime = mktime(&tm)) == -1)
    *isOk = FALSE;
return numTime;
}

time_t gbParseDate(struct lineFile* lf, char* dateStr)
/* parse a date, in YYYY-MM-DD format, converting it to a unix time (as
 * returned by time()).  If lf is not null, the file information will be
 * included in error message. */
{
boolean isOk = TRUE;
time_t numTime = gbParseChkDate(dateStr, &isOk);
if (!isOk)
    {
    if (lf != NULL)
        errAbort("invalid date \"%s\" in %s line %d, must be YYYY-MM-DD",
                 dateStr, lf->fileName, lf->lineIx);
    else
        errAbort("invalid date \"%s\", must be YYYY-MM-DD", dateStr);
    }
return numTime;
}

char* gbFormatDate(time_t date)
/* format a unix date, in YYYY-MM-DD format.
 * Warning: return is a static buffer, however rotated between 4 buffers to
 * make it easy to use in print statements. */
{
static int iBuf = 0;
static char bufs[4][256];
struct tm* tm = gmtime(&date);
if (++iBuf >= 4)
    iBuf = 0;
strftime(bufs[iBuf], 11, "%Y-%m-%d", tm);
return bufs[iBuf];
}

time_t gbParseTimeStamp(char *col)
/* Parse a time stamp, in YYYYMMDDHHMMSS format, converting it to a unix time
 * (as returned by time()). */
{
boolean isOk = TRUE;
struct tm tm;
time_t numTime;
ZeroVar(&tm);

if (strlen(col) != 14)
    isOk = FALSE;
tm.tm_year = parseUnsigned(col, 0, 4, &isOk)-1900;
tm.tm_mon = parseUnsigned(col, 4, 2, &isOk);
tm.tm_mday = parseUnsigned(col, 6, 2, &isOk);
tm.tm_hour = parseUnsigned(col, 8, 2, &isOk);
tm.tm_min = parseUnsigned(col, 10, 2, &isOk);
tm.tm_sec = parseUnsigned(col, 12, 2, &isOk);
/* convert */
if ((numTime = mktime(&tm)) == -1)
    isOk = FALSE;
if (!isOk)
    errAbort("invalid timestamp \"%s\", must be YYYYMMDDHHMMSS", col);

return numTime;
}

char** gbGetCompressor(char *fileName, char *mode)
/* Get two element array with compression program based on the file name and
 * flags flags to pass to compressor.  Returns NULL if not compressed.
 */
{
/* always use fast gzip compress, the difference in compression between 1 and
 * 9 is only about 6% on a large fasta, while it was 20 times faster. */
static char *GZ_READ[] = {
    "gzip", "-dc", NULL
};
static char *GZ_WRITE[] = {
    "gzip", "-c1", NULL
};
static char *Z_READ[] = {
    "compress", "-dc", NULL
};
static char *Z_WRITE[] = {
    "compress", "-c", NULL
};
static char *BZ2_READ[] = {
    "bzip2", "-dc", NULL
};
static char *BZ2_WRITE[] = {
    "bzip2", "-c1", NULL
};

boolean isRead;
if (sameString(mode, "r"))
    isRead = TRUE;
else if (sameString(mode, "w") || sameString(mode, "a"))
    isRead = FALSE;
else
    errAbort("gbGetCompressor: unsupported mode: %s", mode);

if (endsWith(fileName, ".gz"))
    return (isRead ? GZ_READ : GZ_WRITE);
else if (endsWith(fileName, ".Z"))
    return (isRead ? Z_READ : Z_WRITE);
else if (endsWith(fileName, ".bz2"))
    return (isRead ? BZ2_READ : BZ2_WRITE);
else
    return NULL;
}

struct lineFile* gzLineFileOpen(char *fileName)
/* Open a line file, checking the file name to determine if the file is
 * compressed.  If it ends in .gz, .Z, or .bz2 it will be piped through the
 * approriate decompression program. Must use gzLineFileClose() to close the
 * file. */
{
char **compressor = gbGetCompressor(fileName, "r");
if (compressor != NULL)
    {
    char *argv[4], **cmds[2];
    struct gbPipeline *pipeline;
    argv[0] = compressor[0];
    argv[1] = compressor[1];
    argv[2] = fileName;
    argv[3] = NULL;
    cmds[0] = argv;
    cmds[1] = NULL;
    
    pipeline = gbPipelineCreate(cmds, PIPELINE_READ, NULL, NULL);
    return lineFileAttach(gbPipelineDesc(pipeline),
                          TRUE, gbPipelineFd(pipeline));
    }
else
    return lineFileOpen(fileName, TRUE);
}

void gzLineFileClose(struct lineFile** lfPtr)
/* Close a lineFile and process opened by gzLineFileOpen().  Abort if
 * process exits non-zero */
{
struct lineFile* lf = *lfPtr;
if (lf != NULL)
    {
    struct gbPipeline *pipeline = gbPipelineFind(lf->fd);
    if (pipeline != NULL)
        gbPipelineWait(&pipeline);
    lineFileClose(&lf);
    *lfPtr = NULL;
    }
}

FILE* gzMustOpen(char* fileName, char *mode)
/* Open a file for read or write access.  If it ends in .gz, .Z, or .bz2 it
 * will be piped through the approriate decompression program. Must use
 * gzClose() to close the file. */
{
char  **compressor = gbGetCompressor(fileName, mode);
if (compressor != NULL)
    {
    char *argv[4], *outFile, **cmds[2];
    unsigned options = PIPELINE_FDOPEN;
    struct gbPipeline *pipeline;
    argv[0] = compressor[0];
    argv[1] = compressor[1];

    /* compress options */
    if (sameString(mode, "r"))
        {
        argv[2] = fileName;
        argv[3] = NULL;
        outFile = NULL;
        options |= PIPELINE_READ;
        }
    else
        {
        argv[2] = NULL;
        outFile = fileName;
        if (sameString(mode, "w"))
            options |= PIPELINE_WRITE;
        else if (sameString(mode, "a"))
            options |= PIPELINE_APPEND;
        else
            errAbort("invalid mode for gzMustOpen \"%s\"", mode);
        }
    cmds[0] = argv;
    cmds[1] = NULL;

    pipeline = gbPipelineCreate(cmds, options, NULL, outFile);
    return gbPipelineFile(pipeline);
    }
else
    return mustOpen(fileName, mode);
}

void gzClose(FILE** fhPtr)
/* Close a file opened by gzMustOpen().  Abort if compress process exits
 * non-zero or there was an error writing the file. */
{
FILE* fh = *fhPtr;
if (fh != NULL)
    {
    struct gbPipeline *pipeline = gbPipelineFind(fileno(fh));
    if (pipeline != NULL)
        gbPipelineWait(&pipeline);
    else
        carefulClose(&fh);
    *fhPtr = NULL;
    }
}

off_t gbTell(FILE* fh)
/* get offset in file, or abort on error */
{
off_t off = ftello(fh);
if (off < 0)
    errnoAbort("ftello failed");
return off;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

