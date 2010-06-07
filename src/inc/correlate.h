/* correlate - calculate r, also known as Pearson's correlation
 * coefficient.  r*r has the nice property that it explains
 * how much of one variable's variation can be explained as
 * a linear function of the other variable's variation. */
#ifndef CORRELATE_H
#define CORRELATE_H

struct correlate
/* This helps manage correlations. */
    {
    struct correlate *next;	/* Next in list */
    double sumX;		/* Sum of all X's so far. */
    double sumY;		/* Sum of all Y's so far. */
    double sumXY;		/* Sum of all X*Y */
    double sumXX;		/* Sum of all X*X */
    double sumYY;		/* Sum of all Y*Y */
    long long n;		/* Number of samples. */
    };

struct correlate *correlateNew();
/* Return new correlation handler. */

void correlateFree(struct correlate **pC);
/* Free up correlator. */

void correlateNext(struct correlate *c, double x, double y);
/* Add next sample to correlation. */

void correlateNextMulti(struct correlate *c, double x, double y, int count);
/* Do same thing as calling correlateNext with x and y count times. */

double correlateResult(struct correlate *c);
/* Returns correlation (aka R) */

double correlateArrays(double *x, double *y, int size);
/* Return correlation of two arrays of doubles. */

#endif /* CORRELATE_H */

