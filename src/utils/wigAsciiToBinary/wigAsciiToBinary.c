/* wigAsciiToBinary - convert ascii Wiggle data to binary file
 *	plus, produces file read for load into database
 *
 *	The ascii data file is simply two columns, white space separated:
 *	first column: offset within chromosome, 1 relative closed end
 *	second column: score in range 0 to 127
 *
 *	scores will be truncated to the range [0:127]
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

static char const rcsid[] = "$Id: wigAsciiToBinary.c,v 1.9 2003/10/10 21:47:01 hiram Exp $";

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

/*
 *	The table rows need to be printed in a couple of different
 *	places in the code and it wouldn't work well as a subroutine,
 *	so it is a define here, which works just fine.
 *	Definition of this row format can be found in hg/inc/wiggle.h
 *	and hg/lib/wiggle.as, etc ...
 *	Going to use the score field as our first line of data
 *	compression.  It will be the maximum of the data values in this
 *	bin.  Probably do not need the strand, but will
 *	it them around in case some good use can be found later.
 */
#define OUTPUT_ROW \
    if( bincount ) { \
	chromEnd = chromStart + (bincount * dataSpan); \
	fprintf( wigout, \
"%s\t%llu\t%llu\t%s.%llu_%s\t%d\t%c\t%d\t%llu\t%llu\t%llu\t%s\n", \
	    chromName, chromStart+add_offset, chromEnd+add_offset, \
	    featureName, rowCount, spanName, maxScore, strand, minScore, \
	    dataSpan, bincount, fileOffsetBegin, binfile ); \
	++rowCount; \
	} \
	bincount = 0;	/* to count up to binsize	*/ \
	maxScore = 0;	/* new bin, start new max */ \
	minScore = 1000000;	/* new bin, start new min */ \
	chromStart = Offset + dataSpan; \
	fileOffsetBegin = fileOffset;

