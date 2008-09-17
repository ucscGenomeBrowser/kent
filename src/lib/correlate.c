/* correlate - calculate r, also known as Pearson's correlation
 * coefficient.  r*r has the nice property that it explains
 * how much of one variable's variation can be explained as
 * a linear function of the other variable's variation. */

#include "common.h"
#include "correlate.h"

static char const rcsid[] = "$Id: correlate.c,v 1.2 2008/09/17 17:56:37 kent Exp $";

struct correlate *correlateNew()
/* Return new correlation handler. */
{
struct correlate *c;
return AllocVar(c);
}

void correlateFree(struct correlate **pC)
/* Free up correlator. */
{
freez(pC);
}

void correlateNext(struct correlate *c, double x, double y)
/* Add next sample to correlation. */
{
c->sumX += x;
c->sumXX += x*x;
c->sumXY += x*y;
c->sumY += y;
c->sumYY += y*y;
c->n += 1; 
}

double correlateResult(struct correlate *c)
/* Returns correlation (aka R) */
{
double r = 0;
if (c->n > 0)
    {
    double sp = c->sumXY - c->sumX*c->sumY/c->n;
    double ssx = c->sumXX - c->sumX*c->sumX/c->n;
    double ssy = c->sumYY - c->sumY*c->sumY/c->n;
    double q = ssx*ssy;
    if (q != 0)
        r = sp/sqrt(q);
    }
return r;
}

double correlateArrays(double *x, double *y, int size)
/* Return correlation of two arrays of doubles. */
{
struct correlate *c = correlateNew();
double r;
int i;
for (i=0; i<size; ++i)
     correlateNext(c, x[i], y[i]);
r = correlateResult(c);
correlateFree(&c);
return r;
}

