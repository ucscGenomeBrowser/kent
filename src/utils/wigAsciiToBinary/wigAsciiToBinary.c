/* wigAsciiToBinary - convert ascii Wiggle data to binary file
 *	plus, produces file read for load into database
 *
 *	The ascii data file is simply two columns, white space separated:
 *	first column: offset within chromosome, 1 relative closed end
 *	second column: any real data value
 *
 *	Each data block will have its own specific scale computed
 *
 *	This first pass will assume the scores are specified for
 *	a single nucleotide base.  If the offset sequence has
 *	missing data in it, the missing data will be set to the "NO_DATA"
 *	indication.  (NO_DATA == 128)
 *
 *	The binary data file is simply one unsigned char value per byte.
 *	The suffix for this binary data file is: .wib - wiggle binary
 *
 *	The second file produced is a bed-like format file, with chrom,
 *	chromStart, chromEnd, etc.  See hg/inc/wiggle.h for structure
 *	definition.  The important thing about this file is that it will
 *	have the pointers into the binary data file and definitions of
 *	where the bytes belong.  This file extension is: .wig - wiggle
 *
 *	Holes in the data are allowed.  A command line argument can
 *	specify the actual span of a data point == how many nucleotide
 *	bases the value is representative of.
 *
 *	There is a concept of a "bin" size which means how many values
 *	should be maintained in a single table row entry.  This can be
 *	controlled by command line argument.  Holes in the input data
 *	will be filled with NO_DATA values up to the size of the current
 *	"bin".  If the missing data continues past the next "bin" size
 *	number of data points, that table row will be skipped.  Thus, it
 *	is only necessary to have table rows which contain valid data.
 *	The "bin" size is under control with a command line argument.
 */
#include	"common.h"
#include	"options.h"
#include	"linefile.h"

#define	NO_DATA	128
#define MAX_BYTE_VALUE	127

static char const rcsid[] = "$Id: wigAsciiToBinary.c,v 1.11 2003/11/07 00:47:27 hiram Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"offset", OPTION_LONG_LONG},
    {"binsize", OPTION_LONG_LONG},
    {"dataSpan", OPTION_LONG_LONG},
    {"chrom", OPTION_STRING},
    {"wibFile", OPTION_STRING},
    {"name", OPTION_STRING},
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};

/*	FEATURE REQUEST - allow specification of columns in data file
 *	where the chrom offset and data value can be found.  This would
 *	allow any type of column data file to work.  Also, it may be
 *	necessary in the future to allow a third column of data input
 *	which would specify the chromosome this data belongs to, or even
 *	a fourth column which names the data element, although these
 *	extra columns and subsequent processing would require quite a
 *	bit of work.  It is by far much easier to do pre-processing of
 *	mixed data like that to break it up into separate files for each
 *	chromosome and named feature.
 */
static long long add_offset = 0;	/* to allow "lifting" of the data */
static long long binsize = 1024;	/* # of data points per table row */
static long long dataSpan = 1;		/* bases spanned per data point */
static boolean verbose = FALSE;		/* describe what happens	*/
static char * chrom = (char *) NULL;	/* to specify chrom name	*/
static char * wibFile = (char *) NULL;	/* to name the .wib file	*/
static char * name = (char *) NULL;	/* to specify feature name	*/

static void usage()
{
errAbort(
    "wigAsciiToBinary - convert ascii Wiggle data to binary file\n"
    "usage: wigAsciiToBinary [-offset=N] [-binsize=N] [-dataSpan=N] \\\n"
    "\t[-chrom=chrN] [-wibFile=<file name>] [-name=<feature name>] \\\n"
    "\t[-verbose] <two column ascii data file names>\n"
    "\t-offset=N - add N to all coordinates, default 0\n"
    "\t-binsize=N - # of points per database row entry, default 1024\n"
    "\t-dataSpan=N - # of bases spanned for each data point, default 1\n"
    "\t-chrom=chrN - this data is for chrN\n"
    "\t-wibFile=chrN - to name the .wib output file\n"
    "\t-name=<feature name> - to name the feature, default chrN or -chrom specified\n"
    "\t-verbose - display process while underway\n"
    "\t<file names> - list of files to process\n"
    "If the name of the input files are of the form: chrN.<....> this will\n"
    "\tset the output file names.  Otherwise use the -wibFile option.\n"
    "Each ascii file is a two column file.  Whitespace separator\n"
    "First column of data is a chromosome location.\n"
    "Second column is data value for that location, range [0:127]\n"
);
}

