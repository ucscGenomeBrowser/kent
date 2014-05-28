/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
 *	missing data in it, the missing data will be set to the
 *	"WIG_NO_DATA" indication.  (WIG_NO_DATA == 128)
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

#define	WIG_NO_DATA	128
#define MAX_BYTE_VALUE	127


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"offset", OPTION_LONG_LONG},
    {"binsize", OPTION_LONG_LONG},
    {"dataSpan", OPTION_LONG_LONG},
    {"chrom", OPTION_STRING},
    {"wibFile", OPTION_STRING},
    {"name", OPTION_STRING},
    {"minVal", OPTION_FLOAT},
    {"maxVal", OPTION_FLOAT},
    {"obsolete", OPTION_BOOLEAN},
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
static char * chrom = (char *) NULL;	/* to specify chrom name	*/
static char * wibFile = (char *) NULL;	/* to name the .wib file	*/
static char * name = (char *) NULL;	/* to specify feature name	*/
static double minVal = BIGNUM;          /* minimum value that is allowed in data. */
static double maxVal = BIGNUM;     /* maximum value that is allowed in data. */
static boolean trimVals = FALSE;/*should the data be trimmed by min & maxVal?*/
static boolean obsolete = FALSE;/*use this program even though it is obsolete*/

static void usage()
/* Explain how to use program. */
{
errAbort(
    "wigAsciiToBinary - convert ascii Wiggle data to binary file\n"
    "usage: wigAsciiToBinary [options] <file names>\n"
    "\toptions:\n"
    "\t<file names> - list of files to process, (use: stdin for data from pipe)\n"
    "\t-offset=N - add N to all coordinates, default 0\n"
    "\t-binsize=N - # of points per database row entry, default 1024\n"
    "\t-dataSpan=N - # of bases spanned for each data point, default 1\n"
    "\t-chrom=chrN - this data is for chrN. Use with twoColumn output\n"
    "\t-wibFile=chrN - to name the .wib output file\n"
    "\t-name=<feature name> - to name the feature, default chrN or\n"
    "\t\t-chrom= specified\n"
    "\t-verbose=N - display process while underway (only N=2 does something)\n"
    "\t-minVal=N - minimum allowable data value, values will be capped here.\n"
    "\t-maxVal=N - maximum allowable data value, values will be capped here.\n"
    "\t-obsolete - Use this program even though it is obsolete.\n"
    "If the name of the input files are of the form: chrN.<....> this will\n"
    "\tset the output file names.  Otherwise use the -wibFile option.\n"
    "Generally the ascii files have one to three columns.\n"
    "\t<sequence name> <sequence position> <data value>   or \n"
    "\t<sequence position> <data value> or\n"
    "\t<data value>\n"
    "If the sequence position is not specified in a line, it is assumed to be\n"
    "one past the previous position.  If the sequence name is not specified\n"
    "it is the same as the previous line.\n"
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
 */
static void output_row()
{
double lowerLimit = 1.0e+300;
double upperLimit = -1.0e+300;
double dataRange = 0.0;
double sumData = 0.0;
double sumSquares = 0.0;
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
    }
    dataRange = upperLimit - lowerLimit;
    if (upperLimit > overallUpperLimit) overallUpperLimit = upperLimit;
    if (lowerLimit < overallLowerLimit) overallLowerLimit = lowerLimit;
    /*	With limits determined, can now scale and output the values
     *  compressed down to byte data values for the binary file
     */
    for (i=0; i < bincount; ++i)
	{
	int byteOutput;	/* should be unsigned char, but fputc() wants int */
	if (validData[i])
	    {
		if (dataRange > 0.0)
		    {
			byteOutput = MAX_BYTE_VALUE *
				((data_values[i] - lowerLimit)/dataRange);
		    } else {
			byteOutput = 0;
		    }
	    } else {
		byteOutput = WIG_NO_DATA;
	    }
	fputc(byteOutput,binout);	/*	output a data byte */
	}

    chromEnd = chromStart + (bincount * dataSpan);
    verbose(2, "row: %llu %llu-%llu total: %llu valid: %llu, miss: %llu, file offset: %llu\n", rowCount, chromStart+add_offset, chromEnd+add_offset, bincount, validCount, bincount-validCount, fileOffsetBegin);
    fprintf( wigout,
"%s\t%llu\t%llu\t%s.%llu\t%llu\t%llu\t%llu\t%s\t%g\t%g\t%llu\t%g\t%g\n",
	chromName, chromStart+add_offset, chromEnd+add_offset,
	featureName, rowCount, dataSpan, bincount,
	(unsigned long long)fileOffsetBegin, basename(binfile), lowerLimit,
	dataRange, validCount, sumData, sumSquares);
    ++rowCount;
    }
bincount = 0;	/* to count up to binsize	*/
for (i=0; i < binsize; ++i)	/*	reset valid indicator	*/
    validData[i] = FALSE;
chromStart = Offset + dataSpan;
verbose(2, "next chromStart = %llu = %llu + %llu\n", chromStart, Offset, dataSpan);
fileOffsetBegin = fileOffset;

}	/*	static void output_row()	*/

