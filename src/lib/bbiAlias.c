#include "common.h"
#include "bigBed.h"
#include "bbiAlias.h"
#include "bPlusTree.h"
#include "obscure.h"
#include "hash.h"

struct slName *bbiAliasFindAliases(struct bbiFile *bbi, struct lm *lm,  char *seqName)
/* Find the aliases for a given seqName using the alias bigBed. */
{
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, seqName, 0, 1, 0, lm);
char *bedRow[bbi->fieldCount];
char startBuf[16], endBuf[16];
struct slName *list = NULL;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, seqName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    int ii;
    for(ii=3; ii < bbi->fieldCount; ii++)
	{
	struct slName *name = newSlName(bedRow[ii]);
	slAddHead(&list, name);
	}
    }

slReverse(&list);
return list;
}

struct bptIndex *bbiAliasOpenExtra(struct bbiFile *bbi)
/* Open any extra indices that this bigBed has. */
{
struct bptIndex *bptList = NULL;
struct slName *indexList = bigBedListExtraIndexes(bbi);

if (indexList == NULL)
    errAbort("chromAlias bigBed file %s has to have a least one extra index.", bbi->fileName);

for(; indexList; indexList = indexList->next)
    {
    struct bptIndex *bptIndex;
    AllocVar(bptIndex);
    bptIndex->bpt = bigBedOpenExtraIndex(bbi, indexList->name, &bptIndex->fieldIx);
    slAddHead(&bptList, bptIndex);
    }

return bptList;
}

char *bbiAliasFindNative(struct bbiFile *bbi, struct bptIndex *bptIndex, struct lm *lm, char *alias)
/* Find the native seqName for a given alias given a bigBed. */
{
for(; bptIndex; bptIndex = bptIndex->next)
    {
    struct bigBedInterval *bb = bigBedNameQuery(bbi, bptIndex->bpt, bptIndex->fieldIx, alias, lm);

    if (bb != NULL)
        {
        char chromName[1024];
        bptStringKeyAtPos(bbi->chromBpt, bb->chromId,  chromName, sizeof(chromName));

        return cloneString(chromName);
        }
    }

return NULL;
}

unsigned bbiAliasChromSizeExt(struct bbiFile *bbi, struct bptIndex *bptIndex, struct lm *lm, char *chrom, struct hash *usedHash, int lineIx)
/* Find the size of the given chrom in the given chromAlias bbi file. If this alias has been used before, complain and exit. */
{
if (usedHash)
    {
    struct hashEl *hel;

    if ((hel = hashLookup(usedHash, chrom)) != NULL)
        {
        int previousLineIx = ptToInt(hel->val);

        errAbort("line %d: using a different alias for a previously seen name on line %d",
            lineIx, previousLineIx);
        }
    }

struct bigBedInterval *bb =  bigBedIntervalQuery(bbi, chrom, 0, 1, 0, lm);
if (bb == NULL)
    {
    for(; bptIndex; bptIndex = bptIndex->next)
        {
        bb= bigBedNameQuery(bbi, bptIndex->bpt, bptIndex->fieldIx, chrom, lm);

        if (bb != NULL)
            break;
        }
    }

char *bedRow[bbi->fieldCount];
char startBuf[16], endBuf[16];
if (bb != NULL)
    {
    int fieldCount = bigBedIntervalToRow(bb, chrom, startBuf, endBuf, bedRow, ArraySize(bedRow));

    if (usedHash)
        {
        hashAddInt(usedHash, bedRow[0], lineIx);

        int ii;
        for (ii = 3; ii < fieldCount; ii++)
            hashAddInt(usedHash, bedRow[ii], lineIx);
        }

    // sequence size is the end address of the bigBed item
    int ret = atoi(endBuf);
    return ret;
    }

return 0;
}

unsigned bbiAliasChromSize(struct bbiFile *bbi, struct bptIndex *bptIndex, struct lm *lm, char *chrom)
/* Find the size of the given chrom in the given chromAlias bbi file.  */
{
return bbiAliasChromSizeExt(bbi, bptIndex, lm, chrom, NULL, 0);
}
