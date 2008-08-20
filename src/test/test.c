/* Test.c */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nt4.h"
#include "wormdna.h"
#include "cda.h"
#include "crudeali.h"
#include "fuzzyFind.h"
#include "htmshell.h"
#include "cheapcgi.h"

/* Function: EVDDistribution()
 * Date:     SRE, Tue Nov 18 08:02:22 1997 [St. Louis]
 * 
 * Purpose:  Returns the extreme value distribution P(S < x)
 *           evaluated at x, for an EVD controlled by parameters
 *           mu and lambda.
 */
double EVDDistribution(double x, double mu, double lambda)
{
  return (exp(-1. * exp(-1. * lambda * (x - mu))));
}

double poisson(int x, double lambda)
{
double fac = 1.0;
double pow = lambda;
int i;
for (i=2; i<=x; ++i)
    fac *= i;
for (i=1; i<x; ++i)
    pow *= lambda;
return exp(-lambda) * pow / fac; 
}

double simpleGaussean(double x)
{
return 1.0 / sqrt(2.0*3.14159) * exp(-0.5*x*x );
}

double gaussean(double x, double mean, double sd)
{
x -= mean;
return 1.0 / (sd * sqrt(2.0*3.14159)) * exp(-0.5*x*x / (sd * sd));
}

double complexGaussean(double x, double mean, double sd)
{
return simpleGaussean((x-mean)/sd)/sd;
}

void doGraph(double mu, double lambda, double n)
{
struct tempName gifTn;
struct memGfx *mg;
int pixWidth = 400, pixHeight = 300;
int virtWidth = 100, virtHeight = 100;
int i;
int y;
double virtX;
double virtY;
double virtScaleX = (double)virtWidth/pixWidth;
double virtScaleY = (double)pixHeight/virtHeight;

mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);

for (i=0; i<pixWidth; ++i)
    {
    virtX = i *virtScaleX;
//    virtY = EVDDistribution(virtX, mu, lambda) * n;
//    virtY = poisson((int)virtX, lambda) * n;
    virtY = n * gaussean(virtX, mu, lambda);
    y = (int)(virtScaleY*virtY);
    mgPutDot(mg, i, pixHeight-1-y, MG_BLACK);
    }
/* Save out picture and tell html file about it. */
for (i=0; i<10; ++i)
    {
    printf("simple %f complex %f<BR>\n", gaussean(i, 7.0, 3.0),
        complexGaussean(i, 7.0, 3.0));
    }
makeTempName(&gifTn, "trk", ".gif");
mgSaveGif(mg, gifTn.forCgi);
printf(
    "<P><IMAGE SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d NAME = \"mouse\" ALIGN=BOTTOM><BR>\n",
    gifTn.forHtml, pixWidth, pixHeight);

mgFree(&mg);
}

void doMiddle()
{
double mu = cgiDouble("mu");
double lambda = cgiDouble("lambda");
double n = cgiDouble("n");

doGraph(mu, lambda, n);
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
if (argc == 2 && sameWord(argv[1], "test") )
    {
    doGraph(1.0, 2.0, 10.0);
    }
else
    {
    htmShell("Test Output", doMiddle, "QUERY");
    }
return 0;
}

