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
#include	"wiggle.h"


static char const rcsid[] = "$Id: wigAsciiToBinary.c,v 1.2 2004/05/05 23:49:33 hiram Exp $";

static unsigned long lineCount = 0;	/* counting all input lines	*/
static long long add_offset = 0;	/* to allow "lifting" of the data */
static long long binsize = 1024;	/* # of data points per table row */
static long long dataSpan = 1;		/* bases spanned per data point */

static unsigned long long bincount = 0;	/* to count binsize for wig file */
static unsigned long long chromStart = 0;	/* for table row data */
static unsigned long long fixedStart = 0;	/* for fixedStep data */
static unsigned long long stepSize = 1;	/* for fixedStep */
static unsigned long long chromEnd = 0;	/* for table row data */
static unsigned long long bedChromStart = 0;	/* for bed format */
static unsigned long long bedChromEnd = 0;	/* for bed format */
static double bedDataValue = 0.0;		/* for bed format */
static FILE *wigout = (FILE *)NULL;  /* file handle for wiggle database file */
static FILE *binout = (FILE *)NULL;	/* file handle for binary data file */
static char *chromName = (char *)NULL;	/* pseudo bed-6 data to follow */
static char *featureName = (char *)NULL;/* the name of this feature */
static unsigned long long rowCount = 0;	/* to count rows output */
static off_t fileOffsetBegin = 0;	/* where this bin started in binary */
static unsigned long long Offset = 0;	/* from data input	*/
static unsigned long long previousOffset = 0;  /* for data missing detection */
static off_t fileOffset = 0;		/* where are we in the binary data */
static double *data_values = (double *) NULL;
			/* all the data values read in for this one row */
static unsigned char * validData = (unsigned char *) NULL;
				/* array to keep track of what data is valid */
static double overallUpperLimit = -1.0e+300; /* for the complete set of data */
static double overallLowerLimit = 1.0e+300; /* for the complete set of data */
static char *wibFileName = (char *)NULL;	/* for use in output_row() */

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
	errAbort("wigAsciiToBinary internal error: empty row being produced at row: %d, line: %lu\n", rowCount, lineCount);
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
			byteOutput = MAX_WIG_VALUE *
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
	fileOffsetBegin, wibFileName, lowerLimit,
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

static void setDataSpan(char *spec)
{
char *wordPairs[2];
int wc;
char *clone;

clone = cloneString(spec);
wc = chopByChar(spec, '=', wordPairs, 2);
if (wc != 2)
    errAbort("Expecting span=N, no N found in %s at line %lu",
	clone, lineCount);
dataSpan = sqlLongLong(wordPairs[1]);
freeMem(clone);
}

static void setStepSize(char *spec)
/*	given "step=<position>", parse and set stepSize and dataSpan */
{
char *wordPairs[2];
int wc;
char *clone;

clone = cloneString(spec);
wc = chopByChar(spec, '=', wordPairs, 2);
if (wc != 2)
    errAbort("Expecting step=<size>, no <size> found in %s at line %lu",
	clone, lineCount);
stepSize=sqlLongLong(wordPairs[1]);
freeMem(clone);
}


static void setFixedStart(char *spec)
/*	given "start=<position>", parse and set fixedStart */
{
char *wordPairs[2];
int wc;
char *clone;

clone = cloneString(spec);
wc = chopByChar(spec, '=', wordPairs, 2);
if (wc != 2)
    errAbort("Expecting start=<position>, no <position> found in %s at line %lu",
	clone, lineCount);
fixedStart=sqlLongLong(wordPairs[1]);
if (fixedStart == 0)
    errAbort("Found start=0 at line %lu, the first chrom position is 1, not 0",
	lineCount);
else
    fixedStart -= 1;	/* zero relative half-open */
freeMem(clone);
}

static void setChromName(char *spec)
/*	given "chrom=<name>", parse and set chromName, featureName */
{
char *wordPairs[2];
int wc;
char *clone;

clone = cloneString(spec);
wc = chopByChar(spec, '=', wordPairs, 2);
if (wc != 2)
    errAbort("Expecting chrom=<name>, no <name> found in %s at line %lu",
	clone, lineCount);
freez(&chromName);
chromName=cloneString(wordPairs[1]);
freez(&featureName);
featureName=cloneString(wordPairs[1]);
freeMem(clone);
}

/*	add a data value to the accumulating pile of data */
static void setDataValue(double val)
{
data_values[bincount] = val;
validData[bincount] = TRUE;
++fileOffset;
++bincount;	/*	count scores in this bin */
/*	Is it time to output a row definition ? */
if (bincount >= binsize)
    output_row();
}

