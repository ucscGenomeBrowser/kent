/* hgWiggle - fetch wiggle data from database or .wig file */
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
#include "portable.h"

static char const rcsid[] = "$Id: hgWiggle.c,v 1.3 2004/07/28 19:17:37 hiram Exp $";

/* Command line switches. */
static char *db = NULL;		/* database to read from */
static char *chr = NULL;		/* work on this chromosome only */
static boolean silent = FALSE;	/*	turn timing on	*/
static boolean timing = FALSE;	/*	turn timing on	*/
static unsigned span = 0;	/*	select for this span only	*/

/*	global flags	*/
static boolean file = FALSE;	/*	no database specified, use file */
static struct lineFile *wigFile = NULL;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"silent", OPTION_BOOLEAN},
    {"timing", OPTION_BOOLEAN},
    {"span", OPTION_INT},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgWiggle - fetch wiggle data from data base or file\n"
  "usage:\n"
  "   hgWiggle [options] <track names ...>\n"
  "options:\n"
  "   -db=<database> - use specified database\n"
  "   -chr=chrN - examine data only on chrN\n"
  "   -silent - no output, scanning data only\n"
  "   -timing - display timing statistics\n"
  "   When no database is specified, track names will refer to .wig files\n"
  "   example using the file gc5Base.wig:\n"
  "                hgWiggle -chr=chrM gc5Base\n"
  "   example using the database table hg17.gc5Base:\n"
  "                hgWiggle -chr=chrM -db=hg17 gc5Base"
  );
}

static boolean wigNextRow(struct sqlResult *sr, struct lineFile *lf,
	char *row[], int maxRow)
/*	read next wig row from sql query or lineFile	*/
{
int numCols;

if (file)
    {
    numCols = lineFileChopNextTab(wigFile, row, maxRow);
    verbose(3, "#\tnumCols = %d, row[0]: %s, row[1]: %s, row[%d]: %s\n",
	numCols, row[0], row[1], maxRow-1, row[maxRow-1]);
    if (numCols != maxRow) return FALSE;
    }
else
    {
    int i;
    char **sqlRow;
    sqlRow = sqlNextRow(sr);
    if (sqlRow == NULL)
	return FALSE;
    /*	skip the bin column sqlRow[0]	*/
    for (i=1; i <= maxRow; ++i)
	{
	row[i-1] = sqlRow[i];
	}
    }
return TRUE;
}

