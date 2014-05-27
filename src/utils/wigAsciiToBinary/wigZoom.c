/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* wigZoom - process two column wiggle data and compress for zoom views
 *
 *	The ascii data file is simply two columns, white space separated:
 *	first column: offset within chromosome, 1 relative closed end
 *	second column: any real data value
 *
 *	Input arguments will be files to read and amount of points to
 *	put together for a zoomed view
 *
 */
#include	"common.h"
#include	"options.h"
#include	"linefile.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"dataSpan", OPTION_LONG_LONG},
    {"obsolete", OPTION_BOOLEAN},
    {NULL, 0}
};

static long long dataSpan = 1024;	/* bases spanned per data point */
static boolean obsolete = FALSE;/*use this program even though it is obsolete*/

static void usage()
{
errAbort(
    "wigZoom - process wiggle data to a zoomed view\n"
    "usage: wigZoom [-verbose=2] [-dataSpan=N] <file names>\n"
    "\t-dataSpan=N - # of bases spanned for each data point, default 1024\n"
    "\t-obsolete - Use this program even though it is obsolete.\n"
    "\t-verbose=2 - display process while underway\n"
    "\t<file names> - list of files to process\n"
    "Each ascii file is a two column file.  Whitespace separator\n"
    "First column of data is a chromosome location (IN NUMERICAL ORDER !).\n"
    "Second column is data value for that location, any real data value allowed."
);
}

/*	each individual data item from input	*/
struct dataPoint
    {
    unsigned long long offset;
    double value;
    };

/*	array of data points from input	up to dataSpan amount */
struct dataPoint *dataBlock;

/*	block is full of data, process it down to one number.
 *	In this case we are doing the Mean.  Future options will
 *	include Minimum and Maximum
 */
static void processBlock(unsigned long long beginWindow,
    struct dataPoint *dataBlock, int dataCount)
{
double sum = 0.0;
int i;

if (dataCount < 1)
	return;

for (i = 0; i < dataCount; ++i)
	sum += dataBlock[i].value;

printf ("%llu\t%g\n", beginWindow, sum/dataCount);

}

static void wigZoom( int argc, char *argv[] )
{
int i = 0;				/*	loop counter	*/
int lineCount = 0;			/*	lines from input file */
int validLines = 0;			/*	lines with actual data */
struct lineFile *lf;			/* for line file utilities	*/
unsigned long long beginWindow = 0;	/* from data input	*/
unsigned long long Offset = 0;		/* from data input	*/
unsigned long long previousOffset = 0;	/* for data missing detection */
char *line = (char *) NULL;		/* to receive data input line	*/
char *words[2];				/* to split data input line	*/
int dataCount = 0;

dataBlock = (struct dataPoint *)
	needMem( (size_t) (dataSpan * sizeof(struct dataPoint)));

/*	for each input data file	*/
for (i = 1; i < argc; ++i)
    {
    verbose(2, "translating file: %s\n", argv[i]);
    lineCount = 0;
    validLines = 0;
    dataCount = 0;
    lf = lineFileOpen(argv[i], TRUE);	/*	input file	*/
    beginWindow = 1;			/* input coords are 1 relative */
    while (lineFileNext(lf, &line, NULL))
	{
	int wordCount;
	char *val;
	char *valEnd;
	double dataValue;

	++lineCount;
	chopPrefixAt(line, '#'); /* ignore any comments starting with # */
	if (strlen(line) < 3)	/*	anything left on this line */
	    continue;		/*	no, go to next line	*/

	++validLines;
	wordCount = chopByWhite(line, words, 2);
	if (wordCount < 2)
	    errAbort("ERROR: Expecting at least two words at line %d, found %d",
		lineCount, wordCount);
	Offset = atoll(words[0]);
	if (Offset < previousOffset)
	    errAbort("ERROR: chrom positions not in order. previous: %llu is > current: %llu", previousOffset, Offset);
	val = words[1];
	dataValue = strtod(val, &valEnd);
	if ((*val == '\0') || (*valEnd != '\0'))
	    errAbort("Not a valid float at line %d: %s\n", lineCount, words[1]);
	if (Offset < 1)
	    errAbort("Illegal offset: %llu at line %d, dataValue: %g", Offset, 
		    lineCount, dataValue);
	if (Offset > (beginWindow + dataSpan))
	    {
		processBlock(beginWindow, dataBlock, dataCount);
		while ((beginWindow + dataSpan) < Offset)
			beginWindow += dataSpan;
		dataCount = 0;
	    }
	dataBlock[dataCount].offset = Offset;
	dataBlock[dataCount++].value = dataValue;
	previousOffset = Offset;
	}
    }
}

int main( int argc, char *argv[] )
{
optionInit(&argc, argv, optionSpecs);

obsolete = optionExists("obsolete");
if (! obsolete)
    {
    verbose(1,"ERROR: This loader is obsolete.  Please use: 'wigEncode'\n");
    verbose(1,"\t(use -obsolete flag to run this encoder despite it being obsolete)\n");
    errAbort("ERROR: wigZoom is obsolete, use 'wigEncode' instead");
    }

if (argc < 2)
    usage();

dataSpan = optionLongLong("dataSpan", 1024);

verbose(2, "options: -verbose, dataSpan= %llu\n", dataSpan);
if (dataSpan < 2)
    errAbort("ERROR: data span: %llu ! must be greater than one\n");

wigZoom(argc, argv);
exit(0);
}
