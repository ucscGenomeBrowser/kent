/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "jksql.h"
#include "expRecord.h"
#include "psl.h"
#include "hash.h"
#include "bed.h"
#include "stanMad.h"



char *url = "http://genome-www.stanford.edu/nci60/";
char *reference = "D.T. Ross et al. Nature Geneteics 2000, March 24(3):227-234.";
char *credit = "D.T. Ross et al.";
char *description = "Microarray Study of NCI's 60 Cell lines.";
FILE *bedOut = NULL;
void usage() 
{ 
errAbort("stanToBedAndExpRecs - takes a pslFile of alignments and a list of stanfords\n"
	 "expression data files and converts them into a bed file with the scores and experiment\n"
	 "ids. Also creates a corresponding file of expRecords which idicate what the\n"
	 "expression ids correspond to.\n"
	 "usage:\n"
	 "\tstanToBedAndExpRecs <pslFile> <expRecOut> <bedOut> <list of expression files...>\n");
}

void bedHashOutput(void * val) 
{
struct bed *bed = val;
bedOutputN(bed, 15, bedOut, '\t', '\n');
}

void averageValues(void *val)
{
struct bed *bed = val;
int count = 0;
float sum = 0;
int i=0;
for(i=0; i<bed->expCount; i++)
    {
    if(bed->expScores[i] != -10000) 
	{
	sum = sum + bed->expScores[i];
	count++;
	}
    }
sum = (float)sum/count;
bed->score = (int)(1000 * sum);
}

struct bed *pslToBed(struct psl *psl)
/* Convert a psl format row of strings to a bed, very similar to customTrack.c::customTrackPsl*/
{
struct bed *bed;
int i, blockCount, *chromStarts, chromStart;

/* A tiny bit of error checking on the psl. */
if (psl->qStart >= psl->qEnd || psl->qEnd > psl->qSize 
    || psl->tStart >= psl->tEnd || psl->tEnd > psl->tSize)
    {
    errAbort("mangled psl format for %s", psl->qName);
    }

/* Allocate bed and fill in from psl. */
AllocVar(bed);
bed->chrom = cloneString(psl->tName);
bed->chromStart = bed->thickStart =  chromStart = psl->tStart;
bed->chromEnd = bed->thickEnd = psl->tEnd;
bed->score = 1000 - 2*pslCalcMilliBad(psl, TRUE);
if (bed->score < 0) bed->score = 0;
strncpy(bed->strand,  psl->strand, sizeof(bed->strand));
bed->blockCount = blockCount = psl->blockCount;
bed->blockSizes = (int *)cloneMem(psl->blockSizes,(sizeof(int)*psl->blockCount));
bed->chromStarts = chromStarts = (int *)cloneMem(psl->tStarts, (sizeof(int)*psl->blockCount));
bed->name = cloneString(psl->qName);

/* Switch minus target strand to plus strand. */
if (psl->strand[1] == '-')
    {
    int chromSize = psl->tSize;
    reverseInts(bed->blockSizes, blockCount);
    reverseInts(chromStarts, blockCount);
    for (i=0; i<blockCount; ++i)
	chromStarts[i] = chromSize - chromStarts[i];
    }

/* Convert coordinates to relative. */
for (i=0; i<blockCount; ++i)
    chromStarts[i] -= chromStart;
return bed;
}


void readInPslHash(struct hash *pslHash, char *file)
{
struct psl *pslList, *psl;
pslList = pslLoadAll(file);
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    hashAdd(pslHash, psl->qName, psl);
    }
}

