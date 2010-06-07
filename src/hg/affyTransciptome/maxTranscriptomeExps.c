#include "common.h"
#include "sample.h"

static char const rcsid[] = "$Id: maxTranscriptomeExps.c,v 1.4 2008/09/03 19:18:12 markd Exp $";


void usage()
{
errAbort("maxTranscriptomeExps - cycle through a list of of affy transcriptome\n"
	 "experiments and select the max for each position.\n"
	 "usage:\n\t"
	 "maxTranscriptomeExps <outputfile> <trackName> <list of input files>\n");
}


void fillInMaxVals(struct sample *samp, struct sample **pSampList, int sampCount, int index)
{
int i,j;
struct sample *s1 = NULL;
for(i=0;i<sampCount; i++)
    {
    s1 = slElementFromIx(pSampList[i], index);
    for(j=0;j<samp->sampleCount; j++)
	{
	if(!(s1->samplePosition[j] == samp->samplePosition[j]))
	    errAbort("maxTranscriptomeExps::fillInMaxVales() - for probe %s:%d-%d positions at index %i don't agree. %d, %d", 
		     samp->chrom, samp->chromStart, samp->chromEnd, i, samp->samplePosition[j], s1->samplePosition[j]);
	samp->sampleHeight[j] = max(samp->sampleHeight[j], s1->sampleHeight[j]);
	}
    }
}


void maxTranscriptomeExps(char *files[], int numFiles, char *outputFile, char *trackName)
{
struct sample **pSampList = NULL;
struct sample *s1;
struct sample *maxListSamp  = NULL;
struct sample *maxSamp = NULL;
int i;
int count =0;
FILE *out = NULL;
AllocArray(pSampList, numFiles);
for(i=0;i<numFiles; i++)
 {
 warn("Reading %s.", files[i]);
 pSampList[i] = sampleLoadAll(files[i]);
 }

warn("Calculating Maxes.");
count = slCount(pSampList[0]);
for(i=0;i<count;i++)
    {
    AllocVar(maxSamp);
    s1 = slElementFromIx(pSampList[0], i);
    maxSamp->chrom = cloneString(s1->chrom);
    maxSamp->chromStart = s1->chromStart;
    maxSamp->chromEnd = s1->chromEnd;
    maxSamp->name = cloneString(trackName);
    snprintf(maxSamp->strand, sizeof(maxSamp->strand), "%s", s1->strand);
    maxSamp->sampleCount = s1->sampleCount;
    maxSamp->samplePosition = CloneArray(s1->samplePosition, maxSamp->sampleCount);
    AllocArray(maxSamp->sampleHeight, maxSamp->sampleCount);
    fillInMaxVals(maxSamp, pSampList, numFiles, i);
    slAddHead(&maxListSamp, maxSamp);
    }
slReverse(&maxListSamp);
warn("Saving Maxes");
out = mustOpen(outputFile, "w");
for(maxSamp = maxListSamp; maxSamp != NULL; maxSamp = maxSamp->next)
    {
    sampleTabOut(maxSamp, out);
    }
carefulClose(&out);
warn("Cleaning up");
sampleFreeList(&maxListSamp);
for(i=0;i<numFiles; i++)
    sampleFreeList(&pSampList[i]);
freez(&pSampList);
warn("Done.");
}

int main(int argc, char *argv[])
{
if(argc < 4)
    usage();
else
    maxTranscriptomeExps(argv+3, argc-3, argv[1], argv[2]);
return 0;
}

	 
