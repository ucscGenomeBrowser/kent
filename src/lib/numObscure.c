/* numObscure - obscure but sometimes useful numerical functions. */
#include "common.h"
#include "obscure.h"

static void rangeIncludingZero(double start, double end, double *retStart, double *retEnd)
/* If start == end, then make range go from zero to the start/end value. */
{
if (start < 0.0)
    {
    *retStart = start;
    *retEnd = 0;
    }
else if (start > 0.0)
    {
    *retStart = 0;
    *retEnd = end;
    }
else
    {
    *retStart = 0;
    *retEnd = 1;
    }
return;
}

void rangeRoundUp(double start, double end, double *retStart, double *retEnd)
/* Round start and end so that they cover a slightly bigger range, but with more round
 * numbers.  For instance 0.23:9.89 becomes 0:10 */
{
double size = end - start;
if (size < 0)
    errAbort("start (%g) after end (%g) in rangeRoundUp", start, end);

/* Flat ranges get moved to include zero for scale. */
if (size == 0.0)
    {
    rangeIncludingZero(start, end, &start, &end);
    size = end - start;
    }

/* Figure out "increment", which will be 1, 2, 5, or 10, or a multiple of 10 of these 
 * Want to have at least two increments in range. */
double exponent = 1;
double scaledSize = size;
double increment = 0;
while (scaledSize < 100)
    {
    scaledSize *= 10;
    exponent /= 10;
    }
while (scaledSize >= 100)
    {
    scaledSize /= 10;
    exponent *= 10;
    }
/* At this point have a number between 10 and 100 */
if (scaledSize < 12)
    increment = 1;
else if (scaledSize < 20)
    increment = 2;
else if (scaledSize < 75)
    increment = 5;
else
    increment = 10;
increment *= exponent;

int startInIncrements = floor(start/increment);
int endInIncrements = ceil(end/increment);
*retStart = startInIncrements * increment;
*retEnd = endInIncrements * increment;
}

void rangeFromMinMaxMeanStd(double minVal, double maxVal, double mean, double std,
	double *retStart, double *retEnd)
/* Given some basic statistical properties, set a range that will be good on a wide
 * range of biological data. */
{
double start,end;
if (isnan(std))
    {
    /* Handle a case that occurred in version 1 bigWigs and bigBeds due to fault in
     * sumSquares calculation. */
    start = mean-5;
    end = mean+5;
    if (start < minVal) start = minVal;
    if (end > maxVal) end = maxVal;
    }
else if (std == 0)
    {
    start = end = mean;
    rangeIncludingZero(start, end, &start, &end);
    }
else
    {
    start = mean - 5*std;
    end = mean + 5*std;
    if (start < minVal) start = minVal;
    if (end > maxVal) end = maxVal;
    }
*retStart = start;
*retEnd = end;
}

