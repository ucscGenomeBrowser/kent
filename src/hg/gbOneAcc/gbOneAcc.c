/* This program retrieves one (or actually a few) records from a GenBank
 * file. */
#include "common.h"
#include "linefile.h"

static char const rcsid[] = "$Id: gbOneAcc.c,v 1.2 2003/05/06 07:22:17 kate Exp $";


void usage()
/* Explain usage and exit. */
{
errAbort(
"gbOneAcc - retrieve one or a few records from a GenBank flat file.\n"
"usage:\n"
"  gbOneAcc gbFile acc(s)\n"
"The output will be printed to standard out");
}

void unexpectedEof(struct lineFile *lf)
/* Complain about unexpected EOF and die. */
{
errAbort("Unexpected end of file line %d of %s", lf->lineIx, lf->fileName);
}

void extractAccFromGb( char *gbName, char **accNames, int accCount)
/* Parse records of genBank file and print ones that match accession names. */
{
struct lineFile *lf = lineFileOpen(gbName, TRUE);
FILE *out = stdout;
enum {maxHeadLines=200, headLineSize=256 };
char *headLines[maxHeadLines];	/* Store stuff between locus and accession. */
char *line;
int lineSize;
int i;

for (i=0; i<maxHeadLines; ++i)
    headLines[i] = needMem(headLineSize);

for (;;)
    {
    boolean gotAcc = FALSE;
    boolean gotMyAcc = FALSE;
    int headLineCount = 0;
    /* Seek to LOCUS */
    for (;;)
	{
	if (!lineFileNext(lf, &line, &lineSize))
	    return;
	if (startsWith("LOCUS", line))
	    break;
	}
    for (i=0; i<maxHeadLines; ++i)
	{
	++headLineCount;
	if (lineSize >= headLineSize)
	    errAbort("Line too long (%d chars) line %d of %s", lineSize, lf->lineIx, lf->fileName);
	strcpy(headLines[i], line);
	if (!lineFileNext(lf, &line, &lineSize))
	    unexpectedEof(lf);
	if (startsWith("ACCESSION", line))
	    {
	    gotAcc = TRUE;
	    break;
	    }
	}
    if (!gotAcc)
	{
	errAbort("LOCUS without ACCESSION in %d lines at line %d of %s",
	    maxHeadLines, lf->lineIx, lf->fileName);
	}
    for (i=0; i<accCount; ++i)
	{
	if (strstr(line, accNames[i]))
	    {
	    gotMyAcc = TRUE;
	    break;
	    }
	}
    if (gotMyAcc)
	{
	for (i=0; i<headLineCount; ++i)
	    {
	    fputs(headLines[i], out);
	    fputc('\n', out);
	    }
	fputs(line, out);
	fputc('\n', out);
	}
    for (;;)
	{
	if (!lineFileNext(lf, &line, &lineSize))
	    unexpectedEof(lf);
	if (gotMyAcc)
	    {
	    fputs(line, out);
	    fputc('\n', out);
	    }
	if (startsWith("//", line))
	    break;
	}
    }
}

int main(int argc, char *argv[])
{
if (argc < 3)
    usage();
extractAccFromGb(argv[1], argv+2, argc-2);
return 0;
}
