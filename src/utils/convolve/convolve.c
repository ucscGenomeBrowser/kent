#include	"common.h"
#include	"hash.h"
#include	"options.h"
#include	"linefile.h"
#include	<math.h>


static char const rcsid[] = "$Id: convolve.c,v 1.3 2003/10/08 18:35:29 hiram Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};

static boolean verbose = FALSE;		/* describe what happens	*/

static void usage()
{
errAbort(
    "convolve - perform convolution of probabilities\n"
    "usage: convolve <file with initial set of probabilities>\n"
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

elList = hashElListHash(histo);	/*	fetch the hash as a list	*/
for( el = elList; el != NULL; el = el->next )
    {
		++elCount;	/*	count hash elements	*/
    }

/*	Allocate the arrays	*/
probabilities = (double *) needMem( (size_t) (sizeof(double) * elCount) );
log_2 = (double *) needMem( (size_t) (sizeof(double) * elCount) );

/*	Traverse the list again, this type placing all values in the
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

printf( "Histogram with %d bins:\n", elCount );
/*	Now the array is an ordered list	*/
for( i = 0; i < elCount; ++i )
    {
    if( medianBin == i )
	printf("bin %d: %.8g %0.6g\t- median\n", i, probabilities[i], log_2[i]);
    else
	printf("bin %d: %.8g %0.6g\n", i, probabilities[i], log_2[i]);
    }
}

/*	Working in log space for probabilities and need to add
 *	two probabilities together.  This case is for logs base
 *	two, but the formula works for any base.
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
 *	= log(2^m + 2^m*2^(-d))
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
    int medianBin0 = 0;	/*	bin at median for histo0	*/
    int medianBin1 = 0;	/*	bin at median for histo1	*/
    double medianProb = 0.0;	/*	probability at median	*/

    if( verbose ) printf("translating file: %s\n", argv[i]);

    histo0 = newHash(0);

    lf = lineFileOpen(argv[i], TRUE);	/*	input file	*/
    while (lineFileNext(lf, &line, NULL))
	{
	int j;			/*	loop counter over bins	*/
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
	    double probInput;
	    double log_2;
	    probInput = strtod(words[j], NULL);
	    ++inputValuesCount;
	    if( probInput > 0.0 )
		{
		    log_2 = log2(probInput);
		    if( probInput > medianProb )
			{
			medianProb = probInput;
			medianBin0 = j;
			}
		} else {
		    log_2 = -32.0;
		}
	    if( verbose ) printf("bin %d: %g %0.5g\n",
		    inputValuesCount-1, probInput, log_2);

	    AllocVar(hg);	/*	the histogram element	*/
	    hg->bin = j;
	    hg->prob = probInput;
	    hg->log_2 = log_2;
	    snprintf(binName, sizeof(binName), "%d", hg->bin );
	    hashAdd(histo0, binName, hg );

	    }	/*	for each word on an input line	*/
	}	/*	for each line in a file	*/

	/*	file read complete, echo input	*/
	if( verbose )
	    printHistogram(histo0, medianBin0);

	/*	OK, let's try a first convolution	*/
	/*	The convolution is histo0 with itself to produce histo1 */
	histo1 = newHash(0);
	medianBin1 = iteration( histo0, histo1 );
	if( verbose )
	    printHistogram(histo1, medianBin1);

	/*	And a second, histo1 with itself into histo0	*/
	freeHashAndVals(&histo0);
	histo0 = newHash(0);
	medianBin0 = iteration( histo1, histo0 );
	if( verbose )
	    printHistogram(histo0, medianBin0);

	/*	And a third, histo0 with itself into histo1	*/
	freeHashAndVals(&histo1);
	histo1 = newHash(0);
	medianBin1 = iteration( histo0, histo1 );
	if( verbose )
	    printHistogram(histo1, medianBin1);

	/*	And a fourth, histo1 with itself into histo0	*/
	freeHashAndVals(&histo0);
	histo0 = newHash(0);
	medianBin0 = iteration( histo1, histo0 );
	if( verbose )
	    printHistogram(histo0, medianBin0);

    }		/*	for each input file	*/

return;
}	/*	convolve()	*/


int main( int argc, char *argv[] )
{
int i;

optionInit(&argc, argv, optionSpecs);

if(argc < 2)
    usage();

verbose = optionExists("verbose");

if( verbose )
    {
    printf("options: -verbose, input file(s):\n" );
    for( i = 1; i < argc; ++i )
	{
	    printf("\t%s\n", argv[i] );
	}
    }

convolve(argc, argv);

return(0);
}