void wigAsciiToBinary( int argc, char *argv[] )
{
int i = 0;				/* general purpose int counter	*/
struct lineFile *lf;			/* for line file utilities	*/
char * fileName;			/* the basename of the input file */
char *line = (char *) NULL;		/* to receive data input line	*/
char *words[2];				/* to split data input line	*/
int wordCount = 0;			/* result of split	*/
int lineCount = 0;			/* counting input lines	*/
unsigned long long previousOffset = 0;	/* for data missing detection */
unsigned long long bincount = 0;	/* to count binsize for wig file */
unsigned long long Offset = 0;		/* from data input	*/
off_t fileOffset = 0;			/* where are we in the binary data */
off_t fileOffsetBegin = 0;		/* where this bin started in binary */
int score = 0;				/* from data input	*/
int maxScore = 0;			/* max score in this bin */
int minScore = 1000000;			/* min score in this bin */
char *binfile = (char *) NULL;	/*	file name of binary data file	*/
char *wigfile = (char *) NULL;	/*	file name of wiggle database file */
FILE *binout;	/*	file handle for binary data file	*/
FILE *wigout;	/*	file handle for wiggle database file	*/
unsigned long long rowCount = 0;	/* to count rows output */
char *chromName = (char *) NULL;	/* pseudo bed-6 data to follow */
unsigned long long chromStart = 0;	/* for table row data */
unsigned long long chromEnd = 0;	/* for table row data */
char strand = '+';			/* may never use - strand ? */
char spanName[64];			/* short-hand name for dataSpan */
char featureName[254];			/* the name of this feature */

/*	for each input data file	*/
for( i = 1; i < argc; ++i )
    {
    if( verbose ) printf("translating file: %s\n", argv[i]);
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
    if( name )		/*	Is the name of this feature specified ?	*/
	{
	    snprintf( featureName, sizeof(featureName) - 1, "%s", name );
	}
    if( chrom )		/*	Is the chrom name specified ? */
	{
	chromName = cloneString(chrom);
	if( ! name )	/*	that names the feature too if not already */
	    snprintf( featureName, sizeof(featureName) - 1, "%s", chrom );
	}
    /*	Name mangling to determine output file name */
    if( wibFile )	/*	when specified, simply use it	*/
	{
	binfile = addSuffix(wibFile, ".wib");
	wigfile = addSuffix(wibFile, ".wig");
	} else {	/*	not specified, construct from input names */
	if( startsWith("chr",fileName) )
	    {
	    char *tmpString;
	    tmpString = cloneString(fileName);
	    chopSuffix(tmpString);
	    binfile = addSuffix(tmpString, ".wib");
	    wigfile = addSuffix(tmpString, ".wig");
	    if( ! chrom )	/*	if not already taken care of	*/
		chromName = cloneString(tmpString);
	    if( ! name && ! chrom)	/*	if not already done	*/
		snprintf(featureName, sizeof(featureName) - 1, "%s", tmpString);
	    freeMem(tmpString);
	    } else {
errAbort("Can not determine output file name, no -wibFile specified\n");
	    }
	}

    if( verbose ) printf("output files: %s, %s\n", binfile, wigfile);
    rowCount = 0;	/* to count rows output */
    bincount = 0;	/* to count up to binsize	*/
    maxScore = 0;	/* max score in this bin */
    minScore = 0;	/* min score in this bin */
    fileOffset = 0;	/* current location within binary data file	*/
    fileOffsetBegin = 0;/* location in binary data file where this bin starts*/
    binout = mustOpen(binfile,"w");	/*	binary data file	*/
    wigout = mustOpen(wigfile,"w");	/*	table row definition file */
    lf = lineFileOpen(argv[i], TRUE);	/*	input file	*/
    while (lineFileNext(lf, &line, NULL))
	{
	++lineCount;
	chopPrefixAt(line, '#'); /* ignore any comments starting with # */
	if( strlen(line) < 3 )	/*	anything left on this line */
	    continue;		/*	no, go to next line	*/
	wordCount = chopByWhite(line, words, 2);
	if( wordCount < 2 )
errAbort("Expecting at least two words at line %d, found %d", lineCount, wordCount);
	Offset = atoll(words[0]);
	score = atoi(words[1]);
	if( Offset < 1 )
	    errAbort("Illegal offset: %llu at line %d, score: %d", Offset, 
		    lineCount, score );
	Offset -= 1;	/* our coordinates are zero relative half open */
	if( score < 0 )
	    {
warn("WARNING: truncating score %d to 0 at line %d\n", score, lineCount );
		score = 0;
	    }
	if( score > 127 )
	    {
warn("WARNING: truncating score %d to 127 at line %d\n", score, lineCount );
		score = 127;
	    }
	if( score > maxScore )
	    maxScore = score;
	if( score < minScore )
	    minScore = score;
	/* see if this is the first time through, establish chromStart 	*/
	if( lineCount == 1 )
	    chromStart = Offset;
	/* if we are working on a zoom level and the data is not exactly
	 * spaced according to the span, then we need to put each value
	 * in its own row in order to keep positioning correct for these
	 * data values.
	 */
	if( (lineCount != 1) && (dataSpan > 1) && (Offset != (previousOffset + dataSpan) ) )
	    {
	    if( verbose )
printf("data not spanning %llu bases, prev: %llu, this: %llu\n", dataSpan, previousOffset, Offset );
	    OUTPUT_ROW;
	    }
	/*	Check to see if data is being skipped	*/
	else if( Offset > (previousOffset + dataSpan) )
	    {
	    unsigned long long off;
	    unsigned long long fillSize;	/* number of bytes */
	    if( verbose )
printf("missing data offsets: %llu - %llu\n",previousOffset+1,Offset-1);
	    /*	If we are just going to fill the rest of this bin with
	     *  no data, then may as well stop here.  No need to fill
	     *  it with nothing.
	     */
	    fillSize = (Offset - (previousOffset + dataSpan)) / dataSpan;
	    if( verbose )
printf("filling NO_DATA for %llu bytes\n", fillSize );
	    if( fillSize + bincount >= binsize )
		{
		    OUTPUT_ROW;
	    if( verbose )
printf("completed a bin due to  NO_DATA for %llu bytes, only %llu - %llu = %llu to go\n", fillSize, binsize, bincount, binsize - bincount );
		} else {
		fillSize = 0;
		/*	fill missing data with NO_DATA indication	*/
		for( off = previousOffset + dataSpan; off < Offset;
			off += dataSpan )
		    {
		    ++fillSize;
		    fputc(NO_DATA,binout);
		    ++fileOffset;
		    ++bincount;	/*	count scores in this bin */
		    if( bincount >= binsize ) break;
		    }
	    if( verbose )
printf("filled NO_DATA for %llu bytes\n", fillSize );
		/*	If that finished off this bin, output it */
		    if( bincount >= binsize )
			{
			OUTPUT_ROW;
			}
	        }
	    }
	/*	With perhaps the missing data taken care of, back to the
	 *	real data.
	 */
	fputc(score,binout);	/*	output a valid byte	*/
	++fileOffset;
	++bincount;	/*	count scores in this bin */
	/*	Is it time to output a row definition ? */
	if( bincount >= binsize )
	    {
	    OUTPUT_ROW;
	    }
	previousOffset = Offset;
        }	/*	reading file input loop end	*/
    /*	Done with input file, any data points left in this bin ?	*/
    if( bincount )
	{
	OUTPUT_ROW;
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
    if( verbose )
printf("fini: %s, read %d lines, table rows: %llu, data bytes: %lld\n",
	argv[i], lineCount, rowCount, fileOffset );
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

if( verbose ) {
    printf("options: -verbose, offset= %llu, binsize= %llu, dataSpan= %llu\n",
	add_offset, binsize, dataSpan);
    if( chrom ) {
	printf("-chrom=%s\n", chrom);
    }
    if( wibFile ) {
	printf("-wibFile=%s\n", wibFile);
    }
    if( name ) {
	printf("-name=%s\n", name);
    }
}
wigAsciiToBinary(argc, argv);
exit(0);
}
