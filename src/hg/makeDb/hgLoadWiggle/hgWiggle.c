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

static char const rcsid[] = "$Id: hgWiggle.c,v 1.2 2004/07/27 23:43:28 hiram Exp $";

/* Command line switches. */
static char *db = NULL;		/* database to read from */
static char *chr = NULL;		/* work on this chromosome only */

/*	global flags	*/
static boolean file = FALSE;	/*	no database specified, use file */
static struct lineFile *wigFile = NULL;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"chr", OPTION_STRING},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgWiggle - fetch wiggle data from data base or file\n"
  "usage:\n"
  "   hgWiggle [options] [-db=<database>] <track names ...>\n"
  "options:\n"
  "   -chr=chrN\texamine data only on chrN\n"
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
struct sqlConnection *conn = hAllocConn();
int i;
struct wiggle *wiggle;
char *wibFile = NULL;
struct sqlResult *sr;

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
    char query[256];
    FILE *f = (FILE *) NULL;
    unsigned char *ReadData;    /* the bytes read in from the file */
    int dataOffset = 0;         /*      within data block during reading */
    int rowCount = 0;
    unsigned int currentSpan = 0;
    char *currentChrom = NULL;


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
	if (chr)
	    snprintf(query, 256, "select * from %s where chrom = \"%s\"\n",
		tracks[i], chr);
	else
	    snprintf(query, 256, "select * from %s\n", tracks[i]);
	verbose(2, "#\t%s\n", query);
	sr = sqlGetResult(conn,query);
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
	    }

	if ( (currentSpan != wiggle->span) || 
		(currentChrom && differentString(currentChrom, wiggle->chrom)))
	    {
	    freeMem(currentChrom);
	    currentChrom=cloneString(wiggle->chrom);
	    printf ("variableStep chrom=%s", currentChrom);
	    currentSpan = wiggle->span;
	    if (currentSpan > 1)
		printf (" span=%d\n", currentSpan);
	    else
		printf ("\n");
	    }

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
	wiggleFree(&wiggle);
	}
    }	/*	for (i=0; i<trackCount; ++i)	*/

if (file)
    lineFileClose(&wigFile);
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
if (db)
    verbose(2, "#\tdatabase: %s\n", db);
else
    {
    file = TRUE;
    verbose(2, "#\tno database specified, using .wig files\n");
    }
if (chr)
    verbose(2, "#\tselect chrom: %s\n", chr);

if (argc < 2)
    usage();

hgWiggle(argc-1, argv+1);
return 0;
}

