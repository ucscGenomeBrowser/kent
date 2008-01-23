/* hmmerFace.c - Interface between hmmer2 stuff and my stuff -jk */

#include "common.h"
#include "squid.h"
#include "config.h"
#include "structs.h"
#include "funcs.h"

#include "globals.h"

float random(float maxVal)
{
return (float)(rand())*maxVal/RAND_MAX;
}

void testHistogram()
{
struct histogram_s *hist;
int fit;
int i,j;
float acc;

uglyf("Testing histogram\n");
hist = AllocHistogram(0, 20, 2);

for (i=0; i<1000; ++i)
    {
    acc = 0.0f;
    for (j=0; j<3; ++j)
        acc += random(5.0f);
    AddToHistogram(hist, acc);
    } 

// fit= GaussianFitHistogram(hist, 20.0f);
GaussianSetHistogram(hist, 8.0f, 3.0f);
printf("Fit = %d\n", fit);

PrintASCIIHistogram(stdout, hist);
FreeHistogram(hist);
}