static unsigned long long bincount = 0;	/* to count binsize for wig file */
static unsigned long long chromStart = 0;	/* for table row data */
static unsigned long long chromEnd = 0;	/* for table row data */
static FILE *wigout;	/*	file handle for wiggle database file	*/
static FILE *binout;	/*	file handle for binary data file	*/
static char *chromName = (char *) NULL;	/* pseudo bed-6 data to follow */
static char featureName[254];			/* the name of this feature */
static unsigned long long rowCount = 0;	/* to count rows output */
static char spanName[64];		/* short-hand name for dataSpan */
static char strand = '+';		/* may never use - strand ? */
static off_t fileOffsetBegin = 0;	/* where this bin started in binary */
static char *binfile = (char *) NULL;	/* file name of binary data file */
static unsigned long long Offset = 0;	/* from data input	*/
static off_t fileOffset = 0;		/* where are we in the binary data */
static double *data_values = (double *) NULL;
			/* all the data values read in for this one row */
static unsigned char * validData = (unsigned char *) NULL;
				/* array to keep track of what data is valid */
static double overallUpperLimit;	/* for the complete set of data */
static double overallLowerLimit;	/* for the complete set of data */

/*
 *	The table rows need to be printed in a couple of different
 *	places in the code.
 *	Definition of this row format can be found in hg/inc/wiggle.h
 *	and hg/lib/wiggle.as, etc ...
 *	Probably do not need the strand, but will
 *	it them around in case some good use can be found later.
 */
static void output_row()
{
double lowerLimit = 1.0e+300;
double upperLimit = -1.0e+300;
double dataRange = 0.0;
double sumData = 0.0;
double sumSquares = 0.0;
double stddev = 0.0;
double average = 0.0;
int maxScore = 0;		/* max score in this bin */
int minScore = 1000000;		/* min score in this bin */
unsigned long long i;
unsigned long long validCount = 0; /* number of valid data points */

if (bincount)
    {
    /*	scan the data and determine stats for it	*/
    for (i=0; i < bincount; ++i)
	{
	if (validData[i])
	    {
	    ++validCount;
	    if (data_values[i] > upperLimit) upperLimit = data_values[i];
	    if (data_values[i] < lowerLimit) lowerLimit = data_values[i];
	    sumData += data_values[i];
	    sumSquares += data_values[i] * data_values[i];
	    }
	}
    if (validCount < 1) {
	errAbort("ERROR: empty row being produced at row: %d\n", rowCount);
    } else if (validCount > 1) {
	stddev = sqrt(
	(sumSquares - ((sumData*sumData)/validCount))/(validCount-1) );
	average = sumData / validCount;
    } else {
	average = sumData;
	stddev = 0.0;
    }
    dataRange = upperLimit - lowerLimit;
    if (upperLimit > overallUpperLimit) overallUpperLimit = upperLimit;
    if (lowerLimit < overallLowerLimit) overallLowerLimit = lowerLimit;
    /*	With limits determined, can now scale and output the values
     *  compressed down to byte data values for the binary file
     */
    maxScore = 0;
    minScore = 1000000;
    for (i=0; i < bincount; ++i)
	{
	int byteOutput;	/* should be unsigned char, but fputc() wants int */
	if (validData[i])
	    {
		if (dataRange > 0.0 )
		    {
			byteOutput = MAX_BYTE_VALUE *
				((data_values[i] - lowerLimit)/dataRange);
		    } else {
			byteOutput = 0;
		    }
	    } else {
		byteOutput = NO_DATA;
	    }
	fputc(byteOutput,binout);	/*	output a data byte */
	if (byteOutput > maxScore )
	    maxScore = byteOutput;
	if (byteOutput < minScore )
	    minScore = byteOutput;
	}

    chromEnd = chromStart + (bincount * dataSpan);
    fprintf( wigout,
"%s\t%llu\t%llu\t%s.%llu_%s\t%d\t%c\t%d\t%llu\t%llu\t%llu\t%s\t%g\t%g\t%llu\t%g\t%g\n",
	chromName, chromStart+add_offset, chromEnd+add_offset,
	featureName, rowCount, spanName, maxScore, strand, minScore,
	dataSpan, bincount, fileOffsetBegin, binfile, lowerLimit,
	dataRange, validCount, average, stddev );
    ++rowCount;
    }
bincount = 0;	/* to count up to binsize	*/
chromStart = Offset + dataSpan;
fileOffsetBegin = fileOffset;

}	/*	static void output_row()	*/

