/* wigAsciiToBinary - convert ascii Wiggle data to binary file
 *	plus, produces file to read for load into database
 *
 *	The format of the incoming data is described at:
 *	http://hgwdev.cse.ucsc.edu/encode/submission.html#WIG
 *
 *	Incoming data points are chunked up into blocks.
 *	Each block of data is described by an SQL row in the .wig file
 *	output.  The data values are converted to single bytes in a
 *	binning scheme.  These binary bytes are written to the .wib file.
 *
 *	The binary data file is simply one unsigned char value per byte.
 *	The suffix for this binary data file is: .wib - wiggle binary
 *
 *	The second file produced is a bed-like format file, with chrom,
 *	chromStart, chromEnd, etc.  See hg/inc/wiggle.h for structure
 *	definition.  The important thing about this file is that it will
 *	have the pointers into the binary data file and statistical
 *	summaries of the data values in this block.
 *	This file extension is: .wig - wiggle
 *
 *	Holes in the data are allowed.   Incoming data specifications
 *	set the span of a data point == how many nucleotide
 *	bases the value is representative of.
 *
 *	There is a concept of a "bin" size which means how many values
 *	should be maintained in a single table row entry. Holes in the input
 *	data will be filled with WIG_NO_DATA values up to the size of the
 *	current "bin".  If the missing data continues past the next "bin" size
 *	number of data points, that table row will be skipped.  Thus, it
 *	is only necessary to have table rows which contain valid data.
 *	binsize is currently 1024, however it will need to be an input
 *	parameter for the command line version of this function.
 *	(there are several parameters that need to come in from the
 *	command line version -- to be done).
 */
#include	"common.h"
#include	"options.h"
#include	"linefile.h"
#include	"wiggle.h"

static char const rcsid[] = "$Id: wigAsciiToBinary.c,v 1.25 2008/09/10 22:57:05 larrym Exp $";

/*	This list of static variables is here because the several
 *	subroutines in this source file need access to all this business
 *	and since there are so many it is not convenient to pass them
 *	around in a structure.  Just make sure they are initialized
 *	properly upon entry to this function.  This used to be a static
 *	command line program and thus these did not need to be set
 *	except for this declaration, now they are all re-initialized
 *	upon entry to the function.
 *	Functions in this source file:
 *	static void output_row();
 *	static void setDataSpan(char *spec);
 *	static void setStepSize(char *spec);
 *	static void setFixedStart(char *spec);
 *	static void setChromName(char *spec);
 *	static void setDataValue(double val);
 *	void wigAsciiToBinary( char *wigAscii, char *wigFile, char *wibFile,
 *		double *upperLimit, double *lowerLimit );
 */
static unsigned long lineCount = 0;	/* counting all input lines	*/
static long long add_offset = 0;	/* to allow "lifting" of the data */
static boolean noOverlap = FALSE;	/* check for overlapping data */
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
static long long wibSizeLimit = 0;	/*	governor on ct trash files */
static long long wibSize = 0;	/*	counting up to limit */

#if defined(NOT)
/*	not used at this time, perhaps in the future */
static int isIntF(float x)
    {
    float y = floorf(x);
    return (0.0 == (x - y));
    }
#endif

static int isIntD(double x)
    {
    double y = floor(x);
    return (0.0 == (x - y));
    }

/*
 *	The table rows need to be printed in a couple of different
 *	places in the code.
 *	Definition of this row format can be found in hg/inc/wiggle.h
 *	and hg/lib/wiggle.as, etc ...
 *	output_row() is called each time a block of data values are
 *	complete, this produces one SQL line definition of a data block.
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
boolean allIntegers = FALSE;	/*	off until further notice */
	/*	with the allIntegers function on, the "Maximum" of the
	 *	data ends up being artifically high.
	 */

