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

double blatOneSensitivity(double m, int w, int k)
/* Probability blat finds hit given n=1. */
{
int t = w/k;
double p = power(m, k);
double notP = 1.0-p;
return 1.0 - power(notP, t);
}

double blatSensitivity(double m, int w, int k, int n)
/* Probability that blat finds a hit. */
{
if (n == 1)
    return blatOneSensitivity(m, w, k);
else
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
}


double blatSpecificity(int k, int n, double genomeSize, double querySize, 
    double winSize, int alphabetSize)
{
double a = 1.0/alphabetSize;
double p = power(a, k);
double f1 = p*querySize*genomeSize/k;
double s = 1 - power((1 - p), winSize/k);
double total = f1;
int i;

for (i=1; i<n; ++i)
    total *= s;
return total;
}

double exonerateSensitivity(double m, double w, int k)
/* Probability that exonerate finds a hit. */
{
int t = w/k;
double p = power(m, k) + k*power(m, k-1)*(1.0-m);
double pTotal = 1.0 - power((1.0 - p), t);
return pTotal;
}

double exonerateSpecificity(int k, double genomeSize, double querySize, double alphabetSize)
/* Return number of false positives. */
{
double a = 1.0/alphabetSize;
double p = power(a, k) + k*power(a, k-1)*(1.0-a);
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


void oldBlatTheory()
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
    blatSpec = blatSpecificity(4, blatNum, 6.0e9, 600/3, 100, 20);
    exonSens = exonerateSensitivity(dnaConservation, w, exonerateWin);
    exonSpec = exonerateSpecificity(exonerateWin, 6.0e9, 600, 4);
    printf("%3d       %4.2f    %5.1f     %4.2f    %5.1f\n",
    	w, 100*exonSens, exonSpec, 100*blatSens, blatSpec);
    }
}

double p1(double m, int k, double h)
/* Probability of single tile perfect match. */
{
double t = (double)h/(double)k;
double p = pow(m, k);
return 1  - pow((1-p), t);
}

double f1(double q, double g, double k, double a)
/* Number of single tile false positives. */
{
return q*(g/k)*pow(a,-k);
}



void blatTheory2()
/* blatTheory - Calculate some theoretical blat statistics. */
{
int k,m,n;

 for (m=85; m<99; m += 1)
    {
//    printf("%d%% ", m);
    for (n=2; n<=3; ++n)
	for (k=3; k<=6; ++k)
//	    printf("%1.3f ", blatSensitivity(m*0.01, 100.0/3, k, n));
	printf("%1.0f ", blatSpecificity(k, n, 3e9, 500.0/3, 100, 20));
    printf("\n");
    }
}

void blatTheory3()
/* blatTheory - Calculate some theoretical blat statistics. */
{
int w,k,m,n;
int s = 20, e = 150;

printf("5\t");
/* Probability of single tile perfect match. */
for (w=s; w<=e; w += 10)
    printf("%1.3f ", p1(0.89, 5, w/3));
printf("\n");
printf("2x4\t");
for (w=s; w<=e; w += 10)
    printf("%1.3f ", blatSensitivity(0.89, w/3, 4, 2));
printf("\n");
printf("12-1\t");
for (w=s; w<=e; w += 10)
    printf("%1.3e ", exonerateSensitivity(0.86, w, 12));
printf("\n");
printf("8-1\t");
for (w=s; w<=e; w += 10)
    printf("%1.3f ", exonerateSensitivity(0.89, w/3, 8));
printf("\n");
}

void blatTheory4()
/* blatTheory - Calculate some theoretical blat statistics. */
{
int k;

for (k=5; k<12; ++k)
    printf("%4d ", k);
printf("\n");
for (k=5; k<12; ++k)
    printf("%1.3f ", blatSensitivity(0.86, 100.0, k, 2));
printf("\n");
for (k=5; k<12; ++k)
    printf("%4.0f ", blatSpecificity(k, 2, 6.0e9, 600, 100, 4));
printf("\n");
}

void printBlatSensitivityTable(
	double minPercId, double maxPercId, double incPercId,
	int minK, int maxK, int h, int n, boolean oneOff)
/* Print sensitivity table. */
{
double id;
int k;
boolean firstTime = FALSE;

if (oneOff && n > 1)
    errAbort("Not set up for oneOff calcs for N > 1");
for (k = minK; k <= maxK; k += 1)
    {
    printf("\t");
    printf("%d", k);
    }
printf("\n");
for (id=minPercId; id<= maxPercId; id += incPercId)
    {
    printf("%2.0f%%\t", 100*id);
    for (k = minK; k <= maxK; k += 1)
	{
	double sens;
	if (oneOff)
	    sens = exonerateSensitivity(id, h, k);
	else
	    sens = blatSensitivity(id, h, k, n);
	printf("%0.3f", sens);
	if (k == maxK)
	    printf("\n");
	else
	    printf("\t");
	}
    }
}

	
void printSpecificityTable(int minK, int maxK, int n,
	double genomeSize, double querySize, int alphabetSize,
	boolean oneOff)
