/* smoothWindow - run a smoothing window over data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

int winSize = 20000;	/*	window size	*/

void usage()
/* Explain usage and exit. */
{
errAbort(
  "smoothWindow - run a smoothing window over data\n"
  "usage:\n"
  "   smoothWindow [-winSize=N] stdin\n"
  "options:\n"
  "   -winSize=N - use specified window size, default 20,000\n"
  "   -verbose=N - set verbose level [1-3]\n"
  "   two column data input expected on stdin: position and value\n"
  "   only works with stdin at this time, no file arguments."
  );
}

static struct optionSpec options[] = {
   {"winSize", OPTION_INT},
   {NULL, 0},
};

void smoothWindow()
/* smoothWindow - run a smoothing window over data. */
{
unsigned long long lineCount = 0;
struct lineFile *lf;
char *line = (char *) NULL;
boolean firstValue = TRUE;	/*	first line, or gap reset indicator */
double *smoothArray;		/*	data values in rolling window	*/
unsigned long long *offsetArray;	/*	offsets to data values  */
double rollingSum = 0.0;	/*	our current window sum	*/
int itemCount = 0;		/*	out current item count in window */
int valueIn = 0;	/*	rolling window goes from valueIn */
int valueOut = 0;	/*	to valueOut	*/
int lastIn = 0;		/*	to remember the wrap-around situation	*/
int lastOut = 0;	/* window will wrap around in the accumulator array */
int i;			/*	general purpose loop counter	*/

lf = lineFileOpen("stdin", TRUE);

/*	This doesn't need to be winSize here since the incoming data is
 *	NOT one data point for every position, but in the worst case
 *	of one data point for every position, this will take care of it all.
 */
smoothArray = (double *)needMem((size_t) (sizeof (double) * winSize));
offsetArray = (unsigned long long *)
	needMem((size_t) (sizeof (unsigned long long) * winSize));

for (i = 0; i < winSize; ++i)
    {
    smoothArray[i] = 0.0;
    offsetArray[i] = 0;
    }

while (lineFileNext(lf, &line, NULL))
    {
    unsigned long long Offset;
    char *words[2];
    int wordCount;
    char *valEnd;
    char *val;
    double dataValue;

    ++lineCount;
    chopPrefixAt(line, '#'); /* ignore any comments starting with # */
    if (strlen(line) < 3)   /*      anything left on this line ? */
	continue;           /*      no, go to next line     */
    wordCount = chopByWhite(line, words, 2);
    if (wordCount < 2)
	errAbort("Expecting at least two words at line %llu, found %d",
	    lineCount, wordCount);
    Offset = atoll(words[0]);
    val = words[1];
    dataValue = strtod(val, &valEnd);
    if ((*val == '\0') || (*valEnd != '\0'))
	errAbort("Not a valid float at line %d: %s\n", lineCount, words[1]);
    if (Offset < 1)
	errAbort("Illegal offset: %llu at line %d, dataValue: %g", Offset,
	    lineCount, dataValue);
    verbose(3, "#\tline: %llu, offset: %llu, data: %g\n",
	lineCount, Offset, dataValue);
    if (Offset > (offsetArray[lastIn] + winSize))
	{
	verbose(2, "#\tgap at line: %llu %llu\t%g\titems: %d\n",
		lineCount, Offset, dataValue, itemCount);
	firstValue = TRUE;
	if (itemCount > 0)
	    {
	    verbose(3,"#\t%llu\t%g = %g / %d to %llu = length %llu\n",
		offsetArray[lastOut], (rollingSum / (double)itemCount),
		rollingSum, itemCount, offsetArray[lastIn],
		offsetArray[lastIn] - offsetArray[lastOut] );
	    printf("%llu\t%g\n",
		offsetArray[lastOut], (rollingSum / (double)itemCount));
	    }
	valueIn = 0;
	lastIn = 0;
	rollingSum = 0.0;
	itemCount = 0;
	}
    if (firstValue)
	{
	firstValue = FALSE;
	smoothArray[valueIn] = dataValue;
	offsetArray[valueIn] = Offset;
	lastIn = valueIn;
	itemCount = 1;
	rollingSum = dataValue;
	valueIn = 1;
	valueOut = 0;
	lastOut = 0;
	verbose(3, "#\tfirst data point at line: %llu %llu\t%g\n",
	    lineCount, Offset, dataValue);
	}
    else
	{
	if (Offset >= (offsetArray[lastOut] + winSize))
	    {
	verbose(3, "#\tmoving up from %d due to offset %llu\n",
	    lastOut, Offset);
	    if (itemCount > 0)
		{
		verbose(3,"#\t%llu\t%g = %g / %d to %llu = length %llu\n",
		    offsetArray[lastOut], (rollingSum / (double)itemCount),
		    rollingSum, itemCount, offsetArray[lastIn],
		    offsetArray[lastIn] - offsetArray[lastOut] );
		printf("%llu\t%g\n",
		    offsetArray[lastOut], (rollingSum / (double)itemCount));
		}
	    while ((itemCount > 0) &&
			(Offset >= (offsetArray[lastOut] + winSize)))
		{
		rollingSum -= smoothArray[valueOut];
		--itemCount;
		++valueOut;
		if (valueOut >= winSize)
		    valueOut = 0;
		lastOut = valueOut;
		}
	    }
	verbose(3, "#\tadding at line: %llu %llu\t%g\titems: %d\n",
	    lineCount, Offset, dataValue, itemCount);
	smoothArray[valueIn] = dataValue;
	offsetArray[valueIn] = Offset;
	lastIn = valueIn;
	++itemCount;
	rollingSum += dataValue;
	++valueIn;
	if (valueIn >= winSize)
	    valueIn = 0;
	}
    }
if (itemCount > 0)
    {
    printf("%llu\t%g = %g / %d to %llu = length %llu\n", offsetArray[lastOut],
	(rollingSum / (double)itemCount), rollingSum, itemCount,
	offsetArray[lastIn], offsetArray[lastIn] - offsetArray[lastOut] );
    }
lineFileClose(&lf);
verbose(1, "#\tlines processed: %llu\n", lineCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc < 2)
	usage();

winSize = optionInt("winSize", 20000);

verbose(1, "#\twindow size: %d\n", winSize);

smoothWindow();

return 0;
}
