/* lineFileOnBigBed - set up lineFile support on a BigBed
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "bigBed.h"

struct lfBigBedData
/* data used during callbacks */
    {	
    struct bbiFile *bbiHandle;                            // BigBed handle
    struct bbiChromInfo *bbiChrom, *bbiChromList;         // BigBed chrom info
    struct lm *bbiLm;                                     // BigBed local memory
    struct bigBedInterval *bbiInterval, *bbiIntervalList; // BigBed intervals
    };

void checkBigBedSupport(struct lineFile *lf, char *where)
/* abort if not supported by bigBed */
{
if (sameString(where,"lineFileSeek"))
    lineFileAbort(lf, "%s: not supported for lineFile on BigBed.", where);
}

boolean lineFileNextBigBed(struct lineFile *lf, char **retStart, int *retSize)
/* skip to the next line */
{
struct lfBigBedData *lfBigBedData = lf->dataForCallBack;
int lineSize = 0;
if (!lfBigBedData->bbiChrom)
    return FALSE;
if (!lfBigBedData->bbiInterval)
    return FALSE;
lineSize = 1024; // some extra room
lineSize += strlen(lfBigBedData->bbiChrom->name);
if (lfBigBedData->bbiInterval->rest)
    lineSize += strlen(lfBigBedData->bbiInterval->rest);

if (lineSize > lf->bufSize)
    lineFileExpandBuf(lf, lineSize * 2);
safef(lf->buf, lf->bufSize, "%s\t%u\t%u", lfBigBedData->bbiChrom->name, lfBigBedData->bbiInterval->start, lfBigBedData->bbiInterval->end);
if (lfBigBedData->bbiInterval->rest)
    {
    safecat(lf->buf, lf->bufSize, "\t");
    safecat(lf->buf, lf->bufSize, lfBigBedData->bbiInterval->rest);
    }
lf->bufOffsetInFile = -1;
lf->bytesInBuf = strlen(lf->buf);
lf->lineIx++;
lf->lineStart = 0;
lf->lineEnd = lineSize;
*retStart = lf->buf;
if (retSize != NULL)
    *retSize = lineSize;

lfBigBedData->bbiInterval = lfBigBedData->bbiInterval->next;
if (!lfBigBedData->bbiInterval)
    {
    lmCleanup(&lfBigBedData->bbiLm);
    lfBigBedData->bbiLm = lmInit(0);
    lfBigBedData->bbiChrom = lfBigBedData->bbiChrom->next;
    if(lfBigBedData->bbiChrom)
	lfBigBedData->bbiIntervalList = bigBedIntervalQuery(
	    lfBigBedData->bbiHandle, lfBigBedData->bbiChrom->name, 0, lfBigBedData->bbiChrom->size, 0, lfBigBedData->bbiLm);
    }

return TRUE;
}

void lineFileCloseBigBed(struct lineFile *lf)
/* release bigBed resources */
{
struct lfBigBedData *lfBigBedData = lf->dataForCallBack;
lmCleanup(&lfBigBedData->bbiLm);
bbiChromInfoFreeList(&lfBigBedData->bbiChromList);
bbiFileClose(&lfBigBedData->bbiHandle);
freez(&lf->dataForCallBack);
}


struct lineFile *lineFileOnBigBed(char *bigBedFileName)
/* Wrap a line file object around a BigBed. */
{
struct lineFile *lf;
AllocVar(lf);
lf->fileName = cloneString(bigBedFileName);
struct lfBigBedData *lfBigBedData;
AllocVar(lfBigBedData);
lf->dataForCallBack = lfBigBedData;
lfBigBedData->bbiHandle = bigBedFileOpen(lf->fileName);
lfBigBedData->bbiChromList = bbiChromList(lfBigBedData->bbiHandle);
lfBigBedData->bbiChrom = lfBigBedData->bbiChromList;
lfBigBedData->bbiLm = lmInit(0);
if (lfBigBedData->bbiChrom)
    {
    lfBigBedData->bbiIntervalList = bigBedIntervalQuery(
	lfBigBedData->bbiHandle, lfBigBedData->bbiChrom->name, 0, lfBigBedData->bbiChrom->size, 0, lfBigBedData->bbiLm);
    lfBigBedData->bbiInterval = lfBigBedData->bbiIntervalList;
    }
lf->checkSupport = checkBigBedSupport;
lf->nextCallBack = lineFileNextBigBed;
lf->closeCallBack = lineFileCloseBigBed;
lf->fd = -1;
lf->lineIx = 0;
lf->bufSize = 64 * 1024;
lf->buf = needMem(lf->bufSize);
return lf;
}

