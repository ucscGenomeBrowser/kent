/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "sample.h"



void usage() 
{
errAbort("normalizeSampleFiles - calculates average value over a series of\n"
	 "sample files and sets the average of each sample file to the global\n"
	 "average. Optionally will also group together samples into larger groups.\n"
	 "usage:\n\t"
	 "normalizeSampleFiles <minVal - int> <maxVal - int> <doNorm {TRUE,FALSE}> <files>\n");
}

int countGoodSamples(struct sample *sList, int minVal, int maxVal)
{
struct sample *s = NULL;
int goodCount = 0;
int i =0;
for(s = sList; s != NULL; s = s->next)
    {
    for(i=0; i< s->sampleCount; i++)
	{
	if(s->sampleHeight[i] > minVal && s->sampleHeight[i] < maxVal)
	    goodCount++;
	}
    }
return goodCount;
}

float calculateAverage(struct sample *sList, int minVal, int maxVal, int numSamples)
{
struct sample *s = NULL;
float average = 0;
int i =0;
for(s = sList; s != NULL; s = s->next)
    {
    for(i=0; i< s->sampleCount; i++)
	{
	if(s->sampleHeight[i] > minVal && s->sampleHeight[i] < maxVal)
	    average += (float)s->sampleHeight[i]/(float)numSamples;
	}
    }
return average;
}


void normalizeSamples(struct sample *sList, float normVal)
{
struct sample *s;
float sum;
int i;
assert(normVal);
for(s = sList; s != NULL; s = s->next)
    {
    sum = 0;
    for(i=0; i<s->sampleCount; i++)
	{
	s->sampleHeight[i] = s->sampleHeight[i]/normVal;
	if(s->sampleHeight[i] < 1)
	    s->sampleHeight[i] = 1;
	sum += s->sampleHeight[i];
	}
    s->score = (int)(sum/s->sampleCount);
    if(s->score < 1) 
	s->score = 1;
    }
}

void normalizeSampleFiles(int minVal, int maxVal, boolean doNorm, char *files[], int numFiles)
/* Top level function to do nomalization. */
{
float ave = 0;
int i = 0;
int numSamples = 0;
char buff[10*strlen(files[0])];
struct sample *sList = NULL, *s = NULL;
float normVal = 0;
FILE *out = NULL;
for(i=0; i<numFiles; i++)
    {
    float currentAve = 0;
    sList = sampleLoadAll(files[i]);
    numSamples = countGoodSamples(sList, minVal, maxVal);
    currentAve = calculateAverage(sList, minVal, maxVal, numSamples);
    printf("Average val for %s is %f\n", files[i], currentAve);
    sampleFreeList(&sList);
    ave += currentAve;
    }
ave = ave/numFiles;
printf("Global Average is: %f\n", ave);
if(doNorm)
    {
    printf("Normalizing files.");
    for(i=0; i<numFiles; i++)
	{
	float currentAve = 0;
	fprintf(stdout, ".");
	fflush(stdout);
	sList = sampleLoadAll(files[i]);
	numSamples = countGoodSamples(sList, minVal, maxVal);
	currentAve = calculateAverage(sList, minVal, maxVal, numSamples);
	normVal = currentAve/ave;
	normalizeSamples(sList, normVal);
	snprintf(buff, sizeof(buff), "%s.norm", files[i]);
	out = mustOpen(buff, "w");
	for(s=sList; s != NULL; s = s->next)
	    sampleTabOut(s,out);
	carefulClose(&out);
	sampleFreeList(&sList);
	}
    fprintf(stdout, "\tDone.\n");
    }

}

int main(int argc, char *argv[])
{
int maxVal, minVal;
boolean doNorm = FALSE;
if(argc < 5)
    usage();
minVal = atoi(argv[1]);
maxVal = atoi(argv[2]);
if(sameString(argv[3], "TRUE"))
    doNorm = TRUE;
normalizeSampleFiles(minVal, maxVal, doNorm, argv+4, argc-4);
return 0;
}
