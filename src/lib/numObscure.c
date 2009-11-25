/* numObscure - obscure but sometimes useful numerical functions. */
#include "common.h"
#include "obscure.h"

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

