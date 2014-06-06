/* calcGap - calculate gap scores. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


double scale = 200;
int maxGap = 100;
int near = 0;
char *sampleList = "1,2,3,4,5,6,7,8,9,10,15,20,30,40,60,80,100,150,200,300,400,500,1000,2000,5000,10000,20000,50000,100000";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "calcGap - calculate gap scores\n"
  "usage:\n"
  "   calcGap chainFile\n"
  "options:\n"
  "   -scale=N Amount to scale scores by - default 94.\n"
  "   -maxGap=N Largest gap to look at.  Default 100\n"
  "   -near=N How close to consider something 'almost' a indel\n"
  "   -samples=N,M - List of points to sample\n"
  );
}

int score(double count, double total)
/* Return probability as a score. */
{
if (count == 0)
    return 0;
return round(scale * log(count/total));
}

void getSamples(char *sampleList, int **retSamples, int *retCount)
/* Parse out comma separated list. */
{
char *words[256];
int i, wordCount = chopCommas(sampleList, words);
int *samples;
AllocArray(samples, wordCount);
for (i=0; i<wordCount; ++i)
    samples[i] = atoi(words[i]);
*retSamples = samples;
*retCount = wordCount;
}

double aveInArea(int *a, int pos)
/*  Return average value in a near position (+/5%) */
{
int before = round(pos * 0.95);
int after = round(pos * 1.05);
int size = after - before + 1;
int i, acc = 0;

for (i=before; i<=after; ++i)
    acc += a[i];
return (double)acc / size;
}

int scoreArea(int *a, int pos, double total)
/* Score region. */
{
return score(aveInArea(a, pos), total);
}

void calcGap(char *fileName)
/* calcGap - calculate gap scores. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *words[4];
int i, wordCount;
int totalGaps = 0;
int *xGaps, *yGaps, *bothGaps, *xNear, *yNear;
int *samples, sampleCount;
int maxSample;
int maxAlloc;

getSamples(sampleList, &samples, &sampleCount);
maxSample = samples[sampleCount - 1];
uglyf("%d samples max %d\n", sampleCount, maxSample);
maxAlloc = max(maxSample*1.2, maxGap);

AllocArray(xGaps, maxAlloc);
AllocArray(yGaps, maxAlloc);
AllocArray(bothGaps, maxAlloc);
AllocArray(xNear, maxAlloc);
AllocArray(yNear, maxSample);


while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (line[0] == '#' || line[0] == 0)
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 3)
        {
	int bases = lineFileNeedNum(lf, words, 0);
	int xGap = lineFileNeedNum(lf, words, 1);
	int yGap = lineFileNeedNum(lf, words, 2);
	int bothGap = xGap + yGap;
	totalGaps += bases;
	if (yGap == 0 && xGap < maxSample)
	    xGaps[xGap] += 1;
	if (xGap == 0 && yGap < maxSample)
	    yGaps[yGap] += 1;
	if (xGap != 0 && yGap != 0)
	    {
	    if ((xGap <= near && yGap <= near) || (xGap > near && yGap > near))
	        {
		if (bothGap < maxSample)
		    bothGaps[bothGap] += 1;
		}
	    else if (xGap <= near)
	        {
		if (yGap < maxSample)
		    yNear[yGap] += 1;
		}
	    else if (yGap <= near)
	        {
		if (xGap < maxSample)
		    xNear[xGap] += 1;
		}
	    else
	        {
		assert(FALSE);
		}
	    }
	}
    }
if (near)
    printf("#size: indel\tmGap\thGap\tmNear\thNear\tdouble\n");
else
    printf("#size: indel\tmGap\thGap\tdouble\n");
for (i=1; i<maxGap; ++i)
    {
    if (near)
	printf(" %4d:  %d\t%d\t%d\t%d\t%d\t%d\n", i,
	    score(xGaps[i] + yGaps[i], totalGaps),
	    score(2*xGaps[i], totalGaps), 
	    score(2*yGaps[i], totalGaps),
	    score(xNear[i], totalGaps),
	    score(yNear[i], totalGaps),
	    score(bothGaps[i], totalGaps));
    else
	printf(" %4d:  %d\t%d\t%d\t%d\n", i,
	    score((xGaps[i] + yGaps[i])/2, totalGaps),
	    score(xGaps[i], totalGaps), 
	    score(yGaps[i], totalGaps),
	    score(bothGaps[i], totalGaps));
    }
printf("\n");

printf("And selected samples:\n");
for (i=0; i<sampleCount; ++i)
    {
    int s = samples[i];
    printf(" %4d:  %d\t%d\t%d\n", s,
	scoreArea(xGaps, s, totalGaps),
	scoreArea(yGaps, s, totalGaps),
	scoreArea(bothGaps, s, totalGaps));
    }
printf("int pos[%d] = {\n", sampleCount);
for (i=0; i<sampleCount; ++i)
    printf("%d,", samples[i]);
printf("};\n");
printf("int xGap[%d] = {\n", sampleCount);
for (i=0; i<sampleCount; ++i)
    printf("%d,", scoreArea(xGaps, samples[i], totalGaps));
printf("};\n");
printf("int yGap[%d] = {\n", sampleCount);
for (i=0; i<sampleCount; ++i)
    printf("%d,", scoreArea(yGaps, samples[i], totalGaps));
printf("};\n");
printf("int bothGap[%d] = {\n", sampleCount);
for (i=0; i<sampleCount; ++i)
    printf("%d,", scoreArea(bothGaps, samples[i], totalGaps));
printf("};\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
scale = optionFloat("scale", scale);
maxGap = optionInt("maxGap", maxGap);
near = optionInt("near", near);
sampleList = optionVal("samples", cloneString(sampleList));

if (argc != 2)
    usage();
calcGap(argv[1]);
return 0;
}
