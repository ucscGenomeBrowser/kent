/* hgDumpWiggle - dump wiggle data from database or .wig file */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "wiggle.h"
#include "hdb.h"


/* Command line switches. */
boolean noBin = FALSE;		/* do not expect a bin column in the table */
char *db = NULL;		/* database to read from */
char *file = NULL;		/* .wig file to read from */
char *chr = NULL;		/* work on this chromosome only */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"file", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"noBin", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgDumpWiggle - dump wiggle data from data base or .wig file\n"
  "usage:\n"
  "   hgDumpWiggle [options] [-db=<database> <track names ...>] [-file=<file.wig>]\n"
  "options:\n"
  "   -chr=chrN\twork on only chrN\n"
  "   -noBin\ttable has no bin column\n"
  "   Specify either a database with a track or a .wig file, not both\n"
  "   The -file option has not been implemented yet."
  );
}

void hgDumpWiggle(int trackCount, char *tracks[])
/* hgDumpWiggle - dump wiggle data from database or .wig file */
{
int i;
struct wiggle *wiggle;

for (i=0; i<trackCount; ++i)
    verbose(2, "#\ttrack: %s\n", tracks[i]);

if (db)
    {
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    char *wibFile = NULL;
    FILE *f = (FILE *) NULL;
    unsigned char *ReadData;    /* the bytes read in from the file */
    int dataOffset = 0;         /*      within data block during reading */
    int rowCount = 0;

    conn = sqlConnect(db);
    for (i=0; i<trackCount; ++i)
	{
	if (chr)
	    sqlSafef(query, 256, "select * from %s where chrom = \"%s\"\n", tracks[i], chr);
	else
	    sqlSafef(query, 256, "select * from %s\n", tracks[i]);
	verbose(2, "#\t%s\n", query);
	sr = sqlGetResult(conn,query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    ++rowCount;
	    wiggle = wiggleLoad(row + 1);  /* the +1 avoids the bin column*/
	    verbose(2, "#\trow: %d, start: %u, data range: %g: [%g:%g]\n", rowCount, wiggle->chromStart, wiggle->dataRange, wiggle->lowerLimit, wiggle->lowerLimit+wiggle->dataRange);
	    verbose(2, "#\tresolution: %g per bin\n",wiggle->dataRange/(double)MAX_WIG_VALUE);
	    if (wibFile)
		{
		if (differentString(wibFile,wiggle->file))
		    {
		    if (f != (FILE *) NULL)
			{
			fclose(f);
			freeMem(wibFile);
			}
		    wibFile = cloneString(wiggle->file);
		    f = mustOpen(wibFile, "r");
		    }
		}
	    else
		{
		wibFile = cloneString(wiggle->file);
		f = mustOpen(wibFile, "r");
		}
	    fseek(f, wiggle->offset, SEEK_SET);
	    ReadData = (unsigned char *) needMem((size_t) (wiggle->count + 1));
	    fread(ReadData, (size_t) wiggle->count,
		(size_t) sizeof(unsigned char), f);
	    verbose(2, "#\trow: %d, reading: %u bytes\n", rowCount, wiggle->count);
	    for (dataOffset = 0; dataOffset < wiggle->count; ++dataOffset)
                {
                unsigned char datum = ReadData[dataOffset];
                if (datum != WIG_NO_DATA)
		    {
		    double dataValue =
  wiggle->lowerLimit+(((double)datum/(double)MAX_WIG_VALUE)*wiggle->dataRange);
		    printf("%d\t%g\n",
	1 + wiggle->chromStart + (dataOffset * wiggle->span), dataValue);
		    }
		}
	    }
	}
    if (f != (FILE *) NULL)
	{
	fclose(f);
	}
    if (wibFile)
	freeMem(wibFile);

    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
else
    {
    warn("ERROR: file option has not been implemented yet ...");
    }
}	/*	void hgDumpWiggle(int trackCount, char *tracks[])	*/


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);


db = optionVal("db", NULL);
file = optionVal("file", NULL);
chr = optionVal("chr", NULL);
noBin = optionExists("noBin");
if (db)
    verbose(2, "#\tdatabase: %s\n", db);
if (file)
    verbose(2, "#\t.wig file: %s\n", file);
if (chr)
    verbose(2, "#\tselect chrom: %s\n", chr);
if (noBin)
    verbose(2, "#\texpect no bin column in table\n");
if (db && file)
    {
    warn("ERROR: specify only one of db or file, not both");
    usage();
    }
if (!(db || file))
    {
    warn("ERROR: must specify one of db or file");
    usage();
    }
hgDumpWiggle(argc-1, argv+1);
return 0;
}
