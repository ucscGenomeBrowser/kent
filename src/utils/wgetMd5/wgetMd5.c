/* wgetMd5 - Get files from ftp or http site and check their md5's. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "wgetMd5 - Get files from ftp or http site and check their md5's.\n"
  "usage:\n"
  "   wgetMd5 uri check.md5 outDir\n"
  "The uri is something like:\n"
  "   ftp://ftp.ncbi.nih.gov/pub/genbank\n"
  "The check.md5 is a two column file containing md5 sums file names.\n"
  "The outDir is where to put the resulting downloads\n"
  "options:\n"
  "   -tmp=XXX - Use this temporary file instead of default wgetMd5.tmp\n"
  "   -md5tmp=XXX - Use this temporary file instead of default wgetMd5.md5\n"
  "   -maxErrs=N - Maximum errors allowed before aborting, default 100\n"
  "   -verbose=N - Set stderr verbosity:  0 quiet, 1 status, 2 debug\n");
}

char *tmpName = "wgetMd5.tmp";
char *md5tmp = "wgetMd5.md5";
int errCount = 0, maxErrs = 100;

static struct optionSpec options[] = {
   {"tmp", OPTION_STRING},
   {"maxErrs", OPTION_INT},
   {NULL, 0},
};

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
if ((err = system(command->string)) < 0)
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

void wgetMd5(char *uri, char *checkMd5, char *outDir)
/* wgetMd5 - Get files from ftp or http site and check their md5's. */
{
char source[PATH_LEN], dest[PATH_LEN];
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
    safef(dest, sizeof(dest), "%s/%s", outDir, relativePath);
    if (!fileExists(dest))
	{
	/* Create any directories needed. */
	splitPath(relativePath, dir, file, ext);
	dyStringClear(command);
	dyStringPrintf(command, "mkdir -p '%s/%s'", outDir, dir);
	system(command->string);

	/* wget the file. */
	safef(source, sizeof(source), "%s/%s", uri, relativePath);
	if (!safeGetOne(source, md5, dest))
	    if (++errCount > maxErrs)
	       errAbort("Aborting after %d errors", errCount);
	}
    else
        {
	verbose(1, "Already have %s\n", dest);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
tmpName = optionVal("tmp", tmpName);
md5tmp = optionVal("md5tmp", md5tmp);
maxErrs = optionInt("maxErrs", maxErrs);
wgetMd5(argv[1], argv[2], argv[3]);
return 0;
}
