/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fuzzyFind.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test hGap nGap\n"
  "options:\n"
  );
}

static int trimGapPenalty(int hGap, int nGap)
/* Calculate gap penalty for routine below. */
{
int penalty;
if (hGap <=2 && nGap <= 2)
    penalty = hGap + nGap;
else if (hGap < 25)
    penalty = 5 + 2*sqrt(hGap+nGap);
else
    {
    penalty = 1.4*log(hGap + nGap + 1) + 3;
    if (nGap > 0)
	penalty += 4;
    if (hGap > 400000)	/* Discourage really long introns. */
	{
	penalty += (hGap - 400000)/3000;
	if (hGap > ffIntronMax)
	    penalty += (hGap - ffIntronMax)/2000;
	}
    }
return penalty;
}


void test1(int hGap, int nGap)
/* test - Test something. */
{
// printf("logBaseTwo(1000) %d, log(1000) %f\n", digitsBaseTwo(1000),  log(1000));
printf("%d %d: orig %d, old %d, new %d\n", 
	hGap, nGap, ffCalcGapPenalty(hGap, nGap, ffCdna) + 4,
	2*ffCalcGapPenalty(hGap, nGap, ffCdna) + 4,
	trimGapPenalty(hGap, nGap)
	);
}

void test(int hGap, int nGap)
/* test - Test something. */
{
int n,h;
for (n = 0;  n<=1; n++)
    for (h=1; h<=1500000; h *= 2)
        test1(h, n);
    
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
test(atoi(argv[1]), atoi(argv[2]));
return 0;
}