void wigAsciiToBinary( char *wigAscii, char *wigFile, char *wibFile,
   double *upperLimit, double *lowerLimit )
/*	given the three file names, read the ascii wigAscii file and produce
 *	the wigFile and wibFile outputs
 */
{
struct lineFile *lf;			/* for line file utilities	*/
char *line = (char *) NULL;		/* to receive data input line	*/
char *words[10];				/* to split data input line	*/
int wordCount = 0;			/* result of split	*/
int validLines = 0;			/* counting only lines with data */
double dataValue = 0.0;			/* from data input	*/
boolean bedData = FALSE;		/* in bed format data */
boolean variableStep = FALSE;		/* in variableStep data */
boolean fixedStep = FALSE;		/* in fixedStep data */

if ((wigAscii == (char *)NULL) || (wigFile == (char *)NULL) ||
    (wibFile == (char *)NULL))
	errAbort("wigAsciiToBinary: missing data file names, ascii: %s, wig: %s, wib: %s", wigAscii, wigFile, wibFile);

freez(&wibFileName);			/* send this name to the global */
wibFileName = cloneString(wibFile);	/* variable for use in output_row() */
lineCount = 0;	/* to count all lines	*/
add_offset = 0;	/* to allow "lifting" of the data */
validLines = 0;	/* to count only lines with data */
rowCount = 0;	/* to count rows output */
bincount = 0;	/* to count up to binsize	*/
binsize = 1024;	/* # of data points per table row */
dataSpan = 1;	/* default bases spanned per data point */
chromStart = 0;	/* for table row data */
previousOffset = 0;  /* for data missing detection */
fileOffset = 0;	/* current location within binary data file	*/
fileOffsetBegin = 0;/* location in binary data file where this bin starts*/
freez(&data_values);
freez(&validData);
data_values = (double *) needMem( (size_t) (binsize * sizeof(double)));
validData = (unsigned char *)
	    needMem( (size_t) (binsize * sizeof(unsigned char)));
overallLowerLimit = 1.0e+300;	/* for the complete set of data */
overallUpperLimit = -1.0e+300;	/* for the complete set of data */
binout = mustOpen(wibFile,"w");	/*	binary data file	*/
wigout = mustOpen(wigFile,"w");	/*	table row definition file */
#if defined(DEBUG)
chmod(wibFile, 0666);	/*	dbg	*/
chmod(wigFile, 0666);	/*	dbg	*/
#endif
lf = lineFileOpen(wigAscii, TRUE);	/*	input file	*/
while (lineFileNext(lf, &line, NULL))
    {
    boolean readingFrameSlipped;

    ++lineCount;
    line = skipLeadingSpaces(line);
    if (line[1] == '#')		/*	ignore comment lines	*/
	continue;		/*	!!! go to next line of input */

    wordCount = chopByWhite(line, words, 10);
    if (sameWord("variableStep",words[0]))
	{
	int i;
	boolean foundChrom = FALSE;
	if (variableStep | bedData | fixedStep)
	    {
	    output_row();
	    validLines = 0;	/*	reset for first offset	*/
	    }
	dataSpan = 1;	/* default bases spanned per data point */
	for(i = 1; i < wordCount; ++i)
	    {
	    if (startsWith("chrom",words[i]))
		{
		setChromName(words[i]);
		foundChrom = TRUE;
		}
	    else if (startsWith("span",words[i]))
		setDataSpan(words[i]);
	    else
		errAbort("illegal specification on variableStep at line %lu: %s",
		    lineCount, words[i]);
	    }
	if (!foundChrom)
	    errAbort("missing chrom=<name> specification on variableStep declaration at line %lu", lineCount);
	variableStep = TRUE;
	bedData = FALSE;
	fixedStep = FALSE;
	continue;		/*	!!!  go to next input line	*/
	}
    else if (sameWord("fixedStep",words[0]))
	{
	boolean foundChrom = FALSE;
	boolean foundStart = FALSE;
	int i;

	if (variableStep | bedData | fixedStep)
	    {
	    output_row();
	    validLines = 0;	/*	reset for first offset	*/
	    }
	stepSize = 1;	/*	default step size	*/
	dataSpan = 1;	/* default bases spanned per data point */
	for(i = 1; i < wordCount; ++i)
	    {
	    if (startsWith("chrom",words[i]))
		{
		setChromName(words[i]);
		foundChrom = TRUE;
		}
	    else if (startsWith("start",words[i]))
		{
		setFixedStart(words[i]);
		foundStart = TRUE;
		}
	    else if (startsWith("step",words[i]))
		setStepSize(words[i]);
	    else if (startsWith("span",words[i]))
		setDataSpan(words[i]);
	    else
		errAbort("illegal specification on variableStep at line %lu: %s",
		    lineCount, words[i]);
	    }
	if (!foundChrom)
	    errAbort("missing chrom=<name> specification on fixedStep declaration at line %lu", lineCount);
	if (!foundStart)
	    errAbort("missing start=<position> specification on fixedStep declaration at line %lu", lineCount);
	variableStep = FALSE;
	bedData = FALSE;
	fixedStep = TRUE;
	continue;		/*	!!!  go to next input line	*/
	}
    else if (wordCount == 4)
	{
	if (variableStep | fixedStep)
	    {
	    output_row();
	    validLines = 0;	/*	reset for first offset	*/
	    }
	dataSpan = 1;	/* default bases spanned per data point */
	variableStep = FALSE;
	bedData = TRUE;
	fixedStep = FALSE;
	freez(&chromName);
	chromName=cloneString(words[0]);
	freez(&featureName);
	featureName=cloneString(words[0]);
	bedChromStart = sqlLongLong(words[1]);
	bedChromEnd = sqlLongLong(words[2]);
	bedDataValue = sqlDouble(words[3]);
	if (bedChromStart == 0)
	    errAbort("Found chromStart=0 at line %lu, the first chrom position is 1, not 0",
		lineCount);
	else
	    bedChromStart -= 1;	/* zero relative half-open */
	if (bedChromStart > bedChromEnd)
	    errAbort("Found chromStart > chromEnd at line %lu (%llu > %llu)",
		lineCount, bedChromStart, bedChromEnd);
	}

    if (variableStep && (wordCount != 2))
	errAbort("Expecting two words at line %lu, found %d",
	    lineCount, wordCount);
    if (fixedStep && (wordCount != 1))
	errAbort("Expecting one word at line %lu, found %d",
	    lineCount, wordCount);
    if (bedData && (wordCount != 4))
	errAbort("Expecting four words at line %lu, found %d",
	    lineCount, wordCount);

    ++validLines;		/*	counting good lines of data input */

    /*	Offset is the incoming specified position for this value */
    if (variableStep)
	{
	Offset = sqlLongLong(words[0]);
	Offset -= 1;	/* zero relative half open */
	dataValue = sqlDouble(words[1]);
	}
    else if (fixedStep)
	{
	Offset = fixedStart + (stepSize * (validLines - 1));
	dataValue = sqlDouble(words[0]);
	}
    else if (bedData)
	{
	Offset = bedChromStart;
	dataValue = bedDataValue;
	}

    /*	the initial data point sets the chromStart position	*/
    if (validLines == 1) {
	chromStart = Offset;
	verbose(2, "first offset: %llu\n", chromStart);
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
	if (Offset < previousOffset)
	    errAbort("chrom positions not in numerical order at line %lu. previous: %llu > %llu <-current", lineCount, previousOffset, Offset);
	skippedBases = Offset - previousOffset;
	spansSkipped = skippedBases / dataSpan;
	if ((spansSkipped * dataSpan) != skippedBases)
	    readingFrameSlipped = TRUE;
	}

    if (readingFrameSlipped)
	{
	verbose(2, "data not spanning %llu bases, prev: %llu, this: %llu, at line: %lu\n", dataSpan, previousOffset, Offset, lineCount);
	output_row();
	chromStart = Offset;	/*	a full reset here	*/
	}
    /*	Check to see if data is being skipped	*/
    else if ( (validLines > 1) && (Offset > (previousOffset + dataSpan)) )
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
	verbose(2, "filling NO_DATA for %llu bytes\n", fillSize);
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
	    verbose(2, "filled NO_DATA for %llu bytes\n", fillSize);
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
    if (bedData)
	{
	unsigned long long bedSize = bedChromEnd - bedChromStart;
	for ( ; bedSize > 0; --bedSize )
	    {
	    setDataValue(bedDataValue);
	    Offset += 1;
	    }
	Offset -= 1;	/*	loop above increments this one too much.
			 *	This Offset is supposed to be the last
			 *	valid chrom position written, not the
			 *	next to be written */
	}
    else
	{
	setDataValue(dataValue);
	}
    previousOffset = Offset;	/* remember position for gap calculations */
    }	/*	reading file input loop end	*/

/*	Done with input file, any data points left in this bin ?	*/
if (bincount)
    output_row();

lineFileClose(&lf);
fclose(binout);
fclose(wigout);
freez(&chromName);
freez(&featureName);
freez(&data_values);
freez(&validData);
freez(&wibFileName);
/*	return limits if pointers are given	*/
if (upperLimit)
    *upperLimit = overallUpperLimit;
if (lowerLimit)
    *lowerLimit = overallLowerLimit;
return;
}
