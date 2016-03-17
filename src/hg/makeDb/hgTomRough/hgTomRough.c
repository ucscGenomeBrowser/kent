/* hgTomRough - Position roughly located genes. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "cytoBand.h"
#include "chromInfo.h"
#include "tomRough.h"
#include "htmshell.h"


struct chromInfo *chromList = NULL;
struct cytoBand *bandList = NULL;
struct tomRough *roughList = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTomRough - Position roughly located genes\n"
  "usage:\n"
  "   hgTomRough database in.comma out.comma\n");
}


struct chromInfo *loadAllChromInfo(char *database)
/* Load up all chromosome infos. */
{
struct chromInfo *list = NULL, *el;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
char **row;

sr = sqlGetResult(conn, NOSQLINJ "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
sqlDisconnect(&conn);
printf("Loaded %d chromosomes\n", slCount(list));
return list;
}


struct cytoBand *loadAllBands(char *database)
/* Load up all bands from database. */
{
struct cytoBand *list = NULL, *el;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
char **row;

sr = sqlGetResult(conn, NOSQLINJ "select * from cytoBand");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = cytoBandLoad(row);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
sqlDisconnect(&conn);
printf("Loaded %d bands\n", slCount(list));
return list;
}

struct tomRough *loadAllRough(char *fileName)
/* Load up all bands from database. */
{
struct tomRough *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[16], *line;
int wordCount, lineSize;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopCommas(line, words);
    lineFileExpectWords(lf, 5, wordCount);
    el = tomRoughLoad(words);
    slAddHead(&list, el);
    }
slReverse(&list);
lineFileClose(&lf);
printf("Loaded %d rough lines\n", slCount(list));
return list;
}

int chromosomeSize(char *chromosome)
/* Return full extents of chromosome.  Warn and fill in if none. */
{
struct chromInfo *ci;

for (ci = chromList; ci != NULL; ci = ci->next)
    {
    if (sameString(chromosome, ci->chrom))
        {
	return ci->size;
	}
    }
errAbort("Couldn't find size of chromosome %s", chromosome);
return 500000000;
}


void bandExtents(char *chromosome, char *band, int *retStart, int *retEnd)
/* Return start/end of band in chromosome. */
{
struct cytoBand *chrStart = NULL, *chrEnd = NULL, *cb;
int start = 0, end = 500000000;
boolean anyMatch;
char choppedBand[64], *s, *e;

/* Find first band in chromosome. */
for (cb = bandList; cb != NULL; cb = cb->next)
    {
    if (sameString(cb->chrom, chromosome))
        {
	chrStart = cb;
	break;
	}
    }
if (chrStart == NULL)
    errAbort("Couldn't find chromosome %s in band list", chromosome);

/* Find last band in chromosome. */
for (cb = chrStart->next; cb != NULL; cb = cb->next)
    {
    if (!sameString(cb->chrom, chromosome))
        break;
    }
chrEnd = cb;

// uglyf("first band %s, last %s\n", chrStart->name, (chrEnd == NULL ? "n/a" : chrEnd->name));

if (sameWord(band, "cen"))
    {
    for (cb = chrStart; cb != chrEnd; cb = cb->next)
        {
	if (cb->name[0] == 'p')
	    start = cb->chromEnd - 500000;
	else if (cb->name[0] == 'q')
	    {
	    end = cb->chromStart + 500000;
	    break;
	    }
	}
    *retStart = start;
    *retEnd = end;
    return;
    }
else if (sameWord(band, "qter"))
    {
    *retStart = *retEnd = chromosomeSize(chromosome);
    *retStart -= 1000000;
    return;
    }
/* Look first for exact match. */
for (cb = chrStart; cb != chrEnd; cb = cb->next)
    {
    if (sameWord(cb->name, band))
        {
	*retStart = cb->chromStart;
	*retEnd = cb->chromEnd;
	return;
	}
    }

/* See if query is less specific.... */
strcpy(choppedBand, band);
for (;;) 
    {
    anyMatch = FALSE;
    for (cb = chrStart; cb != chrEnd; cb = cb->next)
	{
	if (startsWith(choppedBand, cb->name))
	    {
	    if (!anyMatch)
		{
		anyMatch = TRUE;
		start = cb->chromStart;
		}
	    end = cb->chromEnd;
	    }
	}
    if (anyMatch)
	{
	*retStart = start;
	*retEnd = end;
	return;
	}
    s = strrchr(choppedBand, '.');
    if (s == NULL)
	errAbort("Couldn't find anything like band '%s'", band);
    else
	{
	e = choppedBand + strlen(choppedBand) - 1;
	*e = 0;
	if (e[-1] == '.')
	   e[-1] = 0;
        warn("Band %s%s is at higher resolution than data, chopping to %s%s",
	    chromosome+3, band, chromosome+3, choppedBand);
	}
    }
}

void bandRange(char *chromosome, char *startBand, char *endBand, 
    int *retStart, int *retEnd)
{
int start, end, s, e;

if (startBand[0] == '-' || sameWord(startBand, "pter"))
    start = 0;
else
    {
    bandExtents(chromosome, startBand, &s, &e);
    start = s;
    }
if (endBand[0] == '-' || sameWord(endBand, "qter"))
    {
    end = chromosomeSize(chromosome);
    }
else
    {
    bandExtents(chromosome, endBand, &s, &e);
    end = e;
    }
*retStart = start;
*retEnd = end;
}

void makeHtml(char *fileName)
/* Make html page with links for Tom. */
{
struct tomRough *tr;
int start, end;
char chrom[256];
FILE *f = mustOpen(fileName, "w");

htmStart(f, "roughly mapped disease genes");
fprintf(f, "<PRE>");
for (tr = roughList; tr != NULL; tr = tr->next)
    {
    sprintf(chrom, "chr%s", tr->chromosome);
    bandRange(chrom, tr->startBand, tr->endBand, &start, &end);
    fprintf(f, 
       "<A HREF=\"http://www.ncbi.nlm.nih.gov/entrez/dispomim.cgi?id=%d\">",
       tr->omimId);
    fprintf(f, "OMIM %d</A>\t", tr->omimId);
    fprintf(f, 
        "<A HREF=\"http://genome.ucsc.edu/cgi-bin/hgTracks?position=%s:%d-%d&pix=800\">", chrom, start, end);
    fprintf(f, "%s:%d-%d</A>\t", chrom, start, end);
    fprintf(f, "%s,%s\t", tr->startBand, tr->endBand);
    fprintf(f, "%s\n", tr->description);
    }
htmEnd(f);
fclose(f);
}

void hgTomRough(char *database, char *inFile, char *outFile)
/* hgTomRough - Position roughly located genes. */
{
chromList = loadAllChromInfo(database);
bandList = loadAllBands(database);
roughList = loadAllRough(inFile);
makeHtml(outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
hgTomRough(argv[1], argv[2], argv[3]);
return 0;
}
