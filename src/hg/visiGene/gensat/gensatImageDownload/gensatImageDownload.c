/* gensatImageDownload - Download images from gensat guided by xml file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "xp.h"
#include "xap.h"
#include "../lib/gs.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensatImageDownload - Download images from gensat guided by xml file.\n"
  "usage:\n"
  "   gensatImageDownload gensat.xml outDir outLog\n"
  "options:\n"
  "   -tmp=XXX - Use this temporary file instead of default wgetMd5.tmp\n"
  "   -maxErrs=N - Maximum errors allowed before aborting, default 200\n"
  "   -verbose=N - Set stderr verbosity:  0 quiet, 1 status, 2 debug\n"
  );
}

/* Command line variables. */
char *tmpName = "wgetMd5.tmp";
int maxErrs = 200;

/* Other globals. */
int errCount = 0;	/* Non-fatal error count. */
FILE *fLog;		/* Log file. */

static struct optionSpec options[] = {
   {"tmp", OPTION_STRING},
   {"maxErrs", OPTION_INT},
   {NULL, 0},
};

boolean safeGetOne(char *source, char *dest)
/* Fetch file from source to tmp file.  When fetch
 * is done rename temp file to dest and return TRUE. */
{
struct dyString *command = dyStringNew(0);
boolean ok = TRUE;
int err;

dyStringClear(command);
dyStringPrintf(command, "wget -nv -O %s '%s'", 
    tmpName, source);
verbose(2, "%s\n", command->string);
if ((err = system(command->string)) != 0)
    {
    fprintf(fLog, "Error %d on %s\n", err, command->string);
    warn("Error %d on %s", err, command->string);
    ++errCount;
    if (errCount > maxErrs)
        errAbort("Aborting after %d wget errors", errCount);
    ok = FALSE;
    }
verbose(2, "wget returned %d\n", err);

/* Rename file to proper name */
if (ok)
    {
    if ((err = rename(tmpName, dest)) < 0)
	{
	fprintf(fLog, "Couldn't rename %s to %s\n", tmpName, dest);
	errnoAbort("Couldn't rename %s to %s", tmpName, dest);
	}
    }
dyStringFree(&command);
return ok;
}

  
void gensatImageDownload(char *gensatXml, char *outDir, char *outLog)
/* gensatImageDownload - Download images from gensat guided by xml file.. */
{
struct xap *xap;
struct gsGensatImage *image;
char *ftpUri = "ftp://ftp.ncbi.nih.gov/pub/gensat";
char *jpgCgiUri = "http://www.ncbi.nlm.nih.gov/projects/gensat/gensat_img.cgi?action=image&mode=full&fmt=jpeg&id=";
char finalJpg[PATH_LEN];
char finalDir[PATH_LEN];
char wgetSource[PATH_LEN];
struct hash *dirHash = newHash(16);
struct dyString *mkdir = dyStringNew(0);
int imageIx = 0;

fLog = mustOpen(outLog, "a");
fprintf(fLog, "starting gensatImageDownload from %s to %s\n", gensatXml, outDir);
xap = xapListOpen(gensatXml, "GensatImageSet", gsStartHandler, gsEndHandler);


while ((image = xapListNext(xap, "GensatImage")) != NULL)
    {
    int id = image->gsGensatImageId->text;
    char *imageFile = image->gsGensatImageImageInfo->gsGensatImageImageInfoFullImg
    			->gsGensatImageInfo->gsGensatImageInfoFilename->text;

    /* Mangle file name a little */
    subChar(imageFile, '(', '_');
    stripChar(imageFile, ')');

    /* Figure out name of jpeg file in outDir. */
    verbose(1, "image %d, id %d\n", ++imageIx, id);
    safef(finalJpg, sizeof(finalJpg), "%s/%s", outDir, imageFile);
    stripString(finalJpg, ".full"); /* Image magick can't handle two suffixes */
    chopSuffix(finalJpg);
    strcat(finalJpg, ".jpg");

    /* Create directory that it goes in if necessary */
    splitPath(finalJpg, finalDir, NULL, NULL);
    if (!hashLookup(dirHash, finalDir))
        {
	hashAdd(dirHash, finalDir, NULL);
	dyStringClear(mkdir);
	dyStringPrintf(mkdir, "mkdir -p %s", finalDir);
	if (system(mkdir->string) != 0)
	    errAbort("Couldn't %s", mkdir->string);
	}

    /* Download it - either directly via ftp, or indirectly via cgi. */
    if (fileExists(finalJpg))
	{
	verbose(1, "already have %s\n", imageFile);
	fprintf(fLog, "%s already downloaded\n", finalJpg);
	}
    else
        {
	if (endsWith(imageFile, ".jpg"))
	    {
	    safef(wgetSource, sizeof(wgetSource), "%s/%s", ftpUri, imageFile);
	    if (safeGetOne(wgetSource, finalJpg))
	        fprintf(fLog, "Got via ftp %s\n", finalJpg);
	    }
	else
	    {
	    safef(wgetSource, sizeof(wgetSource), "%s%d", jpgCgiUri, id);
	    if (safeGetOne(wgetSource, finalJpg))
	        fprintf(fLog, "Got via cgi %s\n", finalJpg);
	    }
	}
    }
carefulClose(&fLog);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
gensatImageDownload(argv[1], argv[2], argv[3]);
return 0;
}
