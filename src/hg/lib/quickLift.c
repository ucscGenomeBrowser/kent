
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
#include "hdb.h"
#include "jksql.h"
#include "hgConfig.h"
#include "quickLift.h"
#include "bigChain.h"

struct bigBedInterval *quickLiftGetIntervals(char *quickLiftFile, struct bbiFile *bbi,   char *chrom, int start, int end, struct hash **pChainHash)
/* Return intervals from "other" species that will map to the current window.
 * These intervals are NOT YET MAPPED to the current assembly.
 */
{
char *linkFileName = bigChainGetLinkFile(quickLiftFile);
int maxGapBefore = 0;
int maxGapAfter = 0;
struct chain *chain, *chainList = chainLoadIdRangeHub(NULL, quickLiftFile, linkFileName, chrom, start, end, -1);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = NULL, *bb;

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

    // now grab the items , probably we should parameterize the max number of items, but to what?
    struct bigBedInterval *thisInterval = NULL;
    if (chain->qStrand == '-')
        thisInterval = bigBedIntervalQuery(bbi, chain->qName, chain->qSize - qEnd, chain->qSize - qStart,  1000000, lm);
    else
        thisInterval = bigBedIntervalQuery(bbi, chain->qName, qStart, qEnd, 1000000, lm);

    // find how much of the items are beyond the viewport
    for(bb=thisInterval; bb; bb = bb->next)
        {
        if (bb->start < qStart)
            {
            int gap = qStart - bb->start;
            if (gap > maxGapBefore)
                maxGapBefore = gap;
            }
        if (bb->end > qEnd)
            {
            int gap = bb->end - qEnd;
            if (gap > maxGapAfter)
                maxGapAfter = gap;
            }
        }
    bbList = slCat(thisInterval, bbList);
    }

// now we need to grab the links outside of our viewport so we can map long items
// probably we could reuse the chains from above but for the moment this is easier
// For the moment we use the same padding on both sides so we don't have to worry about strand
if (maxGapBefore > maxGapAfter)
    maxGapAfter = maxGapBefore;
else
    maxGapBefore = maxGapAfter;

int newStart = start - maxGapBefore * 2;
if (newStart < 0)
    newStart = 0;
int newEnd = end + maxGapAfter * 2;
chainList = chainLoadIdRangeHub(NULL, quickLiftFile, linkFileName, chrom, newStart, newEnd, -1);
for(chain = chainList; chain; chain = chain->next)
    {
    chainSwap(chain);

    if (*pChainHash == NULL)
        *pChainHash = newHash(0);
    liftOverAddChainHash(*pChainHash, chain);
    }

return bbList;
}

static void make12(struct bed *bed)
/* Make a bed12 out of something less than that. */
{
bed->blockCount = 1;
bed->blockSizes = needMem(sizeof(int));
bed->blockSizes[0] = bed->chromEnd - bed->chromStart;
bed->chromStarts = needMem(sizeof(int));
bed->chromStarts[0] = 0;
}

struct bed *quickLiftIntervalsToBed(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb)
/* Using chains stored in chainHash, port a bigBedInterval from another assembly to a bed
 * on the reference.
 */
{
char startBuf[16], endBuf[16];
char *bedRow[bbi->fieldCount];
char chromName[256];
static int lastChromId = -1;

bbiCachedChromLookup(bbi, bb->chromId, lastChromId, chromName, sizeof(chromName));

bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

struct bed *bed = bedLoadN(bedRow, bbi->definedFieldCount);
char *error;
if (bbi->definedFieldCount < 12)
    make12(bed);

if ((error = remapBlockedBed(chainHash, bed, 0.0, 0.1, TRUE, TRUE, NULL, NULL)) == NULL)
    return bed;
//else
    //printf("bed %s error:%s<BR>", bed->name, error);

return NULL;
}

unsigned quickLiftGetChain(char *fromDb, char *toDb)
/* Return the id from the quickLiftChain table for given assemblies. */
{
if (!quickLiftEnabled())
    return 0;

unsigned ret = 0;
struct sqlConnection *conn = hConnectCentral();
char query[2048];
sqlSafef(query, sizeof(query), "select q.id from quickLiftChain q  where q.fromDb='%s' and q.toDb='%s'", fromDb, toDb);
char *geneId = sqlQuickString(conn, query);

hDisconnectCentral(&conn);

if (geneId)
    ret = atoi(geneId);

return ret;
}

struct slList *quickLiftSql(struct sqlConnection *conn, char *quickLiftFile, char *table, char *chrom, int start, int end,  char *query, char *extraWhere, ItemLoader2 loader, int numFields,struct hash *chainHash)
// retrieve items for which we have a loader from a SQL database for which we have a set quickLift chains.  
// Save the chains we used to map the item back to the current reference.
{
// need to add some padding to these coordinates
int padStart = start - 100000;
if (padStart < 0)
    padStart = 0;

char *linkFileName = bigChainGetLinkFile(quickLiftFile);
struct chain *chain, *chainList = chainLoadIdRangeHub(NULL, quickLiftFile, linkFileName, chrom, padStart, end+100000, -1);

struct slList *item, *itemList = NULL;
int rowOffset = 0;
struct sqlResult *sr = NULL;
char **row = NULL;

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

    if (chain->qStrand == '-')
        qStart = chain->qSize - qStart;

    // now grab the items 
    if (query == NULL)
        sr = hRangeQuery(conn, table, chain->qName,
                         qStart, qEnd, extraWhere, &rowOffset);
    else
        sr = sqlGetResult(conn, query);

    while ((row = sqlNextRow(sr)) != NULL)
        {
        item = loader(row + rowOffset, numFields);
        slAddHead(&itemList, item);
        }

    // now squirrel the swapped chains we used to use to make the retrieved items back to us
    chainSwap(chain);
    liftOverAddChainHash(chainHash, chain);
    }

return itemList;
}

struct bed *quickLiftBeds(struct bed *bedList, struct hash *chainHash, boolean blocked)
// Map a list of bedd in query coordinates to our current reference
{
struct bed *liftedBedList = NULL;
struct bed *nextBed;
struct bed *bed;
for(bed = bedList; bed; bed = nextBed)
    {
    // remapBlockedBed may want to add new beds after this bed if the region maps to more than one location
    nextBed = bed->next;
    bed->next = NULL;

    char *error;
    if (!blocked)
        {
        error = liftOverRemapRange(chainHash, 0.0, bed->chrom, bed->chromStart, bed->chromEnd, bed->strand[0],
                             
                            0.001, &bed->chrom, (int *)&bed->chromStart, (int *)&bed->chromEnd, &bed->strand[0]);
        }
    else
        error = remapBlockedBed(chainHash, bed, 0.0, 0.1, TRUE, TRUE, NULL, NULL);

    if (error == NULL)
        {
        slAddHead(&liftedBedList, bed);
        }
    }
return liftedBedList;
}

boolean quickLiftEnabled()
/* Return TRUE if feature is available */
{
char *cfgEnabled = cfgOption("browser.quickLift");
return cfgEnabled && (sameString(cfgEnabled, "on") || sameString(cfgEnabled, "true")) ;
}
