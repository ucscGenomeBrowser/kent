/* wigAsciiToBinary - convert ascii Wiggle data to binary file
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
 *	indication.
 *
 *	Future ideas need to account for very large holes in the
 *	data.  There is no fundamental problem with this because it is
 *	only necessary to specify were data is, it is not necessary to
 *	specify where all the NO_DATA is.  I'm guessing some kind of
 *	threshold of hole size will trigger skipping holes.  There will
 *	need to be a file naming convention to go along with that.
 *
 *	This process is related to the data load problem because the
 *	conversion to binary data needs to relate to the coordinates
 *	that will be loaded into the database.  I believe this process
 *	will become part of the database load function.  Right now it is
 *	just an exercise to see how to use Jim's libraries to read data,
 *	organize it and re-write to a second file.
 *
 *	Besides the binary data file, this will also create a database
 *	bed-like row definition file to be used to load the track.
 *
 *	When we run into a string of missing data that is bigger than
 *	the binsize, that is time to start skipping rows for the missing
 *	data
 */
#include	"common.h"
#include	"options.h"
#include	"linefile.h"

#define	NO_DATA	128

static char const rcsid[] = "$Id:";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"offset", OPTION_LONG_LONG},
    {"binsize", OPTION_LONG_LONG},
    {"dataSpan", OPTION_LONG_LONG},
    {"chrom", OPTION_STRING},
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};

static long long add_offset = 0;
static long long binsize = 1024;
static long long dataSpan = 1;
static boolean verbose = FALSE;
static char * chrom = (char *) NULL;

static void usage()
{
errAbort(
    "wigAsciiToBinary - convert ascii Wiggle data to binary file\n"
    "usage: wigAsciiToBinary [-offset=N] -chrom=chrN [-verbose] <file names>\n"
    "\t-offset=N - add N to all coordinates, default 0\n"
    "\t-binsize=N - # of points per database row entry, default 1024\n"
    "\t-dataSpan=N - # of bases spanned for each data point, default 1\n"
    "\t-chrom=chrN - this data is for chrN\n"
    "\t-verbose - display process while underway\n"
    "\t<file names> - list of files to process\n"
    "If the name of the input files are of the form: chrN.<....> this will\n"
    "\tset the chrom name.  Otherwise use the -chrom option.\n"
    "Each ascii file is a two column file.  Whitespace separator\n"
    "First column of data is a chromosome location.\n"
    "Second column is data value for that location, range [0:127]\n"
);
}

#define OUTPUT_ROW \
    chromEnd = chromStart + (bincount * dataSpan) - 1; \
    fprintf( wigout, "%s\t%llu\t%llu\t%s_%llu_%lluwiggle\t%llu\t%llu\n", \
	chromName, chromStart, chromEnd, chromName, rowCount, \
	dataSpan, fileOffsetBin, bincount ); \
    ++rowCount; \
    bincount = 0;	/* to count up to binsize	*/ \
    chromStart = Offset + dataSpan; \
    fileOffsetBin = fileOffset;

void wigAsciiToBinary( int argc, char *argv[] ) {
int i = 0;
struct lineFile *lf;
char *line = (char *) NULL;
int wordCount = 0;
char *words[2];
int lineCount = 0;
unsigned long long previousOffset = 0;	/* for data missing detection */
unsigned long long bincount = 0;	/* to count binsize for wig file */
unsigned long long Offset = 0;
off_t fileOffset = 0;
off_t fileOffsetBin = 0;
int score = 0;
char *binfile = (char *) NULL;	/*	file name of binary data file	*/
char *wigfile = (char *) NULL;	/*	file name of wiggle database file */
FILE *binout;	/*	file handle for binary data file	*/
FILE *wigout;	/*	file handle for wiggle database file	*/
unsigned long long rowCount = 0;	/* to count rows output */
char *chromName = (char *) NULL;	/* pseudo bed-6 data to follow */
unsigned long long chromStart = 0;
unsigned long long chromEnd = 0;
char strand = '+';			/* may never use - strand ? */

for( i = 1; i < argc; ++i )
    {
    if( verbose ) printf("translating file: %s\n", argv[i]);
    /*	Name mangling to determine output file name */
    if( chrom )
	{
	binfile = addSuffix(chrom, ".wib");
	wigfile = addSuffix(chrom, ".wig");
	chromName = cloneString(chrom);
	} else {
	if( startsWith("chr",argv[i]) )
	    {
	    char *tmpString;
	    tmpString = cloneString(argv[i]);
	    chopSuffix(tmpString);
	    binfile = addSuffix(tmpString, ".wib");
	    wigfile = addSuffix(tmpString, ".wig");
	    chromName = cloneString(tmpString);
	    freeMem(tmpString);
	    } else {
errAbort("Can not determine output file name, no -chrom specified\n");
	    }
	}
    if( verbose ) printf("output files: %s, %s\n", binfile, wigfile);
    rowCount = 0;	/* to count rows output */
    bincount = 0;	/* to count up to binsize	*/
    fileOffset = 0;
    fileOffsetBin = 0;
    binout = mustOpen(binfile,"w");
    wigout = mustOpen(wigfile,"w");
    lf = lineFileOpen(argv[i], TRUE);
    while (lineFileNext(lf, &line, NULL))
	{
	++lineCount;
	wordCount = chopByWhite(line, words, 2);
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
	/* see if this is the first time through, establish chromStart 	*/
	if( lineCount == 1 )
	    {
	    chromStart = Offset;
	    chromEnd = chromStart;
	    }
	/*	See if data is being skipped	*/
	if( Offset > (previousOffset + dataSpan) )
	    {
	    unsigned long long off;
	    int no_data = NO_DATA;
	    if( verbose )
printf("missing data offsets: %llu - %llu\n",previousOffset+1,Offset-1);
	    /*	fill missing data with NO_DATA indication	*/
	    for( off = previousOffset + dataSpan; off < Offset; ++off )
		{
		fputc(no_data,binout);
		++fileOffset;
		++bincount;	/*	count scores in this bin */
		if( bincount >= binsize ) break;
		}
		if( bincount >= binsize )
		{
		OUTPUT_ROW;
		}
	    }
	fputc(score,binout);	/*	output a valid byte	*/
	++fileOffset;
	++bincount;	/*	count scores in this bin */
	/*	Is it time to output a row definition ? */
	if( bincount >= binsize )
	    {
	    OUTPUT_ROW;
	    }
	previousOffset = Offset;
        }
    /*	Any data points left in this bin ?	*/
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
    if( verbose ) printf("read %d lines in file %s\n", lineCount, argv[i] );
    }
return;
}

int main( int argc, char *argv[] ) {
optionInit(&argc, argv, optionSpecs);

if(argc < 2)
    usage();

/*	the add_offset will only be useful when this turns into the
 *	database load function.  It has no value here.
 */
add_offset = optionLongLong("verbose", 0);
binsize = optionLongLong("binsize", 1024);
dataSpan = optionLongLong("dataSpan", 1);
chrom = optionVal("chrom", NULL);
verbose = optionExists("verbose");

if( verbose ) {
    printf("options: -verbose, offset= %llu, binsize= %llu, dataSpan= %llu\n",
	add_offset, binsize, dataSpan);
    if( chrom ) {
	printf("-chrom=%s\n", chrom);
    }
}
wigAsciiToBinary(argc, argv);
exit(0);
}
