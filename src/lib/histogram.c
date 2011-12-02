/* histogram function for data array in memory	*/

#include "common.h"
#include "histogram.h"


static unsigned autoScale(float *values, size_t N, float *binSize,
	unsigned *binCount, float *minValue, float *min, float *max)
/*	determine binSize, binCount, minValue for values[N]
 *	If any of those are given, use them instead of calculating.
 *	A given minValue means ignore data below that.
 *	NAN's for binSize or minValue are the signals to not use them.
 *	non-zero for binCount to use it.  NOTE: binCount is actually one
 *	too high to get the minimum and maximum values in the first and
 *	last (binCount-1) bins correctly.
 */
{
float minFound = INFINITY;
float maxFound = -1.0 * INFINITY;
float range = 0.0;
unsigned count = 0;
unsigned bins = DEFAULT_BIN_COUNT;
size_t i;
boolean findMinMax = FALSE;

if ( (*min == 0.0) && (*max == 0.0) )
    findMinMax = TRUE;
else
    {
    minFound = *min;
    maxFound = *max;
    }

if (isnan(*minValue))
    {				/*	minValue is not specified	*/
    for (i = 0; i < N; ++i)
	{
	if (!isnan(values[i]))
	    {
	    if (findMinMax)
		{
		++count;
		if (values[i] < minFound) minFound = values[i];
		if (values[i] > maxFound) maxFound = values[i];
		}
	    else
		{
		if ( (values[i] < *max) && (values[i] > *min) ) ++count;
		}
	    }
	}
    }
else
    {
    minFound = *minValue;		/*	use given minValue	*/
    for (i = 0; i < N; ++i)
	{
	if ((!isnan(values[i])) && (values[i] >= minFound))
	    {
	    if (findMinMax)
		{
		++count;
		if (values[i] > maxFound) maxFound = values[i];
		}
	    else
		{
		if ( (values[i] < *max) && (values[i] > *min) ) ++count;
		}
	    }

	}
    }

if (count > 0)
    {
    /*	if the caller asked us to find min,max, return them	*/
    if (findMinMax)
	{
	*min = minFound;
	*max = maxFound;
	}

    /*	If the caller did not specify a minValue, return it	*/
    if (isnan(*minValue))
	*minValue = minFound;

    range = maxFound - minFound;

    /*	if they gave us a binCount, use it	*/
    if (*binCount > 0)
	bins = *binCount;
    else
	*binCount = bins;

    if ( (range > 0.0) && (bins > 1))
	{
	/*  binSize is calculated on (bins - 1) to allow the minimum value
	 *  to be in the middle of the first bin, and the highest value to be
	 *	in the middle of the last bin
	 */
	if (isnan(*binSize))
	    *binSize = range / (bins - 1);

	if (*binSize > 0.0)
	    return count;
	else
	    return 0;	/*	did not work	*/
	}
    }
return 0;	/*	did not work	*/
}	/*	static unsigned autoScale()	*/

void freeHistoGram(struct histoResult **histoResults)
/*      free the histoResults list	*/
{
if (histoResults && *histoResults)
    {
    struct histoResult *hr, *next;

    for (hr = *histoResults; hr; hr = next)
        {
        next = hr->next;
        freeMem(hr->binCounts);
        freeMem(hr->pValues);
        freeMem(hr);
        }
    *histoResults = NULL;
    }
}

struct histoResult *histoGram(float *values, size_t N, float binSize,
	unsigned binCount, float minValue, float min, float max,
	struct histoResult *accumHisto)
/*	construct histogram of data in values[N] array.  The extra
 *	options of binSize, binCount, minValue, min, max are optional.
 *	Run autoScaling when min == max == 0.0
 *	Defaults for binSize, binCount and minValue can be given even
 *	when auto-scaling, or NAN's for the floats, or 0 for the
 *	binCount to determine them too.
 *	When they are specified they will be used in place of auto
 *	scaled determined values.  If the min and max of the data is
 *	known, pass those in on min,max to aid the calculation of auto
 *	scaled values.  NAN's can be in the values[N] array and will be
 *	ignored.
 *	NOTE: when giving a binCount, it is actually one
 *	higher to get the minimum and maximum values in the first and
 *	last (binCount-1) bins correctly.  The resulting histogram will
 *	appear to be (binCount-1) number of bins.
 *	When given a pointer to accumHisto, use that existing histo gram
 *	and continue accumulations in it.
 */
{
float autoBinSize = NAN;	/*	pass NAN's to cause auto scaling */
float autoMinValue = NAN;
float range = 0.0;
unsigned autoBinCount = 0;
unsigned autoValueCount = 0;
boolean autoScaling = FALSE;
unsigned valueCount = 0;
unsigned i;			/*	array index	*/
struct histoResult *hr;
unsigned missed = 0;

if (N == 0)
    return NULL;	/*	we don't work on zero number of values	*/

if (accumHisto)		/*	if accumulating in existing histogram	*/
    {			/*	use its parameters as the scaling values */
    autoBinCount = accumHisto->binCount;
    autoBinSize = accumHisto->binSize;
    autoMinValue = accumHisto->binZero;
    autoScaling = FALSE;
    range = autoBinSize * (autoBinCount - 1);
    valueCount = accumHisto->count;
    }
else
    {
/*	Caller may give us a range to work within	*/
if ( (0.0 == min) && (0.0 == max) )
    autoScaling = TRUE;
else
    {
    range = max - min;
    if (range == 0.0)
	return NULL;	/*	caller gave us equal min, max !	*/
    }

/*	Caller may give us any of the binCount, binSize, minValue */
if (binCount > 1)
    autoBinCount = binCount;
else if (!autoScaling)
    autoBinCount = DEFAULT_BIN_COUNT;

if (!isnan(binSize))
    autoBinSize = binSize;
else if (!autoScaling)
    autoBinSize = range / (autoBinCount - 1);

if (!isnan(minValue))
    autoMinValue = minValue;
else if (!autoScaling)
    autoMinValue = min;

if (autoScaling)
    {
    autoValueCount = autoScale(values, N, &autoBinSize,
			&autoBinCount, &autoMinValue, &min, &max);
    if (autoValueCount == 0)
	return NULL;	/*	no result !	*/
    }
else
    autoValueCount = N;
    }

if (accumHisto)		/*	if accumulating in existing histogram	*/
    hr = accumHisto;
else
    {
    AllocVar(hr);
    AllocArray(hr->binCounts,autoBinCount);
    AllocArray(hr->pValues,autoBinCount);
    }

for (i = 0; i < N; ++i)
    {
    if (!isnan(values[i]) && (values[i] >= autoMinValue))
	{
	if ( (values[i] <= max) && (values[i] >= min) )
	    {
	    float f = values[i] - autoMinValue;
	    int inx = (int) floor(f / autoBinSize);

	    if ( (inx >= 0) && (inx < autoBinCount))
		{
		++valueCount;
		++hr->binCounts[inx];
		}
	    else
		++missed;
	    }
	    else
		++missed;
	}
	else
	    ++missed;
    }	/*	for (i = 0; i < N; ++i)	*/

if (accumHisto)		/*	if accumulating in existing histogram	*/
    hr->count = valueCount;	/*	only this is new	*/
else
    {
    hr->binSize = autoBinSize;
    hr->binCount = autoBinCount;
    hr->count = valueCount;
    hr->binZero = autoMinValue;
    }

for (i = 0; i < autoBinCount; ++i)
    {
    if (hr->binCounts[i] > 0)
	hr->pValues[i] = (float) hr->binCounts[i] / (float) valueCount;
    else
	hr->pValues[i] = 0.0;
    }

return hr;
}
