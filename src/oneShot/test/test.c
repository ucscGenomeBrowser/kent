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


static int trimGapPenalty(int hGap, int nGap, char *iStart, char *iEnd, int orientation)
/* Calculate gap penalty for routine below. */
{
int penalty =  ffCalcGapPenalty(hGap, nGap, ffCdna);
if (hGap > 2 || nGap > 2)	/* Not just a local extension. */
				/* Score gap to favor introns. */
    {
    penalty <<= 1;
    if (nGap > 0)	/* Intron gaps are not in n side at all. */
	 penalty += 3;
    			/* Good splice sites give you bonus 2,
			 * bad give you penalty of six. */
    penalty += 6 - 2*ffScoreIntron(iStart[0], iStart[1], 
    	iEnd[-2], iEnd[-1], orientation);
    }
return penalty;
}



void test1(int hGap, int nGap)
/* test - Test something. */
{
char *consensus = "agtag";

// printf("logBaseTwo(1000) %d, log(1000) %f\n", digitsBaseTwo(1000),  log(1000));
printf("%d %d: orig %d, old %d, new %d\n", 
	hGap, nGap, ffCalcGapPenalty(hGap, nGap, ffCdna) + 4,
	2*ffCalcGapPenalty(hGap, nGap, ffCdna) + 4,
	trimGapPenalty(hGap, nGap, consensus, consensus+4, 1)
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
