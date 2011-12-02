#include "common.h"
#include "psl.h"
#include "hash.h"
#include "cheapcgi.h"


#define TICS 100
float seqIdent = -1.0;  /* minimum sequence identity allowed, -1 is a good error as nothing will pass */
float basePct = -1.0;   /* minimum percentage of bases allowed, -1 is a good error as nothing will pass */
char *pslIn = NULL;     /* file to input psls from */
char *pslOut = NULL;    /* file to output psls to */

void msg(char *mesg)
{
fprintf(stdout, "%s", mesg);
fflush(stdout);
}

void putTic()
{
msg(".");
}

void usage() 
{
errAbort("pslAffySelect: filters out psls according to parameters of:\n"
	 "\t- sequence identity (default=93)\n"
	 "usage:\n"
	 "\tpslAffySelect seqIdent=<percent default=.9> basePct=<percent default=.9> in=<in.psl> out=<out.psl>\n");
}

/* create object on the heap, copy values from psl and return, don't forget to free */
struct psl *copyPsl(struct psl *psl)
{
struct psl *ret = NULL;
AllocVar(ret);
ret->next = NULL;
ret->match = psl->match;
ret->misMatch = psl->misMatch;
ret->repMatch = psl->repMatch;
ret->nCount = psl->nCount;
ret->qNumInsert = psl->qNumInsert;
ret->qBaseInsert = psl->qBaseInsert;
ret->tNumInsert = psl->tNumInsert;
ret->tBaseInsert = psl ->tBaseInsert;
strcpy(ret->strand, psl->strand);
ret->qName = cloneString(psl->qName);
ret->qSize = psl->qSize;
ret->qStart = psl->qStart;
ret->qEnd = psl->qEnd;
ret->tName = cloneString(psl->tName);
ret->tSize = psl->tSize;
ret->tStart = psl->tStart;
ret->tEnd = psl->tEnd;
ret->blockCount = psl->blockCount;
ret->blockSizes = CloneArray(psl->blockSizes, psl->blockCount);
ret->qStarts = CloneArray(psl->qStarts, psl->blockCount);
ret->tStarts = CloneArray(psl->tStarts, psl->blockCount);
return ret;
}
 
/* filters out all of psl with below a given percent */
struct psl *filterBySeqIdentity(float percent, struct psl *pslList)
{
struct psl *psl=NULL, *ret=NULL;
int count=0, startCount=0, stopCount=0;
char buff[256];
startCount = slCount( pslList );
msg("filtering by seqId");
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    float tmp = (float)(psl->match + psl->repMatch) / (psl->match + psl->repMatch + psl->misMatch);
    if(tmp >= percent) 
	{
	struct psl *tmpPsl = copyPsl(psl);
	slAddHead(&ret, tmpPsl);
	}
    /* let the user know we're making progress */
    if(count % TICS == 0)
	{
	count = 0;
	msg(".");
	}
    count++;
    }
msg("\tDone.\n");
if(ret != NULL)
    stopCount = slCount(ret);
sprintf(buff, "%d of %d had a seqId of %g or better.\n", stopCount, startCount, percent);
msg(buff);
slReverse(&ret);
return ret;
}

/* filters out all of psl with below a given percent */
struct psl *filterByBasePct(float percent, struct psl *pslList)
{
struct psl *psl=NULL, *ret=NULL;
int count=0, startCount=0, stopCount=0;
char buff[256];
startCount = slCount( pslList );
msg("filtering by seqId");
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    float tmp = (float)(psl->match +psl->repMatch) / (psl->qSize - psl->nCount);
    if(tmp >= percent) 
	{
	struct psl *tmpPsl = copyPsl(psl);
	slAddHead(&ret, tmpPsl);
	}
    /* let the user know we're making progress */
    if(count % TICS == 0)
	{
	count = 0;
	msg(".");
	}
    count++;
    }
msg("\tDone.\n");
if(ret != NULL)
    stopCount = slCount(ret);
sprintf(buff, "%d of %d had a basePct of %g or better.\n", stopCount, startCount, percent);
msg(buff);
slReverse(&ret);
return ret;
}

void filterPsls()
{
struct psl *origPslList=NULL, *pslList=NULL, *psl=NULL;
int startCount=0, stopCount=0;
char buff[256];
origPslList = pslLoadAll(pslIn);

/* some messages for the user */
startCount = slCount(origPslList);
sprintf(buff, "Filtering %d psl using seqIdent=%g and basePct=%g\n", 
	startCount, seqIdent, basePct);
msg(buff);

/* do our filtering */
pslList = filterBySeqIdentity(seqIdent, origPslList);
pslFreeList(&origPslList);
origPslList = filterByBasePct(basePct, pslList);


/* let the user know we're done */
if(origPslList != NULL)
{
stopCount = slCount(origPslList);
pslWriteAll(origPslList, pslOut, FALSE);
pslFreeList(&origPslList);
}
pslFreeList(&origPslList);
pslFreeList(&pslList);
sprintf(buff, "After filtering %d of %d are left\n", stopCount, startCount);
msg(buff);
}

int main(int argc, char *argv[])
{
cgiSpoof(&argc, argv);
seqIdent = cgiOptionalDouble("seqIdent",.9);
basePct = cgiOptionalDouble("basePct", .9);
pslIn = cgiOptionalString("in");
pslOut = cgiOptionalString("out");
if(pslIn == NULL || pslOut==NULL)
    usage();
filterPsls();
return 0;
}