void wigAsciiToBinary( int argc, char *argv[] )
{
int i = 0;				/* general purpose int counter	*/
struct lineFile *lf;			/* for line file utilities	*/
char * fileName;			/* the basename of the input file */
char *line = (char *) NULL;		/* to receive data input line	*/
char *words[2];				/* to split data input line	*/
int wordCount = 0;			/* result of split	*/
int lineCount = 0;			/* counting all input lines	*/
int validLines = 0;			/* counting only lines with data */
unsigned long long previousOffset = 0;	/* for data missing detection */
double dataValue = 0.0;				/* from data input	*/
char *wigfile = (char *) NULL;	/*	file name of wiggle database file */

/*	for each input data file	*/
for( i = 1; i < argc; ++i )
    {
    if (verbose ) printf("translating file: %s\n", argv[i]);
    /* let's shorten the dataSpan name which will be used in the
     * "Name of item" column
     */
    fileName = basename(argv[i]);
    if ( dataSpan < 1024 )
	{
	    snprintf( spanName, sizeof(spanName)-1, "%llu", dataSpan );
	} else if ( dataSpan < 1024*1024 ) {
	    snprintf( spanName, sizeof(spanName)-1, "%lluK", dataSpan/1024 );
	} else if ( dataSpan < 1024*1024*1024 ) {
	    snprintf( spanName, sizeof(spanName)-1, "%lluM",
		    dataSpan/(1024*1024) );
	} else {
	    snprintf( spanName, sizeof(spanName)-1, "%lluG",
		    dataSpan/(1024*1024*1024) );
	}
    if (name )		/*	Is the name of this feature specified ?	*/
	{
	    snprintf( featureName, sizeof(featureName) - 1, "%s", name );
	}
    if (chrom )		/*	Is the chrom name specified ? */
	{
	chromName = cloneString(chrom);
	if (! name )	/*	that names the feature too if not already */
	    snprintf( featureName, sizeof(featureName) - 1, "%s", chrom );
	}
    /*	Name mangling to determine output file name */
    if (wibFile )	/*	when specified, simply use it	*/
	{
	binfile = addSuffix(wibFile, ".wib");
	wigfile = addSuffix(wibFile, ".wig");
	} else {	/*	not specified, construct from input names */
	if (startsWith("chr",fileName) )
	    {
	    char *tmpString;
	    tmpString = cloneString(fileName);
	    chopSuffix(tmpString);
	    binfile = addSuffix(tmpString, ".wib");
	    wigfile = addSuffix(tmpString, ".wig");
	    if (! chrom )	/*	if not already taken care of	*/
		chromName = cloneString(tmpString);
	    if (! name && ! chrom)	/*	if not already done	*/
		snprintf(featureName, sizeof(featureName) - 1, "%s", tmpString);
	    freeMem(tmpString);
	    } else {
errAbort("Can not determine output file name, no -wibFile specified\n");
	    }
	}

    if (verbose ) printf("output files: %s, %s\n", binfile, wigfile);
    lineCount = 0;	/* to count all lines	*/
    validLines = 0;	/* to count only lines with data */
    rowCount = 0;	/* to count rows output */
    bincount = 0;	/* to count up to binsize	*/
    fileOffset = 0;	/* current location within binary data file	*/
    fileOffsetBegin = 0;/* location in binary data file where this bin starts*/
    if (data_values) freeMem((void *) data_values);
    if (validData) freeMem((void *) validData);
    data_values = (double *) needMem( (size_t) (binsize * sizeof(double)));
    validData = (unsigned char *)
		needMem( (size_t) (binsize * sizeof(unsigned char)));
    overallLowerLimit = 1.0e+300;	/* for the complete set of data */
    overallUpperLimit = -1.0e+300;	/* for the complete set of data */
    binout = mustOpen(binfile,"w");	/*	binary data file	*/
    wigout = mustOpen(wigfile,"w");	/*	table row definition file */
    lf = lineFileOpen(argv[i], TRUE);	/*	input file	*/
    while (lineFileNext(lf, &line, NULL))
	{
	boolean readingFrameSlipped;
	char *valEnd;
	char *val;

	++lineCount;
	chopPrefixAt(line, '#'); /* ignore any comments starting with # */
	if (strlen(line) < 3 )	/*	anything left on this line */
	    continue;		/*	no, go to next line	*/

	++validLines;
	wordCount = chopByWhite(line, words, 2);
	if (wordCount < 2 )
errAbort("Expecting at least two words at line %d, found %d", lineCount, wordCount);
	Offset = atoll(words[0]);
	val = words[1];
	dataValue = strtod(val, &valEnd);
	if ((*val == '\0') || (*valEnd != '\0'))
	    errAbort("Not a valid float at line %d: %s\n", lineCount, words[1]);
	if (Offset < 1 )
	    errAbort("Illegal offset: %llu at line %d, dataValue: %g", Offset, 
		    lineCount, dataValue );
	Offset -= 1;	/* our coordinates are zero relative half open */
	/* see if this is the first time through, establish chromStart 	*/
	if (validLines == 1 ) {
	    chromStart = Offset;
	    if (verbose )
printf("first offset: %llu\n", chromStart);
	}
	/* if we are working on a zoom level and the data is not exactly
	 * spaced according to the span, then we need to put each value
	 * in its own row in order to keep positioning correct for these
	 * data values.  The number of skipped bases has to be an even
	 * multiple of dataSpan
	 */
	readingFrameSlipped = FALSE;
	if ((validLines > 1) && (dataSpan > 1))
	    {
	    int skippedBases;
	    int spansSkipped;
	    skippedBases = Offset - previousOffset;
	    spansSkipped = skippedBases / dataSpan;
	    if ((spansSkipped * dataSpan) != skippedBases)
		readingFrameSlipped = TRUE;
	    }
	if (readingFrameSlipped)
	    {
	    if (verbose )
printf("data not spanning %llu bases, prev: %llu, this: %llu, at line: %d\n", dataSpan, previousOffset, Offset, lineCount );
	    output_row();
	    }
	/*	Check to see if data is being skipped	*/
	else if ( (validLines > 1) && (Offset > (previousOffset + dataSpan)) )
	    {
	    unsigned long long off;
	    unsigned long long fillSize;	/* number of bytes */
	    if (verbose )
printf("missing data offsets: %llu - %llu\n",previousOffset+1,Offset-1);
	    /*	If we are just going to fill the rest of this bin with
	     *  no data, then may as well stop here.  No need to fill
	     *  it with nothing.
	     */
	    fillSize = (Offset - (previousOffset + dataSpan)) / dataSpan;
	    if (verbose )
printf("filling NO_DATA for %llu bytes\n", fillSize );
	    if (fillSize + bincount >= binsize )
		{
		    output_row();
	    if (verbose )
printf("completed a bin due to  NO_DATA for %llu bytes, only %llu - %llu = %llu to go\n", fillSize, binsize, bincount, binsize - bincount );
		} else {
		fillSize = 0;
		/*	fill missing data with NO_DATA indication	*/
		for( off = previousOffset + dataSpan; off < Offset;
			off += dataSpan )
		    {
		    ++fillSize;
		    ++fileOffset;
		    ++bincount;	/*	count scores in this bin */
		    if (bincount >= binsize ) break;
		    }
	    if (verbose )
printf("filled NO_DATA for %llu bytes\n", fillSize );
		/*	If that finished off this bin, output it */
		    if (bincount >= binsize )
			{
			output_row();
			}
	        }
	    }
	/*	With perhaps the missing data taken care of, back to the
	 *	real data.
	 */
	data_values[bincount] = dataValue;
	validData[bincount] = TRUE;
	++fileOffset;
	++bincount;	/*	count scores in this bin */
	/*	Is it time to output a row definition ? */
	if (bincount >= binsize )
	    {
	    output_row();
	    }
	previousOffset = Offset;
        }	/*	reading file input loop end	*/
    /*	Done with input file, any data points left in this bin ?	*/
    if (bincount)
	{
	output_row();
	}
    lineFileClose(&lf);
    fclose(binout);
    fclose(wigout);
    freeMem(binfile);
    freeMem(wigfile);
    freeMem(chromName);
    binfile = (char *) NULL;
    wigfile = (char *) NULL;
    chromName = (char *) NULL;
    if (verbose )
printf("fini: %s, read %d lines, table rows: %llu, data bytes: %lld\n",
	argv[i], lineCount, rowCount, fileOffset );
    printf("data limits: [%g:%g], range: %g\n", overallLowerLimit,
    		overallUpperLimit, overallUpperLimit - overallLowerLimit);
    }

return;
}

int main( int argc, char *argv[] ) {
optionInit(&argc, argv, optionSpecs);

if(argc < 2)
    usage();

add_offset = optionLongLong("offset", 0);
binsize = optionLongLong("binsize", 1024);
dataSpan = optionLongLong("dataSpan", 1);
chrom = optionVal("chrom", NULL);
wibFile = optionVal("wibFile", NULL);
name = optionVal("name", NULL);
verbose = optionExists("verbose");

if (verbose ) {
    printf("options: -verbose, offset= %llu, binsize= %llu, dataSpan= %llu\n",
	add_offset, binsize, dataSpan);
    if (chrom ) {
	printf("-chrom=%s\n", chrom);
    }
    if (wibFile ) {
	printf("-wibFile=%s\n", wibFile);
    }
    if (name ) {
	printf("-name=%s\n", name);
    }
}
wigAsciiToBinary(argc, argv);
exit(0);
}
