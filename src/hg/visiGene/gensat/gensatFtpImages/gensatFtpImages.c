/* gensatFtpImages - Download images guided by output of gensatFtpList. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensatFtpImages - Download images guided by output of gensatFtpList\n"
  "usage:\n"
  "   gensatFtpImages check.md5 outDir\n"
  "options:\n"
  "   -tmp=XXX - Use this temporary file instead of default wgetMd5.tmp\n"
  "   -md5tmp=XXX - Use this temporary file instead of default wgetMd5.md5\n"
  "   -maxErrs=N - Maximum errors allowed before aborting, default 100\n"
  "   -verbose=N - Set stderr verbosity:  0 quiet, 1 status, 2 debug\n"
  );
}

char *tmpName = "wgetMd5.tmp";
char *md5tmp = "wgetMd5.md5";
int errCount = 0, maxErrs = 100;

static struct optionSpec options[] = {
   {"tmp", OPTION_STRING},
   {"maxErrs", OPTION_INT},
   {NULL, 0},
};


char *uri = "ftp://ftp.ncbi.nih.gov/pub/gensat/Institutions";

boolean safeGetOne(char *source, char *md5, char *dest)
/* Fetch file from source to tmp file.  Check md5.  If
 * it doesn't work return FALSE.  If it does work
 * rename tmp file to dest and return TRUE. */
{
struct dyString *command = dyStringNew(0);
boolean ok = TRUE;
int err;

dyStringClear(command);
dyStringPrintf(command, "wget -nv --timestamping -O %s '%s'", 
    tmpName, source);
verbose(1, "%s\n", command->string);
if ((err = system(command->string)) != 0)
    {
    warn("Error %d on %s", err, command->string);
    ok = FALSE;
    }
verbose(1, "wget returned %d\n", err);

/* Make up a little md5 file. */
if (ok)
    {
    FILE *f = mustOpen(md5tmp, "w");
    fprintf(f, "%s  %s\n", md5, tmpName);
    carefulClose(&f);

    /* Run md5sum. */
    dyStringClear(command);
    dyStringPrintf(command, "md5sum -c %s", md5tmp);
    verbose(1, "%s\n", command->string);
    err = system(command->string);
    verbose(1, "md5sum returned %d\n", err);
    if (err != 0)
	{
	warn("md5sum failed on %s", source);
	ok = FALSE;
	}
    }

/* Rename file to proper name */
if (ok)
    {
    if ((err = rename(tmpName, dest)) < 0)
	errnoAbort("Couldn't rename %s to %s", tmpName, dest);
    }
dyStringFree(&command);
return ok;
}


void gensatFtpImages(char *checkMd5, char *outDir)
/* gensatFtpImages - Download images guided by output of gensatFtpList. */
{
int err;
char source[PATH_LEN], nativeImage[PATH_LEN], jpgImage[PATH_LEN];
char dir[PATH_LEN], file[PATH_LEN], ext[PATH_LEN];
struct lineFile *lf = lineFileOpen(checkMd5, TRUE);
char *line, *md5, *relativePath;

struct dyString *command = dyStringNew(0);

while(lineFileNext(lf, &line, NULL))
    {
    /* Parse out two columns of checkMd5 file. */
    md5 = nextWord(&line);
    relativePath = skipLeadingSpaces(line);

    /* Figure out output path, and if file already exists skip it. */
    safef(nativeImage, sizeof(nativeImage), "%s/%s", outDir, relativePath);
    strcpy(jpgImage, nativeImage);
    if (endsWith(jpgImage, ".bz2"))
	chopSuffix(jpgImage);
    if (endsWith(jpgImage, ".png") || endsWith(jpgImage, ".tif") ||
	endsWith(jpgImage, ".tiff") || endsWith(jpgImage, ".jpeg") ||
    	endsWith(jpgImage, ".jpg") || endsWith(jpgImage, ".JPG") )
        {
	chopSuffix(jpgImage);
	strcat(jpgImage, ".jpg");
	}
    else if (endsWith(jpgImage, ".txt") || endsWith(jpgImage, ".zip")
        || endsWith(jpgImage, ".doc"))
        continue;
    else
        errAbort("Unrecognized image type in file %s", jpgImage);

    if (!fileExists(jpgImage))
	{
	/* Create any directories needed. */
	splitPath(relativePath, dir, file, ext);
	dyStringClear(command);
	dyStringPrintf(command, "mkdir -p '%s/%s'", outDir, dir);
	system(command->string);

	/* wget the file. */
	safef(source, sizeof(source), "%s/%s", uri, relativePath);
	if (safeGetOne(source, md5, nativeImage))
	    {
	    if (endsWith(nativeImage, ".bz2"))
	        {
		dyStringClear(command);
		dyStringPrintf(command, "bunzip2 '%s'", nativeImage);
		verbose(1, "%s\n", command->string);
		err = system(command->string);
		if (err != 0)
		    errAbort("err %d on %s", err, command->string);
		chopSuffix(nativeImage);
		}
	    if (!endsWith(nativeImage, ".jpg") )
	        {
		dyStringClear(command);
		dyStringPrintf(command, "convert '%s' '%s'", nativeImage, jpgImage);
		verbose(1, "%s\n", command->string);
		err = system(command->string);
		if (err != 0)
		    errAbort("err %d on %s", err, command->string);
		remove(nativeImage);
		}
	    }
	else
	    {
	    if (++errCount > maxErrs)
	       errAbort("Aborting after %d errors", errCount);
	    }
	}
    else
        {
	verbose(1, "Already have %s\n", jpgImage);
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tmpName = optionVal("tmp", tmpName);
md5tmp = optionVal("md5tmp", md5tmp);
maxErrs = optionInt("maxErrs", maxErrs);
gensatFtpImages(argv[1], argv[2]);
return 0;
}
