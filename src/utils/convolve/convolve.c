/*	convolve - read in a set of probabilities from a histogram
 *	calculate convolutions of those probabilities for higher order
 *	histograms
 */
#include	"common.h"
#include	"hash.h"
#include	"options.h"
#include	"linefile.h"
#include	<math.h>


static char const rcsid[] = "$Id: convolve.c,v 1.5 2003/10/08 23:05:21 hiram Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"count", OPTION_INT},
    {"html", OPTION_BOOLEAN},
    {"logs", OPTION_BOOLEAN},
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};

static int convolve_count = 4;		/* # of times to convolve	*/
static boolean html = FALSE;		/* need neat html output	*/
static boolean logs = FALSE;		/* input data is in logs, not probs */
static boolean verbose = FALSE;		/* describe what happens	*/

static void usage()
{
errAbort(
    "convolve - perform convolution of probabilities\n"
    "usage: convolve [-count=N] [-logs] [-html] <file with initial set of probabilities>\n"
    "\t-count=N - number of times to run convolution\n"
    "\t-logs - input data is in log base 2 format, not probabilities\n"
    "\t-html - output in html table row format\n"
);
}

/*	the histograms will be kept on a hash list.
 *	Definition of one element of that list
 */
struct histoGram
    {
    int bin;
    double prob;
    double log_2;
    };

/* log base two is	[regardless of the base of log() function] */
#define	log2(x)	(log(x)/log(2.0))

/*	Working in log space for probabilities and need to add
 *	two probabilities together.  This case is for logs base
 *	two, but the formula works for any base.  The trick is that if
 *	one of the probabilities is much smaller than the other it is
 *	negligible to the sum.
 *
 *	have: l0 = log(p0) and l1 = log(p1)
 *	want: l = log(p0 + p1) = log(2^l0 + 2^l1)
 *	but the problem with this is that the 2^l0 and 2^l1
 *	will lose a lot of the precision that is in l0 and l1
 *
 *	To solve:
 *
 *	let m = max(l0, l1)
 *	d = max(l0, l1) - min(l0, l1)
 *	then l = log(2^m + 2^(m-d))	(if m is l0, (m-d) = l1, etc...)
 *	= log(2^m + 2^m*2^(-d))		(x^(a+b) = x^a*x^b)
 *	= log(2^m * (1 + 2^(-d)))	(factor out 2^m)
 *	= m + log(1 + 2^(-d))		(log(a*b) = log(a) + log(b))
 */
static double addLogProbabilities( double l0, double l1 )
{
    double result;
    double m;
    double d;

    if( l0 > l1 )
	{
	m = l0;
	d = l0 - l1;
	} else {
	m = l1;
	d = l1 - l0;
	}
    result = m + log2(1 + pow(2.0,-d));
    return(result);
}	/*	addLogProbabilities()	*/

/*	the printHistorgram needs to order the entries from the hash
 *	by bin number since the hash is not necessarily ordered.
 *	Thus, we first count them, then allocate an array for all,
 *	read them into the array, and then traversing the array they
 *	are ordered.  bin numbers are assumed to run from 0 to N-1
 *	for N bins.
 */
