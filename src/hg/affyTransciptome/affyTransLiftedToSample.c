/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "affyTransLifted.h"
#include "sample.h"
#include "hash.h"
#include "linefile.h"


void usage()
/* Give the user a hint about how to use this program. */
{
errAbort("affyTransLiftedToSample - takes a data file from Simon Cawley at\n"
	 "Affymetrix which represents the transcriptome data, lifted and normalized\n"
	 "for a new version of the human genome. File format is:\n"
	 "hs.ncbiFreeze.chrom\tchromPosition\txCoord\tyCoord\trawValPM\trawValMM\tnormValPM\tnormValMM\n"
	 "and  produces a sample file representing the  same data.\n"
	 "usage:\n\t"
	 "affyTransLiftedToSample <int grouping amount> <lifted files.....>\n");
}

char *findChromName(char *longName)
/* Find the chrXX in longName. */
{
char *s = strstr(longName, "chr");
if(s == NULL)
    errAbort("affyTransLiftedToSample::findChromName() - Can't find 'chr' in %s", longName);
return s;
}

struct sample *sampleFromAffyTransLifted(struct affyTransLifted *atl, char *name)
/* Create a sample data structure from information in the atl, free with
   sampleFree(). */
{
struct sample *s = NULL;
char *chrom = NULL;
float score = 0;
assert(atl);
AllocVar(s);
chrom = findChromName(atl->chrom);
s->chrom = cloneString(chrom);
s->chromStart = s->chromEnd = atl->chromPos;
s->name = cloneString(name);
score = (atl->normPM - atl->normMM);
if(score < 0) 
    score = 0;
s->score = score;
sprintf(s->strand, "+");
s->sampleCount = 1;
AllocArray(s->samplePosition, s->sampleCount);
s->samplePosition[0] = 0;
AllocArray(s->sampleHeight, s->sampleCount);
s->sampleHeight[0] = s->score;
return s;
}

int sampleCoordCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct sample *a = *((struct sample **)va);
const struct sample *b = *((struct sample **)vb);
int diff;
diff = strcmp(a->chrom, b->chrom);
if(diff == 0)
    diff = a->chromStart - b->chromStart;
return diff;
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
	addSampleToCurrent(currSamp, samp, grouping);
	count += samp->sampleCount;
	samp = sampNext = samp->next;
	}
    if(count != 0)
	currSamp->score = currSamp->score / count;
    currSamp->chromEnd = currSamp->chromStart + currSamp->samplePosition[count -1];
    slAddHead(&groupedList, currSamp);
    }
slReverse(&groupedList);
return groupedList;
}

void affyTransLiftedToSample(int grouping, char *affyTransIn)
/* Top level function to run combine pairs and offset files to give sample. */
{
struct affyTransLifted *atl = NULL, *atlList = NULL;
struct sample *sampList = NULL, *samp = NULL;
struct sample *groupedList = NULL;
char *fileRoot = NULL;
char buff[10+strlen(affyTransIn)];
FILE *out = NULL;
char *fileNameCopy = cloneString(affyTransIn);
chopSuffix(fileNameCopy);
fprintf(stderr, ".");
fflush(stderr);
atlList = affyTransLiftedLoadAll(affyTransIn);
//warn("Creating samples.");
for(atl = atlList; atl != NULL; atl = atl->next)
    {
    samp = sampleFromAffyTransLifted(atl, fileNameCopy);
    if(samp != NULL)
	slAddHead(&sampList, samp);
    }
//warn("Sorting Samples");
slSort(&sampList, sampleCoordCmp);
groupedList = groupByPosition(grouping, sampList);
//warn("Saving Samples.");
snprintf(buff, sizeof(buff), "%s.sample", affyTransIn);
out = mustOpen(buff, "w");
for(samp = groupedList; samp != NULL; samp = samp->next)
    {
    sampleTabOut(samp, out);
    }
//warn("Cleaning up.");
freez(&fileNameCopy);
carefulClose(&out);
sampleFreeList(&sampList);
sampleFreeList(&groupedList);
affyTransLiftedFreeList(&atlList);
}

int main(int argc, char *argv[])
{
int i;
int grouping;
if(argc < 3)
    usage();
grouping = atoi(argv[1]);
warn("starting conversion of %d files.\n", argc - 2);
for(i=2; i<argc; i++)
    affyTransLiftedToSample(grouping , argv[i]);
warn("\nfinished.");
return 0;
}