void createBeds(struct hash *bedHash, struct hash *pslHash, char *file, int numExps)
{
struct stanMad *smList=NULL, *sm=NULL;
struct psl *psl = NULL;
struct bed *bed = NULL;
char buff[256];
warn("File is %s", file);
smList = stanMadLoadAll(file);

for(sm=smList; sm != NULL; sm = sm->next)
    {
    sprintf(buff, "%d", sm->clid);
    psl = hashFindVal(pslHash, buff);
    if(psl != NULL) 
	{
	snprintf(buff,sizeof(buff), "%d-%s-%d", sm->clid, sm->prow, sm->pcol);
	bed = pslToBed(psl);
	bed->expCount = numExps;
	bed->expIds = needMem(sizeof(int) * numExps);
	bed->expScores = needMem(sizeof(float) * numExps);
	hashAddUnique(bedHash, buff, bed);
	}
    }
}

float safeLog2(float num)
{
if(num > 0) 
    return logBase2(num);
else 
    return -10000.0;
}

struct expRecord *createExpRec(char *file, int expNum)
{
struct expRecord *er = NULL;
char * name = cloneString(file);
char *exp=NULL, *tissue=NULL;
chopSuffix(name);
chopSuffix(name);
AllocVar(er);
er->name = cloneString(name);
tissue = strrchr(name,'_');
if(tissue != NULL)
    {
    *tissue = '\0';
    tissue++;
    }
else 
    {
    tissue = "DUPLICATE";
    }

er->id = expNum;
er->description = description;
er->url = url;
er->ref = reference;
er->credit = credit;
er->numExtras = 2;
er->extras = needMem(sizeof(char *) * er->numExtras);
exp = strstr(name, "_");
if(exp != NULL)
    {
    *exp = '\0';
    }
er->extras[0] = name;
er->extras[1] = tissue;
return er;
}

void appendNewExperiment(char *file, struct hash *bedHash, struct hash *pslHash, struct expRecord **erList, int expNum)
{
struct expRecord *er = NULL;
char buff[256];
int count = 0;
struct bed *bed = NULL;
struct stanMad *smList = NULL, *sm = NULL;
smList = stanMadLoadAll(file);
er = createExpRec(file, expNum);
slAddHead(erList, er);
for(sm = smList; sm != NULL; sm = sm->next)
    {
    count++;
    snprintf(buff,sizeof(buff), "%d-%s-%d", sm->clid, sm->prow, sm->pcol);
    bed = hashFindVal(bedHash, buff);
    if(bed != NULL)
	{
	bed->expIds[expNum] = expNum;
	bed->expScores[expNum] = safeLog2(sm->rat2n);
	}
    else
	{
	if(sm->clid != 0) 
	    {
	    struct psl *psl = NULL;
	    snprintf(buff,sizeof(buff), "%d", sm->clid);
	    psl = hashFindVal(pslHash, buff);
	    if(psl != NULL)
		errAbort("Counldn't find hash entry at line %d in %s for %s, %d, %d.\n", count, file, buff, sm->clid, sm->spot);
	    }	
	}
    }

stanMadFreeList(&smList);
}

int main(int argc, char *argv[])
{
struct hash *pslHash = newHash(5);
struct hash *bedHash = newHash(5);
struct expRecord *erList = NULL, *er=NULL;

int i;
FILE *erOut = NULL;
if(argc <4) 
    usage();
warn("Reading in psls...");
readInPslHash(pslHash, argv[1]);
warn("Creating beds...");
createBeds(bedHash, pslHash, argv[4], (argc-4));
warn("Appending Experiements...");
for(i=4; i < argc; i++)
    {
    printf("%d,",i-4);
    fflush(stdout);
    appendNewExperiment(argv[i], bedHash, pslHash,&erList, (i-4));
    }
warn("\tDone.");
warn("Writing to files...");
erOut = mustOpen(argv[2],"w");
bedOut= mustOpen(argv[3],"w");
for(er = erList; er != NULL; er = er->next)
    {
    expRecordTabOut(er, erOut);
    }
hashTraverseVals(bedHash, averageValues);
hashTraverseVals(bedHash, bedHashOutput);
carefulClose(&erOut);
carefulClose(&bedOut);
freeHash(&pslHash);
freeHash(&bedHash);
warn("Finished.");
return 0;
}