static void printHistogram( struct hash * histo, int medianBin )
{
int i;				/*	for loop counting	*/
int elCount = 0;		/*	to count the hash elements	*/
double * probabilities;		/*	will be an array of probabilities */
double * log_2;			/*	will be an array of the log_2	*/
struct hashEl *el, *elList;	/*	to traverse the hash	*/
double cumulativeProbability;	/*	to show CPD	*/
double cumulativeLog_2;		/*	to show CPD	*/

elList = hashElListHash(histo);	/*	fetch the hash as a list	*/
for( el = elList; el != NULL; el = el->next )
    {
		++elCount;	/*	count hash elements	*/
    }

/*	Allocate the arrays	*/
probabilities = (double *) needMem( (size_t) (sizeof(double) * elCount) );
log_2 = (double *) needMem( (size_t) (sizeof(double) * elCount) );

/*	Traverse the list again, this time placing all values in the
 *	arrays
 */
for( el = elList; el != NULL; el = el->next )
    {
    struct histoGram * hg;	/*	histogram hash element	*/
    hg = el->val;
    probabilities[hg->bin] = hg->prob;
    log_2[hg->bin] = hg->log_2;
    }
hashElFreeList(&elList);

cumulativeProbability = 0.0;
cumulativeLog_2 = -500.0;	/*	arbitrarily small number	*/

printf( "Histogram with %d bins:\n", elCount );
/*	Now the array is an ordered list	*/
for( i = 0; i < elCount; ++i )
    {
    double inverseProbability;
    double inverseLog_2;

    cumulativeLog_2 = addLogProbabilities( cumulativeLog_2, log_2[i] );
    cumulativeProbability  = pow(2.0,cumulativeLog_2);
    inverseProbability = 1.0 - cumulativeProbability;
    inverseLog_2 = log2(inverseProbability);
    if( html )
	{
	if( medianBin == i )
	    printf("<TR><TH> %d <BR> (median) </TH>\n", i);

	printf("\t<TD ALIGN=RIGHT> %% %.2f </TD><TD ALIGN=RIGHT> %.4g </TD>\n\t<TD ALIGN=RIGHT> %% %.2f </TD><TD ALIGN=RIGHT> %.4f </TD>\n\t<TD ALIGN=RIGHT> %% %.2f </TD><TD ALIGN=RIGHT> %.4f </TD></TR>\n",
		100.0 * probabilities[i],
		log_2[i], 100.0 * cumulativeProbability,
		cumulativeLog_2, 100.0 * inverseProbability, inverseLog_2);
	} else {
	printf("bin %d: %% %.2f %0.6g\t%% %.2f\t%.6g\t%% %.2f\t%.6g", i,
		100.0 * probabilities[i], log_2[i],
		100.0 * cumulativeProbability,
		cumulativeLog_2, 100.0 * inverseProbability, inverseLog_2);
	if( medianBin == i )
	    printf(" - median\n");
	else
	    printf("\n");
	}
    }
}

/*	one iteration of convolution
 *	input bins with probabilities and logs of them are histo0
 *	the resulting convolution is returned in histo1
 */

static int iteration( struct hash * histo0, struct hash * histo1 )
{
struct hashEl *el, *elList;
int median = 0;
double maxLog = -1000000.0;

elList = hashElListHash(histo0);
for( el = elList; el != NULL; el = el->next )
	    {
	    struct histoGram * hg0;	/*	a hash element	*/
	    struct hashEl *el1;
	    int bin0;

	    hg0 = el->val;
	    bin0 = hg0->bin;
	    for( el1 = elList; el1 != NULL; el1 = el1->next )
		{
		struct histoGram * hg1;	/*	a hash element	*/
		int bin1;
		char binName[128];
		struct hashEl *el;
		struct histoGram * hgNew;


		hg1 = el1->val;
		bin1 = hg1->bin;
		snprintf(binName, sizeof(binName), "%d", bin0+bin1 );
		el = hashLookup(histo1, binName);
		if( el == NULL )
		    {
			AllocVar(hgNew);
			hgNew->bin = bin0+bin1;
			hgNew->prob = hg0->prob * hg1->prob;
			hgNew->log_2 = hg0->log_2 + hg1->log_2;
			hashAdd(histo1, binName, hgNew);
		    } else {
			hgNew = el->val;
hgNew->log_2 = addLogProbabilities( hgNew->log_2, (hg0->log_2+hg1->log_2) );
			hgNew->prob = pow(2.0,hgNew->log_2);
		    }
		    if( hgNew->log_2 > maxLog )
			{
			maxLog = hgNew->log_2;
			median = hgNew->bin;
			}
		}
	    }
hashElFreeList(&elList);

return median;
}	/*	iteration()	*/

