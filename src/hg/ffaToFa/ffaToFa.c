/* ffaToFa - convert Greg Schulers .ffa fasta files to our .fa files */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
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
   "ffaToFa convert Greg Schuler .ffa fasta files to UCSC .fa fasta files\n"
   "usage:\n"
   "    ffaToFa file.ffa faDir trans\n"
   "where ffaDir is directory full of .ffa files, faDir is where you want\n"
   "to put the corresponding .fa files, trans is a table that\n"
   "translates from one name to the other and cloneSizes is a file\n"
   "that lists the size of each clone.\n"
   "If you put 'stdin' for file.ffa, it will read from standard input.\n");
}

void ffaToFa(char *inFile, char *outDir, char *outTabName)
/* convert Greg Schulers .ffa fasta files to our .fa files */
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
errLog = mustOpen("ffaToFa.err", "w");
tab = mustOpen(outTabName, "w");
printf("Converting %s", inFile);
fflush(stdout);
if (sameString(inFile, "stdin"))
    in = lineFileStdin(TRUE);
else
    in = lineFileOpen(inFile, TRUE);
while (lineFileNext(in, &line, &lineSize))
    {
    if (line[0] == '>')
	{
	ignore = FALSE;
	gsToUcsc(line+1, ucscName);
	faRecNameToFaFileName(outDir, ucscName, path);
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
printf("Made %d .fa files in %s\n", outFileCount, outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
ffaToFa(argv[1], argv[2], argv[3]);
return 0;
}
