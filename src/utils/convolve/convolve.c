/*	convolve - read in a set of probabilities from a histogram
 *	calculate convolutions of those probabilities for higher order
 *	histograms
 */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include	"common.h"
#include	"hash.h"
#include	"options.h"
#include	"linefile.h"
#include	<math.h>



/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"count", OPTION_INT},
    {"html", OPTION_BOOLEAN},
    {"logs", OPTION_BOOLEAN},
    {NULL, 0}
};

static int convolve_count = 4;		/* # of times to convolve	*/
static boolean html = FALSE;		/* need neat html output	*/
static boolean logs = FALSE;		/* input data is in logs, not probs */

static void usage()
{
errAbort(
    "convolve - perform convolution of probabilities\n"
    "usage: convolve [-count=N] [-logs] \\\n\t[-html] <file with initial set of probabilities>\n"
    "\t-count=N - number of times to run convolution\n"
    "\t-logs - input data is in log base 2 format, not probabilities\n"
    "\t-html - output in html table row format"
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

#ifndef log2
/* log base two is	[regardless of the base of log() function] */
#define	log2(x)	(log(x)/log(2.0))
#endif

/*	Working in log space for probabilities and need to add
 *	two probabilities together.  This case is for logs base
 *	two, but the formula works for any base.  The trick is that if
 *	one of the probabilities is much smaller than the other it is
 *	negligible to the sum.
 *
 *	have: log0 = log(p0) and log1 = log(p1)
 *	want: l = log(p0 + p1) = log(2^log0 + 2^log1)
 *	but the problem with this is that the 2^log0 and 2^log1
 *	will lose a lot of the precision that is in log0 and log1
 *
 *	To solve:
 *
 *	let m = max(log0, log1)
 *	d = max(log0, log1) - min(log0, log1)
 *	then l = log(2^m + 2^(m-d))	(if m is log0, (m-d) = log1, etc...)
 *	= log(2^m + 2^m*2^(-d))		(x^(a+b) = x^a*x^b)
 *	= log(2^m * (1 + 2^(-d)))	(factor out 2^m)
 *	= m + log(1 + 2^(-d))		(log(a*b) = log(a) + log(b))
 */
static double addLogProbabilities(double log0, double log1)
{
double result;
double m;
double d;

if (log0 > log1) {
    m = log0;
    d = log0 - log1;
} else {
    m = log1;
    d = log1 - log0;
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
static void printHistogram(struct hash * histo, int medianBin)
{
int i;				/*	for loop counting	*/
int elCount = 0;		/*	to count the hash elements	*/
double *probabilities;		/*	will be an array of probabilities */
double *log_2;			/*	will be an array of the log_2	*/
double *inverseProbabilities;	/*	inverse of probabilities	*/
double *inverseLog_2;		/*	inverse of log_2	*/
struct hashEl *el, *elList;	/*	to traverse the hash	*/
double cumulativeProbability;	/*	to show CPD	*/
double cumulativeLog_2;		/*	to show CPD	*/

elList = hashElListHash(histo);	/*	fetch the hash as a list	*/
for (el = elList; el != NULL; el = el->next) {
    ++elCount;	/*	count hash elements	*/
}

/*	Allocate the arrays	*/
probabilities = (double *) needMem((size_t)(sizeof(double) * elCount));
log_2 = (double *) needMem((size_t) (sizeof(double) * elCount));
inverseProbabilities = (double *) needMem((size_t)(sizeof(double) * elCount));
inverseLog_2 = (double *) needMem((size_t) (sizeof(double) * elCount));

/*	Traverse the list again, this time placing all values in the
 *	arrays
 */
for (el = elList; el != NULL; el = el->next)
    {
    struct histoGram *hg;	/*	histogram hash element	*/
    hg = el->val;
    probabilities[hg->bin] = hg->prob;
    log_2[hg->bin] = hg->log_2;
    }
hashElFreeList(&elList);

cumulativeProbability = 0.0;
cumulativeLog_2 = -500.0;	/*	arbitrarily small number	*/

/*	compute the inverse  P(V<=v)	*/
for (i = elCount-1; i >= 0; --i)
    {
    if (i == (elCount-1))
	{
    	cumulativeLog_2 = log_2[i];
	} else {
	cumulativeLog_2 = addLogProbabilities(cumulativeLog_2, log_2[i]);
	}
    if (cumulativeLog_2 > 0.0) cumulativeLog_2 = 0.0;
    inverseLog_2[i] = cumulativeLog_2;
    inverseProbabilities[i] = pow(2.0,cumulativeLog_2);
    }

printf("Histogram with %d bins:\n", elCount);
/*	Now the array is an ordered list	*/
for (i = 0; i < elCount; ++i)
    {
    if (i == 0)
	{
    	cumulativeLog_2 = log_2[i];
	} else {
	cumulativeLog_2 = addLogProbabilities(cumulativeLog_2, log_2[i]);
	}
    if (cumulativeLog_2 > 0.0) cumulativeLog_2 = 0.0;
    cumulativeProbability  = pow(2.0,cumulativeLog_2);
    if (html)
	{
	if (medianBin == i)
	    printf("<TR><TH> %d <BR> (median) </TH>\n", i);

	printf("\t<TD ALIGN=RIGHT> %% %.2f </TD><TD ALIGN=RIGHT> %.4g </TD>\n\t<TD ALIGN=RIGHT> %% %.2f </TD><TD ALIGN=RIGHT> %.4f </TD>\n\t<TD ALIGN=RIGHT> %% %.2f </TD><TD ALIGN=RIGHT> %.4f </TD></TR>\n",
		100.0 * probabilities[i],
		log_2[i], 100.0 * cumulativeProbability,
		cumulativeLog_2, 100.0 * inverseProbabilities[i], inverseLog_2[i]);
	} else {
	printf("bin %d: %% %.2f %0.6g\t%% %.2f\t%.6g\t%% %.2f\t%.6g", i,
		100.0 * probabilities[i], log_2[i],
		100.0 * cumulativeProbability,
		cumulativeLog_2, 100.0 * inverseProbabilities[i], inverseLog_2[i]);
	if (medianBin == i)
	    printf(" - median\n");
	else
	    printf("\n");
	}
    }
}	/*	printHistogram()	*/

/*	one iteration of convolution
 *
 *	input is a probability histogram with N bins
 *		bins are numbered 0 through N-1
 *	output is a probability histogram with (N*2)-1 bins
 *
 *	The value to calculate in an output bin is a sum
 *	of the probabilities from the input bins that contribute to that
 *	output bin.  Specifically:
 *
 *	output[0] = input[0] * input[0]
 *	output[1] = (input[0] * input[1]) +
 *			(input[1] * input[0])
 *	output[2] = (input[0] * input[2]) +
 *			(input[1] * input[1]) +
 *			(input[2] * input[0])
 *	output[3] = (input[0] * input[3]) +
 *			(input[1] * input[2]) +
 *			(input[2] * input[1]) +
 *			(input[3] * input[0])
 *	output[4] = (input[0] * input[4]) +
 *			(input[1] * input[3]) +
 *			(input[2] * input[2]) +
 *			(input[3] * input[1]) +
 *			(input[4] * input[0])
 *	the pattern here is that the bin numbers from the input are
 *	summed together to get the bin number of the output.  The
 *	probabilities from those bins used from the input in each
 *	individual sum are multiplied together, and all those resulting
 *	multiplies are added together for the resulting output.
 *
 *	A double for() loop would do the above calculation:
 *	for (i = 0; i < N; ++i) {
 *		for (j = 0; j < N; ++j) {
 *			output[i+j] += input[i] * input[j];
 *		}
 *	}
 */
static int iteration(struct hash *input, struct hash *output)
{
struct hashEl *el, *elList;
int median = 0;
double maxLog = -1000000.0;

/*	This outside loop is like (i = 0; i < N; ++i)	*/
elList = hashElListHash(input);
for (el = elList; el != NULL; el = el->next)
    {
    struct histoGram *hg0;	/*	a hash element	*/
    struct hashEl *elInput;
    int bin0;

    /*	This inside loop is like (j = 0; j < N; ++j)	*/
    hg0 = el->val;
    bin0 = hg0->bin;
    for (elInput = elList; elInput != NULL; elInput = elInput->next)
	{
	struct histoGram *hg1;	/*	a hash element	*/
	int bin1;
	char binName[128];
	struct hashEl *el;
	struct histoGram *hgNew;

	hg1 = elInput->val;
	bin1 = hg1->bin;
	/*	output bin name is (i+j) == (bin0+bin1)	*/
	snprintf(binName, sizeof(binName), "%d", bin0+bin1);
	el = hashLookup(output, binName);
	/*	start new output bin if it does not yet exist	*/
	if (el == NULL) {
	    AllocVar(hgNew);
	    hgNew->bin = bin0+bin1;
	    hgNew->prob = hg0->prob * hg1->prob;
	    hgNew->log_2 = hg0->log_2 + hg1->log_2;
	    hashAdd(output, binName, hgNew);
	} else {	/*	Add to existing output bin the new inputs */
	    hgNew = el->val;
	    hgNew->log_2 =
		addLogProbabilities(hgNew->log_2, (hg0->log_2+hg1->log_2));
	    hgNew->prob = pow(2.0,hgNew->log_2);
	}
	/*	Keep track of the resulting output median	*/
	if (hgNew->log_2 > maxLog)
	    {
	    maxLog = hgNew->log_2;
	    median = hgNew->bin;
	    }
	}
    }
hashElFreeList(&elList);

return median;
}	/*	iteration()	*/

/*	convolve() - perform the task on the input data
 *	I would like to rearrange this business here, and instead of
 *	reading in the data and leaving it in the hash for all other
 *	routines to work with, it would be best to get it immediately
 *	into an array.  That makes the work of the other routines much
 *	easier.
 */
static void convolve(int argc, char *argv[])
{
int i;
struct lineFile *lf;			/* for line file utilities	*/

for (i = 1; i < argc; ++i)
    {
    int lineCount = 0;			/* counting input lines	*/
    char *line = (char *)NULL;		/* to receive data input line	*/
    char *words[128];			/* to split data input line	*/
    int wordCount = 0;			/* result of split	*/
    struct hash *histo0;	/*	first histogram	*/
    struct hash *histo1;	/*	second histogram	*/
    int medianBin0 = 0;		/*	bin at median for histo0	*/
    double medianLog_2 = -500.0;	/*	log at median	*/
    int bin = 0;		/*	0 to N-1 for N bins	*/
    int convolutions = 0;	/*	loop counter for # of convolutions */

    histo0 = newHash(0);

    lf = lineFileOpen(argv[i], TRUE);	/*	input file	*/
    verbose(1, "Processing %s\n", argv[1]);
    while (lineFileNext(lf, &line, NULL))
	{
	int j;			/*	loop counter over words	*/
	int inputValuesCount = 0;
	struct histoGram *hg;	/*	an allocated hash element	*/

	++lineCount;
	chopPrefixAt(line, '#'); /* ignore any comments starting with # */
	if (strlen(line) < 3)	/*	anything left on this line ? */
	    continue;		/*	no, go to next line	*/
	wordCount = chopByWhite(line, words, 128);
	if (wordCount < 1)
warn("Expecting at least a word at line %d, file: %s, found %d words",
	lineCount, argv[i], wordCount);
	if (wordCount == 128)
warn("May have more than 128 values at line %d, file: %s", lineCount, argv[i]);

	verbose(2, "Input data read from file: %s\n", argv[i]);
	for (j = 0; j < wordCount; ++j)
	    {
	    char binName[128];
	    double dataValue;
	    double probInput;
	    double log_2;
	    dataValue = strtod(words[j], NULL);
	    ++inputValuesCount;
	    if (logs)
		{
		log_2 = dataValue;
		probInput = pow(2.0,log_2);
		} else {
		if (dataValue > 0.0)
		    {
		    log_2 = log2(dataValue);
		    probInput = dataValue;
		    } else {
		    log_2 = -500.0;	/*	arbitrary limit	*/
		    probInput = pow(2.0,log_2);
		    }
		}
	    if (log_2 > medianLog_2)
		{
		medianLog_2 = log_2;
		medianBin0 = bin;
		}
	    verbose(2, "bin %d: %g %0.5g\n",
		    inputValuesCount-1, probInput, log_2);

	    AllocVar(hg);	/*	the histogram element	*/
	    hg->bin = bin;
	    hg->prob = probInput;
	    hg->log_2 = log_2;
	    snprintf(binName, sizeof(binName), "%d", hg->bin);
	    hashAdd(histo0, binName, hg);

	    ++bin;
	    }	/*	for each word on an input line	*/
	}	/*	for each line in a file	*/

	/*	file read complete, echo input	*/
	if (verboseLevel() >= 2)
	    printHistogram(histo0, medianBin0);

	/*	perform convolutions to specified count
	 *	the iteration does histo0 with itself to produce histo1
	 *	Then histo0 is freed and histo1 copied to it for the
	 *	next loop.
	 */
	for (convolutions = 0; convolutions < convolve_count; ++convolutions)
	    {
	    int medianBin;
	    histo1 = newHash(0);
	    medianBin = iteration(histo0, histo1);
	    if (verboseLevel() >= 2)
		printHistogram(histo1, medianBin);
	    freeHashAndVals(&histo0);
	    histo0 = histo1;
	    }

    }		/*	for each input file	*/
}	/*	convolve()	*/


int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);

if(argc < 2)
    usage();

convolve_count = optionInt("count", 4);
logs = optionExists("logs");
html = optionExists("html");

if (verboseLevel() >= 2)
    {
    printf("options: -verbose, input file(s):\n");
    printf("-count=%d\n", convolve_count);
    printf("data input is in %s format\n", logs ? "log" : "probability");
    if (html) printf ("output in html format\n");
    }

convolve(argc, argv);

return(0);
}
