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
 *	holes in it, those holes will be set to the "NO_DATA"
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
 */
#include	"common.h"
#include	"options.h"
#include	"linefile.h"

#define	NO_DATA	128

static char const rcsid[] = "$Id:";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"offset", OPTION_LONG_LONG},
    {"chrom", OPTION_STRING},
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};

static long long add_offset = 0;
static boolean verbose = FALSE;
static char * chrom = (char *) NULL;

static void usage()
{
errAbort(
    "wigAsciiToBinary - convert ascii Wiggle data to binary file\n"
    "usage: wigAsciiToBinary [-offset=N] -chrom=chrN [-verbose] <file names>\n"
    "\t-offset=N - add N to all coordinates\n"
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

void wigAsciiToBinary( int argc, char *argv[] ) {
int i = 0;
struct lineFile *lf;
char *line = (char *) NULL;
int wordCount = 0;
char *words[2];
int lineCount = 0;
unsigned long long previousOffset = 0;
unsigned long long Offset = 0;
int score = 0;
char *outfile = (char *) NULL;
FILE *out;

for( i = 1; i < argc; ++i )
    {
    if( verbose ) printf("translating file: %s\n", argv[i]);
    /*	Name mangling to determine output file name */
    if( chrom )
	{
	outfile = addSuffix(chrom, ".wib");
	} else {
	if( startsWith("chr",argv[i]) )
	    {
	    char *tmpString;
	    tmpString = cloneString(argv[i]);
	    chopSuffix(tmpString);
	    outfile = addSuffix(tmpString, ".wib");
	    freeMem(tmpString);
	    } else {
errAbort("Can not determine output file name, no -chrom specified\n");
	    }
	}
    if( verbose ) printf("output file: %s\n", outfile);
    out = mustOpen(outfile,"w");
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
	/*	we have no need to translate coordinates here, but this
	 *	will become important when we are loading the database
	 */
	/*Offset -= 1;	/* our coordinates are zero relative half open */
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
	if( Offset > (previousOffset + 1) )
	    {
	    unsigned long long off;
	    int no_data = NO_DATA;
	    if( verbose )
		printf("hole in data %llu - %llu\n",previousOffset+1,Offset-1);
	    /*	fill no data hole with NO_DATA indication	*/
	    for( off = previousOffset + 1; off < Offset; ++off )
		fputc(no_data,out);
	    fputc(score,out);
	    } else {
	    if( verbose && (0 == (lineCount % 1000) ))
		printf("line %d: %llu %d\n", lineCount, Offset, score );
	    fputc(score,out);
	    }
	previousOffset = Offset;
        }
    lineFileClose(&lf);
    fclose(out);
    freeMem(outfile);
    outfile = (char *) NULL;
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
chrom = optionVal("chrom", NULL);
verbose = optionExists("verbose");

if( verbose ) {
    printf("options: -verbose, offset = %llu\n", add_offset);
    if( chrom ) {
	printf("-chrom=%s\n", chrom);
    }
}
wigAsciiToBinary(argc, argv);
exit(0);
}