if (bincount)
    {
    wibSize += bincount;	/* counting up to check limit	*/
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
#ifdef NOT
	    if (allIntegers && (! isIntD(data_values[i])))
		allIntegers = FALSE;
#endif
	    }
	}
    if (validCount < 1) {
	errAbort("wigAsciiToBinary internal error: empty row being produced at row: %lld, line: %lu\n", rowCount, lineCount);
    }

    dataRange = upperLimit - lowerLimit;

    /*	If all integer data and will fit in the 128 bins,
     *	then do them as integer bins
     */

    if (allIntegers && isIntD(dataRange) && isIntD(upperLimit) &&
	isIntD(lowerLimit) && (dataRange < (double)MAX_WIG_VALUE) &&
	(dataRange > 0.0))
	{
	dataRange = MAX_WIG_VALUE;
	}

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
    verbose(2, "row: %llu %llu-%llu total: %llu valid: %llu, miss: %llu, file offset: %llu\n", rowCount, chromStart+add_offset, chromEnd+add_offset, bincount, validCount, bincount-validCount, (unsigned long long)fileOffsetBegin);
    fprintf( wigout,
"%s\t%llu\t%llu\t%s.%llu\t%llu\t%llu\t%llu\t%s\t%g\t%g\t%llu\t%g\t%g\n",
	chromName, chromStart+add_offset, chromEnd+add_offset,
	featureName, rowCount, dataSpan, bincount,
	(unsigned long long)fileOffsetBegin, wibFileName, lowerLimit,
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
/*	given *spec: "dataSpan=N",  set parse and dataSpan to N	*/
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
/*	given *spec: "step=<position>", parse and set stepSize and dataSpan */
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
/*	given *spec: "start=<position>", parse and set fixedStart */
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
    fixedStart = BASE_0(fixedStart);	/* zero relative half-open */
freeMem(clone);
}

static void setChromName(char *spec)
/*	given *spec: "chrom=<name>", parse and set chromName, featureName */
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

static void setDataValue(double val)
/*	add a data value to the accumulating block of data */
{
data_values[bincount] = val;
validData[bincount] = TRUE;
++fileOffset;
++bincount;	/*	count scores in this bin */
/*	Is it time to output a row definition ? */
if (bincount >= binsize)
    output_row();
}

/*	The single externally visible routine.
 *	Future improvements will need to add a couple more arguments to
 *	satisify the needs of the command line version and its options.
 *	Currently, this is used only in customTrack input parsing.
 */
void wigAsciiToBinary( char *wigAscii, char *wigFile, char *wibFile,
   double *upperLimit, double *lowerLimit, struct wigEncodeOptions *options)
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
char *prevChromName = (char *)NULL;	/* to watch for chrom name changes */

if ((wigAscii == (char *)NULL) || (wigFile == (char *)NULL) ||
    (wibFile == (char *)NULL))
	errAbort("wigAsciiToBinary: missing data file names, ascii: %s, wig: %s, wib: %s", wigAscii, wigFile, wibFile);

/*	need to be careful here and initialize all the global variables */
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

if (options != NULL)
    {
    if (options->lift != 0)
	add_offset = options->lift;
    if (options->noOverlap)
	noOverlap = TRUE;
    if (options->wibSizeLimit > 0)
	wibSizeLimit = options->wibSizeLimit;
    }