/* Print a table of specificities. */
{
int k;
int winSize = (alphabetSize > 4 ? 100 : 100);
printf("K");
for (k = minK; k <= maxK; k += 1)
    {
    printf("\t");
    printf("%d", k);
    }
printf("\n");

printf("F");
for (k = minK; k <= maxK; k += 1)
    {
    double s;
    printf("\t");
    if (oneOff)
	s = exonerateSpecificity(k, genomeSize, querySize, alphabetSize);
    else
	s = blatSpecificity(k, n, genomeSize, querySize, winSize, alphabetSize);
    if (s < 10)
        printf("%1.1f", s);
    else if (s <= 999999)
	printf("%1.0f", s);
    else
	printf("%1.1e", s);
    if (k == maxK)
	printf("\n");
    }
}

void blatTheory()
/* blatTheory - Calculate some theoretical blat statistics. */
{
double x;

printf("Single Perfect Nucleotide Matches\n");
printBlatSensitivityTable(0.81, 0.975, 0.02, 7, 14, 100, 1, FALSE);
printf("Specificity\n");
printSpecificityTable(7, 14, 1, 3.0e9, 500, 4, FALSE);
printf("\n");
printf("\n");

printf("Single Perfect Amino Acid Matches\n");
printBlatSensitivityTable(0.71, 0.94, 0.02, 3, 7, 33, 1, FALSE);
printf("Specificity\n");
printSpecificityTable(3, 7, 1, 2*3.0e9, 167, 20, FALSE);
printf("\n");
printf("\n");

printf("Near Perfect Nucleotide Matches\n");
printBlatSensitivityTable(0.81, 0.975, 0.02, 12, 22, 100, 1, TRUE);
printf("Specificity\n");
printSpecificityTable(12, 22, 1, 3.0e9, 500, 4, TRUE);
printf("\n");
printf("\n");

printf("Near Perfect Amino Acid Matches\n");
printBlatSensitivityTable(0.71, 0.94, 0.02, 4, 9, 33, 1, TRUE);
printf("Specificity\n");
printSpecificityTable(4, 9, 1, 2*3.0e9, 167, 20, TRUE);
printf("\n");
printf("\n");

printf("Two Perfect Nucleotide Matches\n");
printBlatSensitivityTable(0.81, 0.975, 0.02, 8, 12, 100, 2, FALSE);
printf("Specificity\n");
printSpecificityTable(8, 12, 2, 3.0e9, 500, 4, FALSE);
printf("\n");
printf("\n");

printf("Three Perfect Nucleotide Matches\n");
printBlatSensitivityTable(0.81, 0.975, 0.02, 8, 12, 100, 3, FALSE);
printf("Specificity\n");
printSpecificityTable(8, 12, 3, 3.0e9, 500, 4, FALSE);
printf("\n");
printf("\n");

printf("Two Perfect Amino Acid Matches\n");
printBlatSensitivityTable(0.71, 0.94, 0.02, 3, 7, 33, 2, FALSE);
printf("Specificity\n");
printSpecificityTable(3, 7, 2, 2*3.0e9, 167, 20, FALSE);
printf("\n");
printf("\n");

printf("Three Perfect Amino Acid Matches\n");
printBlatSensitivityTable(0.71, 0.94, 0.02, 3, 7, 33, 3, FALSE);
printf("Specificity\n");
printSpecificityTable(3, 7, 3, 2*3.0e9, 167, 20, FALSE);
printf("\n");
printf("\n");

printf("Some Mouse/Human possibilities.");
printf("\tK\tF\tF translated\n");
printf("1 perfect-DNA\t");
printf("7\t%0.0f\n", blatSpecificity(7, 1, 3.0e9, 500, 100, 4));
printf("1 Perfect-AA\t");
x = blatSpecificity(5, 1, 2*3.0e9, 167, 100, 20);
printf("5\t%0.0f\t%0.0f\n", x, 3*x);
printf("Near-perfect-DNA\t");
printf("12\t%0.0f\n", exonerateSpecificity(12, 3.0e9, 500, 4));
printf("Near-perfect-AA\t");
x = exonerateSpecificity(8, 2*3.0e9, 167, 20);
printf("8\t%0.0f\t%0.0f\n", x, 3*x);
printf("2 perfect-DNA\t");
printf("6\t%0.0f\n", blatSpecificity(6, 2, 3.0e9, 500, 100, 4));
printf("2 Perfect-AA\t");
x = blatSpecificity(4, 2, 2*3.0e9, 167, 100, 20);
printf("5\t%0.0f\t%0.0f\n", x, 3*x);
printf("3 perfect-DNA\t");
printf("5\t%0.0f\n", blatSpecificity(5, 3, 3.0e9, 500, 100, 4));
printf("2 Perfect-AA\t");
x = blatSpecificity(3, 3, 2*3.0e9, 167, 100, 20);
printf("3\t%0.0f\t%0.0f\n", x, 3*x);
}

int main(int argc, char *argv[])
/* Process command line. */
{
blatTheory();
return 0;
}
