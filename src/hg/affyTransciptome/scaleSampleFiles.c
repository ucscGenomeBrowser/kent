/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "sample.h"


void usage()
{
errAbort("scaleSampleFiles - scale all of the scores in a file by a scale factor.\n"
	 "usage:\n\t"
	 "scaleSampleFiles <minVal> <maxVal> <scaleVal> <files....>\n");
}


void scaleSamples(int minVal, int maxVal, float scaleVal, char *files[], int fileCount)
{
struct sample *sampList = NULL, *samp = NULL;
FILE *out;
char buff[2048];
int i;
int j;
for(i=0;i<fileCount; i++)
    {
    warn("Scaling file %s by %f", files[i],scaleVal);
    sampList = sampleLoadAll(files[i]);
    for(samp = sampList; samp != NULL; samp = samp->next)
	{
	samp->score = (int)(scaleVal * samp->score);
	if(samp->score < minVal)
	    samp->score = minVal;
	if(samp->score > maxVal)
	    samp->score = maxVal;
	for(j =0;j<samp->sampleCount; j++ )
	    {
	    samp->sampleHeight[j] = (int)(scaleVal * samp->sampleHeight[j]);
	    if(samp->sampleHeight[j] < minVal)
		samp->sampleHeight[j] = minVal;
	    if(samp->sampleHeight[j] > maxVal)
		samp->sampleHeight[j] = maxVal;
	    }
	}
    snprintf(buff, sizeof(buff), "%s.scale", files[i]);
    out = mustOpen(buff, "w");
    for(samp= sampList; samp != NULL; samp = samp->next)
	sampleTabOut(samp, out);
    carefulClose(&out);
    sampleFreeList(&sampList);
    }
}

int main(int argc, char *argv[])
{
int minVal, maxVal;
float scaleVal;
if(argc < 5)
    usage();
minVal = atoi(argv[1]);
maxVal = atoi(argv[2]);
scaleVal = atof(argv[3]);
scaleSamples(minVal, maxVal, scaleVal, argv+4, argc-4);
return 0;
}
    