void hgWiggle(int trackCount, char *tracks[])
/* hgWiggle - dump wiggle data from database or .wig file */
{
struct sqlConnection *conn;
int i;
struct wiggle *wiggle;
char *wibFile = NULL;
struct sqlResult *sr;
unsigned long fileBytes = 0;
unsigned long fileNoDataBytes = 0;
long fileEt = 0;


verbose(2, "#\texamining tracks:");
for (i=0; i<trackCount; ++i)
    verbose(2, " %s", tracks[i]);
verbose(2, "\n");

if (db)
    {
    conn = hAllocConn();
    conn = sqlConnect(db);
    }

for (i=0; i<trackCount; ++i)
    {
    char *row[WIGGLE_NUM_COLS];
    FILE *f = (FILE *) NULL;
    unsigned char *ReadData;    /* the bytes read in from the file */
    int dataOffset = 0;         /*      within data block during reading */
    int rowCount = 0;
    unsigned int currentSpan = 0;
    char *currentChrom = NULL;
    long startClock = clock1000();
    long endClock;
    long chrStartClock = clock1000();
    long chrEndClock;
    unsigned long bytesRead = 0;
    unsigned long chrBytesRead = 0;
    unsigned long noDataBytes = 0;
    unsigned long chrNoDataBytes = 0;

    if (file)
	{
	struct dyString *fileName = dyStringNew(256);
	lineFileClose(&wigFile);	/*	possibly a previous file */
	dyStringPrintf(fileName, "%s.wig", tracks[i]);
	wigFile = lineFileOpen(fileName->string, TRUE);
	dyStringFree(&fileName);
	}
    else
	{
	struct dyString *query = dyStringNew(256);
	dyStringPrintf(query, "select * from %s", tracks[i]);
	if ((chr != NULL) || (span != 0))
	    {
	    dyStringPrintf(query, " where (");
	    if (chr != NULL)
		dyStringPrintf(query, "chrom = \"%s\"", chr);
	    if ((chr != NULL) && (span != 0))
		dyStringPrintf(query, " AND ");
	    if (span != 0)
		dyStringPrintf(query, "span = \"%d\"", span);
	    dyStringPrintf(query, ")");
	    }
	verbose(2, "#\t%s\n", query->string);
	sr = sqlGetResult(conn,query->string);
	}
    while (wigNextRow(sr, wigFile, row, WIGGLE_NUM_COLS))
	{
	++rowCount;
	wiggle = wiggleLoad(row);
	if (file)
	    {
	    if (chr)
		if (differentString(chr,wiggle->chrom))
		    continue;
	    if (span)
		if (span != wiggle->span)
		    continue;
	    }

	if ( (currentSpan != wiggle->span) || 
		(currentChrom && differentString(currentChrom, wiggle->chrom)))
	    {
	    if (currentChrom && differentString(currentChrom, wiggle->chrom))
		{
		long et;
		chrEndClock = clock1000();
		et = chrEndClock - chrStartClock;
		if (timing)
 verbose(1,"#\t%s.%s %lu data bytes, %lu no-data bytes, ET: %ld ms\n",
		    tracks[i], currentChrom, chrBytesRead, chrNoDataBytes, et);
    		chrNoDataBytes = chrBytesRead = 0;
		chrStartClock = clock1000();
		}
	    freeMem(currentChrom);
	    currentChrom=cloneString(wiggle->chrom);
	    if (!silent)
		printf ("variableStep chrom=%s", currentChrom);
	    currentSpan = wiggle->span;
	    if (!silent && (currentSpan > 1))
		printf (" span=%d\n", currentSpan);
	    else if (!silent)
		printf ("\n");
	    }

	verbose(2, "#\trow: %d, start: %u, data range: %g: [%g:%g]\n",
		rowCount, wiggle->chromStart, wiggle->dataRange,
		wiggle->lowerLimit, wiggle->lowerLimit+wiggle->dataRange);
	verbose(2, "#\tresolution: %g per bin\n",
		wiggle->dataRange/(double)MAX_WIG_VALUE);
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
    		++bytesRead;
    		++chrBytesRead;
		if (!silent)
		    printf("%d\t%g\n",
			1 + wiggle->chromStart + (dataOffset * wiggle->span),
				dataValue);
		}
	    else
		{
		++noDataBytes;
		++chrNoDataBytes;
		}
	    }
	wiggleFree(&wiggle);
	}
    endClock = clock1000();
    if (timing)
	{
	long et;
	chrEndClock = clock1000();
	et = chrEndClock - chrStartClock;
	if (!file)
 verbose(1,"#\t%s.%s %lu data bytes, %lu no-data bytes, ET: %ld ms\n",
		    tracks[i], currentChrom, chrBytesRead, chrNoDataBytes, et);
    		chrNoDataBytes = chrBytesRead = 0;
		chrStartClock = clock1000();
	et = endClock - startClock;
	fileEt += et;
 verbose(1,"#\t%s %lu data bytes, %lu no-data bytes, ET: %ld ms\n",
		tracks[i], bytesRead, noDataBytes, et);
	}
    fileBytes += bytesRead;
    fileNoDataBytes += noDataBytes;
    }	/*	for (i=0; i<trackCount; ++i)	*/

if (file)
    {
    lineFileClose(&wigFile);
    if (timing)
 verbose(1,"#\ttotal %lu data bytes, %lu no-data bytes, ET: %ld ms\n",
		fileBytes, fileNoDataBytes, fileEt);
    }
if (wibFile)
    freeMem(wibFile);

if (db)
    {
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
}	/*	void hgWiggle(int trackCount, char *tracks[])	*/


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

db = optionVal("db", NULL);
chr = optionVal("chr", NULL);
silent = optionExists("silent");
timing = optionExists("timing");
span = optionInt("span", 0);

if (db)
    verbose(2, "#\tdatabase: %s\n", db);
else
    {
    file = TRUE;
    verbose(2, "#\tno database specified, using .wig files\n");
    }
if (chr)
    verbose(2, "#\tselect chrom: %s\n", chr);
if (silent)
    verbose(2, "#\tsilent option on\n");
if (timing)
    verbose(2, "#\ttiming option on\n");
if (span)
    verbose(2, "#\tselect for span=%u only\n", span);

if (argc < 2)
    usage();

hgWiggle(argc-1, argv+1);
return 0;
}

