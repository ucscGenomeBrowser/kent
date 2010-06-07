#include "common.h"
#include "dnautil.h"
#include "seqStats.h"

double dnaMatchEntropy(DNA *query, DNA *target, int baseCount)
/* Return entropy of matching bases - a number between 0 and 1, with
 * higher numbers the more diverse the matching bases. */
{
#define log4 1.386294
#define invLog4 (1.0/log4)
double p, q, e = 0, invTotal;
int c, count[4], total;
int i, qVal, tVal;
count[0] = count[1] = count[2] = count[3] = 0;
for (i=0; i<baseCount; ++i)
    {
    qVal = ntVal[(int)query[i]];
    tVal = ntVal[(int)target[i]];
    if (qVal == tVal && qVal >= 0)
	count[qVal] += 1;
    }
total = count[0] + count[1] + count[2] + count[3];
invTotal = 1.0/total;
for (i=0; i<4; ++i)
    {
    if ((c = count[i]) > 0)
        {
	p = c * invTotal;
	q = log(p);
	e -= p*q;
	}
    }
e *= invLog4;
return e;
}