overallLowerLimit = 1.0e+300;	/* for the complete set of data */
overallUpperLimit = -1.0e+300;	/* for the complete set of data */
binout = mustOpen(wibFile,"w");	/*	binary data file	*/
wigout = mustOpen(wigFile,"w");	/*	table row definition file */
#if defined(DEBUG)	/*	dbg	*/
chmod(wibFile, 0666);
chmod(wigFile, 0666);
#endif
lf = lineFileOpen(wigAscii, TRUE);	/*	input file	*/
while (lineFileNext(lf, &line, NULL))
    {
    boolean readingFrameSlipped;

    ++lineCount;
    if ((wibSizeLimit > 0) && (wibSize >= wibSizeLimit))
        errAbort("wibSizeLimit of %lld has been exceeded; you must use bedGraph's for files with more data points", wibSizeLimit);

    line = skipLeadingSpaces(line);
    /*	ignore blank or comment lines	*/
    if ((line == (char *)NULL) || (line[0] == '\0') || (line[0] == '#'))
	continue;		/*	!!! go to next line of input */

    wordCount = chopByWhite(line, words, ArraySize(words));

    if (sameWord("track",words[0]) || sameWord("browser",words[0]))
	{
	continue;	/* ignore track,browser lines if present	*/
	}
    else if (sameWord("variableStep",words[0]))
	{
	int i;
	boolean foundChrom = FALSE;
	/*	safest thing to do if we were processing anything is to
	 *	output that previous block and start anew
	 *	Future improvement could get fancy here and decide if it
	 *	is really necessary to start over, although the concept
	 *	of a line between data points on one item may use this
	 *	block behavior later to define line segments, so don't
	 *	get too quick to be fancy here.  This line behavior
	 *	implies that feature names will need to be specified to
	 *	identify the line segments that belong together.
	 */
	if (variableStep || bedData || fixedStep)
	    {
	    output_row();
	    validLines = 0;	/*	to cause reset for first offset	*/
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
	freez(&prevChromName);
	prevChromName = cloneString(chromName);
	continue;		/*	!!!  go to next input line	*/
	}
    else if (sameWord("fixedStep",words[0]))
	{
	boolean foundChrom = FALSE;
	boolean foundStart = FALSE;
	int i;

	/*	same comment as above	*/
	if (variableStep || bedData || fixedStep)
	    {
	    output_row();
	    validLines = 0;	/*	to cause reset for first offset	*/
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
	if (noOverlap && validLines && prevChromName)
	    {
	    if (sameWord(prevChromName,chromName) && (fixedStart < chromStart))
		errAbort("specified fixedStep chromStart %llu is less than expected next chromStart %llu", fixedStart, chromStart);
	    }
	variableStep = FALSE;
	bedData = FALSE;
	fixedStep = TRUE;
	freez(&prevChromName);
	prevChromName = cloneString(chromName);
	continue;		/*	!!!  go to next input line	*/
	}
    else if (wordCount == 4)
	{
	/*	while in bedData, we do not necessarily need to start a new
	 *	batch unless the chrom name is changing, since dataSpan
	 *	is always 1 for bedData.  As above, this may change in
	 *	the future if each bed line specification is talking
	 *	about a different feature.
	 */
	if (variableStep || fixedStep ||
		(bedData && ((prevChromName != (char *)NULL) &&
			differentWord(prevChromName,words[0]))))
	    {
	    output_row();
	    validLines = 0;	/*	to cause reset for first offset	*/
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
	/* the bed format coordinate system is zero relative, half-open,
	 * hence, no adjustment of bedChromStart is needed here, unlike the
	 * fixed and variable step formats which will subtract one from the
	 * incoming coordinate.
	 */
	if (bedChromStart >= bedChromEnd)
	    errAbort("Found chromStart >= chromEnd at line %lu (%llu > %llu)",
		lineCount, bedChromStart, bedChromEnd);
	if (bedChromEnd > (bedChromStart + 10000000))
	    errAbort("Limit of 10,000,000 length specification for bed format at line %lu, found: %llu)",
		lineCount, bedChromEnd-bedChromStart);
	if ((validLines > 0) && (bedChromStart < previousOffset))
	    errAbort("chrom positions not in numerical order at line %lu. previous: %llu > %llu <-current", lineCount, previousOffset, bedChromStart);
	freez(&prevChromName);
	prevChromName = cloneString(chromName);
	}

    /*	We must be in one of these data formats at this point */
    if (!(variableStep || fixedStep || bedData))
	errAbort("at the line beginning: %s, variableStep or fixedStep data declaration not found or BED data 4 column format not recognized.", words[0]); 
    if (variableStep && (wordCount != 2))
	errAbort("Expecting two words for variableStep data at line %lu, found %d",
	    lineCount, wordCount);
    if (fixedStep && (wordCount != 1))
	errAbort("Expecting one word for fixedStep data at line %lu, found %d",
	    lineCount, wordCount);
    if (bedData && (wordCount != 4))
	errAbort("Expecting four words for bed format data at line %lu, found %d",
	    lineCount, wordCount);

    ++validLines;		/*	counting good lines of data input */

    /*	Offset is the incoming specified position for this value,
     *	fixedStart has already been converted to zero
     *	relative half open
     */
    if (variableStep)
	{
	Offset = sqlLongLong(words[0]);
	Offset = BASE_0(Offset);	/* zero relative half open */
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

    /* see if this is the first time through, establish chromStart 	*/
    if (validLines == 1)
	{
	chromStart = Offset;
	verbose(2, "first offset: %llu\n", chromStart);
	}
    else if ((validLines > 1) && (Offset <= previousOffset))
	errAbort("chrom positions not in numerical order at line %lu. previous: %llu > %llu <-current", lineCount, BASE_1(previousOffset), BASE_1(Offset));

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
		BASE_1(previousOffset),BASE_0(Offset));
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
if (wibSizeLimit > 0)
	options->wibSizeLimit = wibSize;
}
