/* hgDumpWiggle - dump wiggle data from database or .wig file */
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

static char const rcsid[] = "$Id: hgDumpWiggle.c,v 1.2 2004/02/16 02:17:47 kent Exp $";

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
  "   Specify either a database with a track or a .wig file, not both"
  );
}

void hgDumpWiggle(int trackCount, char *tracks[])
/* hgDumpWiggle - dump wiggle data from database or .wig file */
{
int i;
struct wiggle *wiggle;

for (i=0; i<trackCount; ++i)
    logPrintf(2, "track: %s\n", tracks[i]);

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
	    snprintf(query, 256, "select * from %s where chrom = \"%s\"\n", tracks[i], chr);
	else
	    snprintf(query, 256, "select * from %s\n", tracks[i]);
	logPrintf(2, "%s\n", query);
	sr = sqlGetResult(conn,query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    ++rowCount;
	    wiggle = wiggleLoad(row + 1);  /* the +1 avoids the bin column*/
	    logPrintf(2, "row: %d, start: %u, data range: %g: [%g:%g]\n", rowCount, wiggle->chromStart, wiggle->dataRange, wiggle->lowerLimit, wiggle->lowerLimit+wiggle->dataRange);
	    logPrintf(2, "\tresolution: %g per bin\n",wiggle->dataRange/(double)MAX_WIG_VALUE);
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
	    logPrintf(2, "row: %d, reading: %u bytes\n", rowCount, wiggle->count);
	    for (dataOffset = 0; dataOffset < wiggle->count; ++dataOffset)
                {
                unsigned char datum = ReadData[dataOffset];
                if (datum != WIG_NO_DATA)
		    {
		    double dataValue =
  wiggle->lowerLimit+(((double)datum/(double)MAX_WIG_VALUE)*wiggle->dataRange);
		    logPrintf(1, "%d\t%g\n",
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
    warn("ERROR: file option has not been implemented yet ...\n");
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
    logPrintf(2, "database: %s\n", db);
if (file)
    logPrintf(2, ".wig file: %s\n", file);
if (chr)
    logPrintf(2, "select chrom: %s\n", chr);
if (noBin)
    logPrintf(2, "expect no bin column in table\n");
if (db && file)
    {
    warn("ERROR: specify only one of db or file, not both\n");
    usage();
    }
if (!(db || file))
    {
    warn("ERROR: must specify one of db or file\n");
    usage();
    }
hgDumpWiggle(argc-1, argv+1);
return 0;
}