static void convolve( int argc, char *argv[] )
{
int i;
struct lineFile *lf;			/* for line file utilities	*/

for( i = 1; i < argc; ++i )
    {
    int lineCount = 0;			/* counting input lines	*/
    char *line = (char *) NULL;		/* to receive data input line	*/
    char *words[128];			/* to split data input line	*/
    int wordCount = 0;			/* result of split	*/
    struct hash * histo0;	/*	first histogram	*/
    struct hash * histo1;	/*	second histogram	*/
    int medianBin0 = 0;		/*	bin at median for histo0	*/
    int medianBin1 = 0;		/*	bin at median for histo1	*/
    double medianLog_2 = -500.0;	/*	log at median	*/
    int bin = 0;		/*	0 to N-1 for N bins	*/
    int convolutions = 0;	/*	loop counter for # of convolutions */

    if( verbose ) printf("translating file: %s\n", argv[i]);

    histo0 = newHash(0);

    lf = lineFileOpen(argv[i], TRUE);	/*	input file	*/
    while (lineFileNext(lf, &line, NULL))
	{
	int j;			/*	loop counter over words	*/
	int inputValuesCount = 0;
	struct histoGram * hg;	/*	an allocated hash element	*/

	++lineCount;
	chopPrefixAt(line, '#'); /* ignore any comments starting with # */
	if( strlen(line) < 3 )	/*	anything left on this line ? */
	    continue;		/*	no, go to next line	*/
	wordCount = chopByWhite(line, words, 128);
	if( wordCount < 1 )
warn("Expecting at least a word at line %d, file: %s, found %d words",
	lineCount, argv[i], wordCount);
	if( wordCount == 128 )
warn("May have more than 128 values at line %d, file: %s", lineCount, argv[i]);

	for( j = 0; j < wordCount; ++j )
	    {
	    char binName[128];
	    double dataValue;
	    double probInput;
	    double log_2;
	    dataValue = strtod(words[j], NULL);
	    ++inputValuesCount;
	    if( logs )
		{
		log_2 = dataValue;
		probInput = pow(2.0,log_2);
		} else {
		if( dataValue > 0.0 )
		    {
		    log_2 = log2(dataValue);
		    probInput = dataValue;
		    } else {
		    log_2 = -500.0;	/*	arbitrary limit	*/
		    probInput = pow(2.0,log_2);
		    }
		}
	    if( log_2 > medianLog_2 )
		{
		medianLog_2 = log_2;
		medianBin0 = bin;
		}
	    if( verbose ) printf("bin %d: %g %0.5g\n",
		    inputValuesCount-1, probInput, log_2);

	    AllocVar(hg);	/*	the histogram element	*/
	    hg->bin = bin;
	    hg->prob = probInput;
	    hg->log_2 = log_2;
	    snprintf(binName, sizeof(binName), "%d", hg->bin );
	    hashAdd(histo0, binName, hg );

	    ++bin;
	    }	/*	for each word on an input line	*/
	}	/*	for each line in a file	*/

	/*	file read complete, echo input	*/
	if( verbose )
	    printHistogram(histo0, medianBin0);

	/*	perform convolutions to specified count
	 *	the iteration does histo0 with itself to produce histo1
	 *	Then histo0 is freed and histo1 copied to it for the
	 *	next loop.
	 */
	for( convolutions = 0; convolutions < convolve_count; ++convolutions )
	    {
	    int medianBin;
	    histo1 = newHash(0);
	    medianBin = iteration( histo0, histo1 );
	    if( verbose )
		printHistogram(histo1, medianBin);
	    freeHashAndVals(&histo0);
	    histo0 = histo1;
	    }

    }		/*	for each input file	*/

return;
}	/*	convolve()	*/


int main( int argc, char *argv[] )
{
int i;

optionInit(&argc, argv, optionSpecs);

if(argc < 2)
    usage();

convolve_count = optionInt("count", 4);
verbose = optionExists("verbose");
logs = optionExists("logs");
html = optionExists("html");

if( verbose )
    {
    printf("options: -verbose, input file(s):\n" );
    printf("-count=%d\n", convolve_count );
    printf("data input is in %s format\n", logs ? "log" : "probability" );
    if( html ) printf ("output in html format\n");
    }

convolve(argc, argv);

return(0);
}