void badFormat(struct lineFile *lf)
/* Complain about format. */
{
errAbort("Badly formatted line %d of %s", lf->lineIx, lf->fileName);
}

void wigAsciiToBinary( int argc, char *argv[] )
{
int i = 0;				/* general purpose int counter	*/
struct lineFile *lf;			/* for line file utilities	*/
char * fileName;			/* the basename of the input file */
char *line = (char *) NULL;		/* to receive data input line	*/
char *words[4];				/* to split data input line	*/
int wordCount = 0;			/* result of split	*/
int validLines = 0;			/* counting only lines with data */
unsigned long long previousOffset = 0;	/* for data missing detection */
double dataValue = 0.0;				/* from data input	*/
char *wigfile = (char *) NULL;	/*	file name of wiggle database file */
boolean firstInChrom;		/* Is this the first line in chromosome? */

/*	for each input data file	*/
for (i = 1; i < argc; ++i)
    {
    verbose(2, "translating file: %s\n", argv[i]);

    fileName = basename(argv[i]);
    if (name)		/*	Is the name of this feature specified ?	*/
	{
	safef( featureName, sizeof(featureName) - 1, "%s", name);
	}
    if (chrom)		/*	Is the chrom name specified ? */
	{
	chromName = cloneString(chrom);
	if (! name)	/*	that names the feature too if not already */
	    safef( featureName, sizeof(featureName) - 1, "%s", chrom);
	}
    /*	Name mangling to determine output file name */
    if (wibFile)	/*	when specified, simply use it	*/
	{
	binfile = addSuffix(wibFile, ".wib");
	wigfile = addSuffix(wibFile, ".wig");
	} else {	/*	not specified, construct from input names */
	if (startsWith("chr",fileName))
	    {
	    char *tmpString;
	    tmpString = cloneString(fileName);
	    chopSuffix(tmpString);
	    binfile = addSuffix(tmpString, ".wib");
	    wigfile = addSuffix(tmpString, ".wig");
	    if (! chrom)	/*	if not already taken care of	*/
		chromName = cloneString(tmpString);
	    if (! name && ! chrom)	/*	if not already done	*/
		safef(featureName, sizeof(featureName) - 1, "%s", tmpString);
	    freeMem(tmpString);
	    } else {
	    errAbort("Can not determine output file name, no -wibFile specified\n");
	    }
	}

    verbose(2, "output files: %s, %s\n", binfile, wigfile);
    validLines = 0;	/* to count only lines with data */
    rowCount = 0;	/* to count rows output */
    bincount = 0;	/* to count up to binsize	*/
    fileOffset = 0;	/* current location within binary data file	*/
    fileOffsetBegin = 0;/* location in binary data file where this bin starts*/
    firstInChrom = TRUE;
    freeMem(data_values);
    freeMem(validData);
    data_values =  needMem( (size_t) (binsize * sizeof(double)));
    validData = needMem( (size_t) (binsize * sizeof(unsigned char)));
    overallLowerLimit = 1.0e+300;	/* for the complete set of data */
    overallUpperLimit = -1.0e+300;	/* for the complete set of data */
    binout = mustOpen(binfile,"w");	/*	binary data file	*/
    wigout = mustOpen(wigfile,"w");	/*	table row definition file */
    lf = lineFileOpen(argv[i], TRUE);	/*	input file	*/
    while (lineFileNextReal(lf, &line))
	{
	boolean readingFrameSlipped;
	char *valEnd;
	char *val;
	++validLines;
	wordCount = chopByWhite(line, words, ArraySize(words));
	if (wordCount == 1)
	    {
	    Offset += 1;
	    val = words[0];
	    }
	else if (wordCount == 2)
	    {
	    Offset = atoll(words[0]) - 1;
	    val = words[1];
	    }
	else if (wordCount == 3)
	    {
	    char *newChrom = words[0];
	    boolean sameChrom = (chromName == NULL || sameString(chromName, newChrom));
	    Offset = atoll(words[1]) - 1;
	    val = words[2];
	    if (!sameChrom)
		{
		output_row();
		firstInChrom = TRUE;
		freez(&chromName);
		}
	    if (chromName == NULL)
		chromName = cloneString(newChrom);
	    }
	else
	    {
	    val = NULL;
	    badFormat(lf);
	    }
	if (Offset < 0)
	    errAbort("Illegal offset %llu at line %d of %s", Offset+1, lf->lineIx,
	    	lf->fileName);
	dataValue = strtod(val, &valEnd);
	if(trimVals)
	    {
	    dataValue = max(minVal, dataValue);
	    dataValue = min(maxVal, dataValue);
	    }
	if ((*val == '\0') || (*valEnd != '\0'))
	    errAbort("Not a valid float at line %d: %s\n", lf->lineIx, val);
	/* see if this is the first time through, establish chromStart 	*/
	if (firstInChrom) {
	    chromStart = Offset;
	    verbose(2, "first offset: %llu\n", chromStart);
	}
	else if (!firstInChrom && (Offset <= previousOffset))
	    errAbort("ERROR: chrom positions not in order. line %d of %s\n"
	             "previous: %llu >= %llu <-current", 
		     lf->lineIx, lf->fileName, previousOffset+1, Offset+1);
	/* if we are working on a zoom level and the data is not exactly
	 * spaced according to the span, then we need to put each value
	 * in its own row in order to keep positioning correct for these
	 * data values.  The number of skipped bases has to be an even
	 * multiple of dataSpan
	 */
	readingFrameSlipped = FALSE;
	if (!firstInChrom && (dataSpan > 1))
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
	    verbose(2, "data not spanning %llu bases, prev: %llu, this: %llu, at line: %d\n", dataSpan, previousOffset, Offset, lf->lineIx);
	    output_row();
	    chromStart = Offset;	/*	a full reset here	*/
	    }
	/*	Check to see if data is being skipped	*/
	else if ( (!firstInChrom) && (Offset > (previousOffset + dataSpan)) )
	    {
	    unsigned long long off;
	    unsigned long long fillSize;	/* number of bytes */
	    verbose(2, "missing data offsets: %llu - %llu\n",
		    previousOffset+1,Offset-1);
	    /*	If we are just going to fill the rest of this bin with
	     *  no data, then may as well stop here.  No need to fill
	     *  it with nothing.
	     */
	    fillSize = (Offset - (previousOffset + dataSpan)) / dataSpan;
	    verbose(2, "filling NO_DATA for %llu bytes at bincount: %llu\n", fillSize, bincount);
	    if (fillSize + bincount >= binsize)
		{
		verbose(2, "completing a bin due to  NO_DATA for %llu bytes, only %llu - %llu = %llu to go\n", fillSize, binsize, bincount, binsize - bincount);
		verbose(2, "Offset: %llu, previousOffset: %llu\n",
			Offset, previousOffset);
		output_row();
		chromStart = Offset;	/*	a full reset here	*/
	    } else {
		fillSize = 0;
		/*	fill missing data with NO_DATA indication	*/
		for (off = previousOffset + dataSpan; off < Offset;
			off += dataSpan)
		    {
		    ++fillSize;
		    ++fileOffset;
		    ++bincount;	/*	count scores in this bin */
		    if (bincount >= binsize) break;
		    }
		verbose(2, "filled NO_DATA for %llu bytes at bincount: %llu\n", fillSize, bincount);
		/*	If that finished off this bin, output it
		 *	This most likely should not happen here.  The
		 *	check above: if (fillSize + bincount >= binsize) 
		 *	should have caught this case already.
		 */
		    if (bincount >= binsize)
			{
			output_row();
			chromStart = Offset;	/* a full reset here */
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
	if (bincount >= binsize)
	    {
	    output_row();
	    }
	previousOffset = Offset;
	firstInChrom = FALSE;
        }	/*	reading file input loop end	*/
    /*	Done with input file, any data points left in this bin ?	*/
    if (bincount)
	{
	output_row();
	}
    verbose(2, "fini: %s, read %d lines, table rows: %llu, data bytes: %lld\n",
	    argv[i], lf->lineIx, rowCount, fileOffset);
    verbose(1, "data limits: [%g:%g], range: %g\n", 
	overallLowerLimit, overallUpperLimit,
	overallUpperLimit - overallLowerLimit);
    lineFileClose(&lf);
    fclose(binout);
    fclose(wigout);
    freeMem(binfile);
    freeMem(wigfile);
    freeMem(chromName);
    binfile = (char *) NULL;
    wigfile = (char *) NULL;
    chromName = (char *) NULL;
    }
return;
}

int main( int argc, char *argv[] )
{
optionInit(&argc, argv, optionSpecs);

obsolete = optionExists("obsolete");
if (! obsolete)
    {
    verbose(1,"ERROR: This loader is obsolete.  Please use: 'wigEncode'\n");
    verbose(1,"\t(use -obsolete flag to run this encoder despite it being obsolete)\n");
    errAbort("ERROR: wigAsciiToBinary is obsolete, use 'wigEncode' instead");
    }

if (argc < 2)
    usage();

add_offset = optionLongLong("offset", 0);
binsize = optionLongLong("binsize", 1024);
dataSpan = optionLongLong("dataSpan", 1);
chrom = optionVal("chrom", NULL);
wibFile = optionVal("wibFile", NULL);
name = optionVal("name", NULL);
minVal = optionFloat("minVal", -1 * INFINITY);
maxVal = optionFloat("maxVal", INFINITY);
trimVals = optionExists("minVal") || optionExists("maxVal");

verbose(2, "options: -verbose, offset= %llu, binsize= %llu, dataSpan= %llu\n",
    add_offset, binsize, dataSpan);
if (chrom)
    {
    verbose(2, "-chrom=%s\n", chrom);
    }
if (wibFile)
    {
    verbose(2, "-wibFile=%s\n", wibFile);
    }
if (name)
    {
    verbose(2, "-name=%s\n", name);
    }
wigAsciiToBinary(argc, argv);
exit(0);
}
