/* blatTheory - Calculate some theoretical blat statistics. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blatTheory - Calculate some theoretical blat statistics\n"
  "usage:\n"
  "   blatTheory XXX\n");
}

double factorial(int n)
{
int i;
double tot = 1.0;

for (i=1; i<=n; ++i)
    tot *= i;
return tot;
}

double choose(int m, int n)
{
return factorial(n)/(factorial(m) * factorial(n-m));
}

double power(double x, int y)
{
int i;
double tot = 1.0;

for (i=1; i<=y; ++i)
    tot *= x;
return tot;
}

double blatSensitivity(double m, int w, int k, int n)
/* Probability that blat finds a hit. */
{
int t = w/k;
double p = power(m, k);
int i;
double pn;
double pTotal = 0;

for (i = n; i<= t; ++i)
    {
    pn = choose(i, t) * power(p, i) * power(1.0-p, t-i);
    pTotal += pn;
    }
return pTotal;
}

double blatSpecificity(int k, int n, double genomeSize, double querySize)
{
double p = power(0.05, k);
double f1 = p*querySize*genomeSize/k;
double total = f1;
int i;

for (i=1; i<n; ++i)
    total *= 300*p;
return total;
}

double exonerateSensitivity(double m, int w, int k)
/* Probability that exonerate finds a hit. */
{
int t = w/k;
double p = power(m, k) + k*power(m, k-1)*(1.0-m);
double pTotal = 1.0 - power((1.0 - p), t);
return pTotal;
}

double exonerateSpecificity(int k, double genomeSize, double querySize)
/* Return number of false positives. */
{
double p = power(0.25, k) + k*power(0.25, k-1)*(1.0-0.25);
return p*genomeSize*querySize/k;
}


void printBlat(double m, int w, int k, int n)
/* Print blat probability .*/
{
double pTotal = blatSensitivity(m, w, k, n);
printf("blat sensitivity %f (m=%f, w=%d, k=%d, n=%d)\n",
	pTotal, m, w, k, n);
}

void printExonerate(double m, int w, int k)
/* Print exonerate hit probability given parameters. */
{
double pTotal = exonerateSensitivity(m, w, k);
printf("exonerate sensitivity %f (m=%f, w=%d, k=%d)\n",
	pTotal, m, w, k);
}


void blatTheory()
/* blatTheory - Calculate some theoretical blat statistics. */
{
int k;
int n;
int w;
double m;
double blatSens, blatSpec, exonSens, exonSpec;
double dnaConservation = 0.862, aaConservation = 0.89;
int blatNum = 2, exonerateWin = 14;

printf("homology     exonerate            blat      \n");
printf("  size    hit%%     false pos   hit%%     false pos\n");
for (w=10; w<=150; w += 10)
    {
    blatSens = blatSensitivity(aaConservation, w/3, 4, blatNum);
    blatSpec = blatSpecificity(4, blatNum, 6.0e9, 600/3);
    exonSens = exonerateSensitivity(dnaConservation, w, exonerateWin);
    exonSpec = exonerateSpecificity(exonerateWin, 6.0e9, 600);
    printf("%3d       %4.2f    %5.1f     %4.2f    %5.1f\n",
    	w, 100*exonSens, exonSpec, 100*blatSens, blatSpec);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
blatTheory();
return 0;
}
