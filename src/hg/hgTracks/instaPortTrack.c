
/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hgTracks.h"
#include "bedCart.h"
#include "bigWarn.h"
#include "lolly.h"
#include "limits.h"
#include "float.h"
#include "bigBedFilter.h"
#include "asParse.h"
#include "chromAlias.h"
#include "chain.h"
#include "binRange.h"

struct chromMap
/* Remapping information for one (old) chromosome */
    {
    char *name;                 /* Chromosome name. */
    struct binKeeper *bk;       /* Keyed by old position, values are chains. */
    };

struct hash *makeChainHash(struct chain *chain)
{
struct chromMap *map;

struct hash *chainHash = newHash(0);

if ((map = hashFindVal(chainHash, chain->tName)) == NULL)
    {
    AllocVar(map);
    map->bk = binKeeperNew(0, chain->tSize);
    hashAddSaveName(chainHash, chain->tName, map, &map->name);
    }
binKeeperAdd(map->bk, chain->tStart, chain->tEnd, chain);

return chainHash;
}

static void instaPortLoadItems(struct track *track)
{
struct linkedFeatures *lfList = NULL;
char *fileName = cloneString(trackDbSetting(track->tdb, "bigChainUrl"));
char *linkFileName = cloneString(trackDbSetting(track->tdb, "linkDataUrl"));
extern struct chain *chainLoadIdRangeHub(char *db, char *fileName, char *linkFileName,   char *chrom, int start, int end, int id);
// maybe add some slop here for items that cross the window boundaries ?
struct chain *chainList = chainLoadIdRangeHub(database, fileName, linkFileName, chromName, winStart, winEnd, -1);

struct chain *chain;
for(chain = chainList; chain; chain = chain->next)
    {
    struct cBlock *cb;
    cb = chain->blockList; 
    int qStart = cb->qStart;
    int qEnd = cb->qEnd;
    for(; cb; cb = cb->next)
        {
        if (cb->qStart < qStart)
            qStart = cb->qStart;
        if (cb->qEnd > qEnd)
            qEnd = cb->qEnd;
        }

    // now grab the items 
    struct lm *lm = lmInit(0);
    struct bbiFile *bbi = fetchBbiForTrack(track);
    struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chain->qName, qStart, qEnd, 10000, lm);

    for (bb = bbList; bb != NULL; bb = bb->next)
        {
        char startBuf[16], endBuf[16];
        char *bedRow[bbi->fieldCount];
        bigBedIntervalToRow(bb, chain->qName, startBuf, endBuf, bedRow, ArraySize(bedRow));

        // now we're going to use the same chain we queried on to map the items, but we need to swap it
        chainSwap(chain);

        struct hash *chainHash = makeChainHash(chain);
        struct bed *bed = bedLoadN(bedRow, 12);//bbi->fieldCount);
        extern char *remapBlockedBed(struct hash *chainHash, struct bed *bed,
                                     double minMatch, double minBlocks, bool fudgeThick,
                                                                  bool multiple, char *db, char *chainTable)
                                                                  ;
        if (remapBlockedBed(chainHash, bed, 0.0, 0.1, TRUE, TRUE, NULL, NULL) == NULL)
            {
            struct linkedFeatures *lf = lfFromBedExtra(bed, 0, 1000);
            slAddHead(&lfList, lf);
            }
        }
    }
track->items = lfList;
}

void instaPortMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
{
char *mywords[2];
mywords[1] = "12";
bigBedMethods(track, tdb, 2, mywords);
track->loadItems = instaPortLoadItems;
}
