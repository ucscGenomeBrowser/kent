#include "common.h"
#include "sample.h"

void usage()
{
errAbort("groupSamples - Group samples together into one sample.\n"
	 "usage:\n\t"
	 "groupSamples <gouping size - int> < input file > < output file >\n");
}

void addSampleToCurrent(struct sample *target, struct sample *samp, int grouping)
{
int i;
int base = target->sampleCount;
for(i=0;i<samp->sampleCount; i++)
    {
    assert(base+i < grouping);
    target->score += samp->score;
    target->samplePosition[base + i] = samp->samplePosition[i] + samp->chromStart - target->chromStart;
    target->sampleHeight[base + i] = samp->sampleHeight[i];
    target->sampleCount++;
    }
}

struct sample *groupByPosition(int grouping , struct sample *sampList)
{
struct sample *groupedList = NULL, *samp = NULL, *currSamp = NULL, *sampNext=NULL;
int count = 0;
for(samp = sampList; samp != NULL; samp = sampNext)
    {
    sampNext = samp->next;
    AllocVar(currSamp);
    currSamp->chrom = cloneString(samp->chrom);
    currSamp->chromStart = samp->chromStart;
    currSamp->name = cloneString(samp->name);
    snprintf(currSamp->strand, sizeof(currSamp->strand), "%s", samp->strand);
    AllocArray(currSamp->samplePosition, grouping);
    AllocArray(currSamp->sampleHeight, grouping);
    count = 0;
    while(samp != NULL && count < grouping && sameString(samp->chrom,currSamp->chrom))
	{
	if(sameString(currSamp->name, "Empty") && differentString(samp->name, "Empty"))
	    {
	    freez(&currSamp->name);
	    currSamp->name = cloneString(samp->name);
	    }
	addSampleToCurrent(currSamp, samp, grouping);
	count += samp->sampleCount;
	sampNext = samp->next;
	sampleFree(&samp);
	samp = sampNext;
	}
    if(count != 0)
	currSamp->score = currSamp->score / count;
    currSamp->chromEnd = currSamp->chromStart + currSamp->samplePosition[count -1];
    slAddHead(&groupedList, currSamp);
    }
slReverse(&groupedList);
return groupedList;
}


void groupSamples(int grouping, char  *input, char *output)
{
FILE *out = NULL;
struct sample *sampList = NULL, *groupedList = NULL, *samp = NULL;
sampList = sampleLoadAll(input);
groupedList = groupByPosition(grouping, sampList);
out = mustOpen(output, "w");
for(samp = groupedList; samp != NULL; samp = samp->next)
    {
    sampleTabOut(samp, out);
    }
carefulClose(&out);
//sampleFreeList(&sampList);
sampleFreeList(&groupedList);
}

int main(int argc, char *argv[])
{
int grouping = 0;
if(argc != 4)
    usage();
grouping = atoi(argv[1]);
groupSamples(grouping, argv[2], argv[3]);
return 0;
}
