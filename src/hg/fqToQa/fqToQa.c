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
 "fqToQa - convert from fq format with one big file to\n"
 "format with one file per clone.\n"
 "usage:\n"
 "    fqToQa infile.fq outDir qaInfo\n"
 "This will put the quality scores from infile.fq into a series of\n"
 ".qa files in outdir, one file per clone.  Info from the '>' lines\n"
 "from the .fq file will be stored in the qaInfo file");
}

void faRecNameToQaFileName(char *outDir, char *ucscName, char *path)
/* Convert fa record name to qa file name. */
{
char *dup = cloneString(ucscName);
chopSuffix(dup);
sprintf(path, "%s/%s.qa", outDir, dup);
freeMem(dup);
}

void fqToQa(char *inFile, char *outDir, char *outTabName)
/* fqToQa - convert from fq format with one big file to
 * format with one file per clone. */
{
struct lineFile *in;
FILE *out = NULL, *tab;
int lineSize;
char *line;
char ucscName[128];
char path[512];
static char lastPath[512];
int outFileCount = 0;
struct hash *uniqClone = newHash(16);
struct hash *uniqFrag = newHash(19);
boolean ignore = FALSE;

makeDir(outDir);
errLog = mustOpen("fqToQa.err", "w");
pushWarnHandler(warnHandler);
tab = mustOpen(outTabName, "w");
printf("Converting %s", inFile);
fflush(stdout);
in = lineFileOpen(inFile, TRUE);
while (lineFileNext(in, &line, &lineSize))
    {
    if (line[0] == '>')
	{
	ignore = FALSE;
	gsToUcsc(line+1, ucscName);
	faRecNameToQaFileName(outDir, ucscName, path);
	if (hashLookup(uniqFrag, ucscName))
	    {
	    ignore = TRUE;
	    warn("Duplicate %s in %s, ignoring all but first",
	    	ucscName, inFile);
	    }
	else
	    {
	    hashAdd(uniqFrag, ucscName, NULL);
	    }
	if (!sameString(path, lastPath))
	    {
	    strcpy(lastPath, path);
	    carefulClose(&out);
	    if (hashLookup(uniqClone, path))
		{
		warn("Duplicate %s in %s ignoring all but first", 
		    ucscName, inFile);
		}
	    else
		{
		hashAdd(uniqClone, path, NULL);
		out = mustOpen(path, "w");
		++outFileCount;
		if ((outFileCount&7) == 0)
		    {
		    putc('.', stdout);
		    fflush(stdout);
		    }
		}
	    }
	if (out != NULL && !ignore)
	    {
	    fprintf(out, ">%s\n", ucscName);
	    fprintf(tab, "%s\t%s\n", ucscName, line+1);
	    }
	}
    else
	{
	if (out != NULL && !ignore)
	    {
	    fputs(line, out);
	    fputc('\n', out);
	    }
	}
    }
carefulClose(&out);
fclose(tab);
lineFileClose(&in);
printf("Made %d .qa files in %s\n", outFileCount, outDir);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
fqToQa(argv[1], argv[2], argv[3]);
return 0;
}
