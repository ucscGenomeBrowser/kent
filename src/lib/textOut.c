/* textOut - set up stdout to be HTTP text, file or compressed file. */

#include "common.h"
#include "errabort.h"
#include "cheapcgi.h"
#include "pipeline.h"
#include "textOut.h"


static void textOutWarnHandler(char *format, va_list args)
/* Text mode error message handler. */
{
char *hLine =
"---------------------------------------------------------------------------\n";
if (format != NULL) {
    fflush(stdout);
    fprintf(stdout, "%s", hLine);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fprintf(stdout, "%s", hLine);
    }
}

static void textOutAbortHandler()
/* Text mode abort handler. */
{
exit(-1);
}

static char *getCompressSuffix(char *compressType)
/* Return the file dot-suffix (including the dot) for compressType. */
{
static char *gzipSuffix = ".gz";
static char *compressSuffix = ".Z";
static char *bz2Suffix = ".bz2";
static char *zipSuffix = ".zip";
if (sameWord(compressType, textOutCompressGzip))
    return gzipSuffix;
else if (sameWord(compressType, textOutCompressCompress))
    return compressSuffix;
else if (sameWord(compressType, textOutCompressBzip2))
    return bz2Suffix;
else if (sameWord(compressType, textOutCompressZip))
    return zipSuffix;
else
    errAbort("getCompressSuffix: Unsupported textOutCompress type %s",
	     compressType);
return NULL;
}

static char **getCompressor(char *compressType)
/* Return a compressor specification for pipelineOpen1(). */
{
static char *GZ_WRITE[] = {"gzip", "-qc", NULL};
static char *Z_WRITE[] = {"compress", "-c", NULL};
static char *BZ2_WRITE[] = {"bzip2", "-qzc", NULL};
static char *ZIP_WRITE[] = {"zip", "-q", NULL};

if (sameWord(compressType, textOutCompressGzip))
    return GZ_WRITE;
else if (sameWord(compressType, textOutCompressCompress))
    return Z_WRITE;
else if (sameWord(compressType, textOutCompressBzip2))
    return BZ2_WRITE;
else if (sameWord(compressType, textOutCompressZip))
    return ZIP_WRITE;
else
    errAbort("getCompressor: Unsupported textOutCompress type %s",
	     compressType);
return NULL;
}

static void cleanEnvVars(char *compressType)
/* Ensure vanilla behavior of compressors by removing environment variables 
 * that they read for option settings. */
{
if (sameWord(compressType, textOutCompressGzip))
    {
    unsetenv("GZIP");
    unsetenv("GZIP_OPT");
    }
else if (sameWord(compressType, textOutCompressCompress))
    {
    /* No environment variables mentioned in man page. */
    }
else if (sameWord(compressType, textOutCompressBzip2))
    {
    unsetenv("BZIP");
    unsetenv("BZIP2");
    }
else if (sameWord(compressType, textOutCompressZip))
    {
    unsetenv("ZIPOPT");
    }
else
    {
    errAbort("cleanEnvVars: Unsupported textOutCompress type %s",
	     compressType);
    }
}


struct pipeline *textOutInit(char *fileName, char *compressType)
/* Set up stdout to be HTTP text, file (if fileName is specified), or 
 * compressed file (if both fileName and compressType are specified -- 
 * see textOut.h for supported compression types).  
 * Return NULL if no compression, otherwise a pipeline handle on which 
 * textOutClose should be called when we're done writing stdout. */
{
struct pipeline *compressPipeline = NULL;

trimSpaces(fileName);
if (isEmpty(fileName))
    {
    printf("Content-Type: text/plain\n\n");
    }
else if (isEmpty(compressType) || sameWord(compressType, textOutCompressNone))
    {
    printf("Content-Type: application/octet-stream\n");
    printf("Content-Disposition: attachment; filename=%s\n\n", fileName);
    }
else
    {
    char *suffix = getCompressSuffix(compressType);

    printf("Content-Type: application/x-%s\n", compressType);
    if (endsWith(fileName, suffix))
	printf("Content-Disposition: attachment; filename=%s\n\n", fileName);
    else
	printf("Content-Disposition: attachment; filename=%s%s\n\n",
	       fileName, suffix);

    /* Send the Content header uncompressed! */
    fflush(stdout);

    /* Make sure no environment variables interfere with compressor. */
    cleanEnvVars(compressType);

    /* Redirect stdout to compressor pipeline object. */
    compressPipeline = pipelineOpen1(getCompressor(compressType),
				     pipelineWrite, NULL, NULL);
    if (-1 == dup2(pipelineFd(compressPipeline), STDOUT_FILENO))
	errnoAbort("dup2(pipelineFd %d, stdout %d) failed in textOpen()",
		   pipelineFd(compressPipeline), STDOUT_FILENO);
    }
pushWarnHandler(textOutWarnHandler);
pushAbortHandler(textOutAbortHandler);
return(compressPipeline);
}

void textOutClose(struct pipeline **pCompressPipeline)
/* Flush and close stdout, wait for the pipeline to finish, and then free 
 * the pipeline object. */
{
if (pCompressPipeline && *pCompressPipeline)
    {
    fflush(stdout);
    fclose(stdout);
    pipelineWait(*pCompressPipeline);
    pipelineFree(pCompressPipeline);
    }
}

