/* fqToQa - convert from fq format with one big file to
 * format with one file per clone. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errAbort.h"
#include "portable.h"
#include "hash.h"
#include "linefile.h"
#include "hCommon.h"
#include "qaSeq.h"


FILE *errLog;

void warnHandler(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) 
    {
    vfprintf(stderr, format, args);
    vfprintf(errLog, format, args);
    fprintf(stderr, "\n");
    fprintf(errLog, "\n");
    }
}


void usage()
/* Explain usage and exit. */
{
errAbort(
 "fqToQac - convert from fq format with one big file to\n"
 "compressed format with one file per clone.\n"
 "usage:\n"
 "    fqToQac [infile.fq] outDir\n"
 "This will put the quality scores from infile.fq into a series of\n"
 ".qac files in outdir, one file per clone.  The .qac files can be\n"
 "uncompressed with qacToQa.");
}

void fqToQa(struct lineFile *in, char *outDir)
/* fqToQa - convert from fq format with one big file to
 * format with one file per clone. */
{
FILE *out = NULL;
char ucscName[128];
char path[512];
static char lastPath[512];
char *gsName;
int outFileCount = 0;
struct hash *uniqClone = newHash(16);
struct hash *uniqFrag = newHash(19);
// boolean ignore = FALSE;  unused
struct qaSeq qa;

makeDir(outDir);
errLog = mustOpen("fqToQa.err", "w");
pushWarnHandler(warnHandler);
printf("Converting %s", in->fileName);
fflush(stdout);

while (qaFastReadNext(in, &qa.qa, &qa.size, &gsName))
    {
    gsToUcsc(gsName, ucscName);
    qa.name = ucscName;
    if (hashLookup(uniqFrag, ucscName))
	{
//	ignore = TRUE;  unused
	warn("Duplicate %s in %s, ignoring all but first",
	    ucscName, in->fileName);
        continue;
	}
    else
	{
	hashAdd(uniqFrag, ucscName, NULL);
	}
    faRecNameToQacFileName(outDir, ucscName, path);
    if (!sameString(path, lastPath))
	{
	strcpy(lastPath, path);
	carefulClose(&out);
	if (hashLookup(uniqClone, path))
	    {
	    warn("Duplicate %s in %s ignoring all but first", 
		ucscName, in->fileName);
	    }
	else
	    {
	    hashAdd(uniqClone, path, NULL);
	    out = mustOpen(path, "w");
	    qacWriteHead(out);
	    ++outFileCount;
	    if ((outFileCount&7) == 0)
		{
		putc('.', stdout);
		fflush(stdout);
		}
	    }
	}
    if (out != NULL)
	qacWriteNext(out, &qa);
    }
carefulClose(&out);
printf("Made %d .qac files in %s\n", outFileCount, outDir);
}


int main(int argc, char *argv[])
/* Process command line. */
{
struct lineFile *lf = NULL;
char *dir = NULL;

if (argc == 2)
    {
    lf = lineFileStdin(TRUE);
    dir = argv[1];
    }
else if (argc == 3)
    {
    lf = lineFileOpen(argv[1], TRUE);
    dir = argv[2];
    }
else
    usage();
fqToQa(lf, dir);
return 0;
}
