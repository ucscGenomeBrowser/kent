/* wgetMd5 - Get files from ftp or http site and check their md5's. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"

static char const rcsid[] = "$Id: wgetMd5.c,v 1.1 2005/11/18 04:20:00 kent Exp $";

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
  "NOTE: CURRENTLY DOES NOT CHECK MD5!\n");
}

char *tmpName = "wgetMd5.tmp";

static struct optionSpec options[] = {
   {NULL, 0},
};

void wgetMd5(char *uri, char *checkMd5, char *outDir)
/* wgetMd5 - Get files from ftp or http site and check their md5's. */
{
char dest[PATH_LEN];
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
	int err; 
	/* Create any directories needed. */
	splitPath(relativePath, dir, file, ext);
	dyStringClear(command);
	dyStringPrintf(command, "mkdir -p '%s/%s'", outDir, dir);
	system(command->string);

	/* wget the file. */
	dyStringClear(command);
	dyStringPrintf(command, "wget -nv --timestamping -O %s '%s/%s'", 
	    tmpName, uri, relativePath);
	verbose(1, "%s\n", command->string);
	if ((err = system(command->string)) < 0)
	    errAbort("Error %d on %s", err, command->string);

	/* Rename file to proper name */
	if ((err = rename(tmpName, dest)) < 0)
	    errnoAbort("Couldn't rename %s to %s", tmpName, dest);
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
wgetMd5(argv[1], argv[2], argv[3]);
return 0;
}
