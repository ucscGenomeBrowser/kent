
/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "obscure.h"
#include "limits.h"
#include "float.h"
#include "asParse.h"
#include "chain.h"
#include "binRange.h"
#include "basicBed.h"
#include "liftOver.h"
#include "hash.h"
#include "bigBed.h"
#include "bbiFile.h"
#include "chainNetDbLoad.h"
#include "instaPort.h"

static char *getLinkFile(char *instaPortFile)
/* Construct the file name of the chain link file from the name of a chain file. 
 * That is, change file.bb to file.link.bb */
{
char linkBuffer[4096];

if (!endsWith(instaPortFile, ".bb"))
    errAbort("instaPort file (%s) must end in .bb", instaPortFile);

safef(linkBuffer, sizeof linkBuffer, "%s", instaPortFile);

// truncate string at ending ".bb"
int insertOffset = strlen(linkBuffer) - sizeof ".bb" + 1;
char *insert = &linkBuffer[insertOffset];
*insert = 0;

// add .link.bb
strcpy(insert, ".link.bb");

return cloneString(linkBuffer);
}

struct bigBedInterval *instaIntervals(char *instaPortFile, struct bbiFile *bbi,   char *chrom, int start, int end, struct hash **pChainHash)
/* Return intervals from "other" species that will map to the current window.
 * These intervals are NOT YET MAPPED to the current assembly.
 */
{
char *linkFileName = getLinkFile(instaPortFile);
// need to add some padding to these coordinates
struct chain *chain, *chainList = chainLoadIdRangeHub(NULL, instaPortFile, linkFileName, chrom, start-100000, end+100000, -1);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = NULL;

for(chain = chainList; chain; chain = chain->next)
    {
    struct cBlock *cb;
    cb = chain->blockList; 

    if (cb == NULL)
        continue;

    int qStart = cb->qStart;
    int qEnd = cb->qEnd;

    // get the range for the links on the "other" species
    for(; cb; cb = cb->next)
        {
        if (cb->qStart < qStart)
            qStart = cb->qStart;
        if (cb->qEnd > qEnd)
            qEnd = cb->qEnd;
        }

    // now grab the items 
    struct bigBedInterval *thisInterval = bigBedIntervalQuery(bbi, chain->qName, qStart, qEnd, 10000, lm);
    bbList = slCat(thisInterval, bbList);
    
    // for the mapping we're going to use the same chain we queried on to map the items, but we need to swap it
    chainSwap(chain);

    if (*pChainHash == NULL)
        *pChainHash = newHash(0);
    liftOverAddChainHash(*pChainHash, chain);
    }

return bbList;
}

struct bed *instaBed(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb)
/* Using chains stored in chainHash, port a bigBedInterval from another assembly to a bed
 * on the reference.
 */
{
char startBuf[16], endBuf[16];
char *bedRow[bbi->fieldCount];
char chromName[256];
static int lastChromId = -1;

bbiCachedChromLookup(bbi, bb->chromId, lastChromId, chromName, sizeof(chromName));
//lastChromId=bb->chromId;

bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

struct bed *bed = bedLoadN(bedRow, bbi->definedFieldCount);
char *error;
if ((error = remapBlockedBed(chainHash, bed, 0.0, 0.1, TRUE, TRUE, NULL, NULL)) == NULL)
    return bed;
//else
    //printf("bed %s error:%s<BR>", bed->name, error);

return NULL;
}
