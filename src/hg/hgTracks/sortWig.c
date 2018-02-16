
#include "common.h"
#include "hgTracks.h"
#include "wigCommon.h"
//#include "clearcut.h"
#include "dystring.h"
#include "trashDir.h"
#include "optimalLeaf.h"

struct trackBins
    {
    struct trackBins *next;
    char *name;
    struct pixelCountBin *bin;
    };

struct sortGroup
    {
    struct sortGroup *next;
    struct trackBins *trackBins;
    };

struct sortGroup *getSortGroup(struct hash *groupHash, struct flatTracks *flatTrack)
{
struct trackDb *tdb = flatTrack->track->tdb;
struct sortGroup *sortGroup;

while(tdb->parent)
    tdb = tdb->parent;

struct hashEl *hel;
if ((hel = hashLookup(groupHash, tdb->track)) == NULL)
    {
    AllocVar(sortGroup);
    hashAdd(groupHash, tdb->track, sortGroup);
    }
else
    sortGroup = hel->val;

return sortGroup;
}

struct trackSum
    {
    char *name;
    unsigned long long sum;
    };

int nameCompare(const void *vone, const void *vtwo)
/* Compares two trackBins by their track name */
{
struct trackBins *one = *((struct trackBins **)vone);
struct trackBins *two = *((struct trackBins **)vtwo);

return strcmp(one->name, two->name);
}

int sumCompare(const void *vone, const void *vtwo)
{
struct trackSum *one = (struct trackSum *)vone;
struct trackSum *two = (struct trackSum *)vtwo;

return two->sum - one->sum;
}

void calcWiggleOrdering(struct cart *cart, struct flatTracks *flatTracks)
{
struct track *track;
struct flatTracks *flatTrack = NULL;
struct hash *groupHash = newHash(5);

for (flatTrack = flatTracks; flatTrack != NULL; flatTrack = flatTrack->next)
    {
    track = flatTrack->track;
    if ( track->wigGraphOutput )
        {
        struct pixelCountBin *bins = track->wigGraphOutput->pixelBins;
        if (bins != 0)
            {
            struct sortGroup *sg = getSortGroup(groupHash, flatTrack);
            struct trackBins *tb;

            AllocVar(tb);
            slAddHead(&sg->trackBins, tb);
            tb->bin = bins;
            tb->name = track->track;
            }
        }
    }

struct hashCookie cookie = hashFirst(groupHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct sortGroup *sg = hel->val;
    int numRows = slCount(sg->trackBins);
    int numCols = sg->trackBins->bin->binCount;
    float **rows;
    AllocArray(rows, numRows);
    char **wigNames;
    AllocArray(wigNames, numRows);

    struct trackBins *tb = sg->trackBins;
    
    // make sure initial state is always the same.
    slSort(&tb, nameCompare);

    int ii, jj;
    struct trackSum *sums;
    AllocArray(sums, numRows);
    
    for(ii=0; ii < numRows; ii++, tb = tb->next)
        {
        wigNames[ii] = tb->name;
        AllocArray(rows[ii], numCols);
        struct pixelCountBin *pcb = tb->bin;
        unsigned long *vals = pcb->bins;
        sums[ii].name = tb->name;
        for(jj=0; jj < numCols; jj++, vals++)
            {
            rows[ii][jj] = *vals;
            sums[ii].sum += *vals;
            }
        }

    char group[4096];
    struct dyString *dy = newDyString(100);

    safef(group, sizeof group, "expOrder_%s", hel->name);
    qsort(sums, numRows, sizeof(struct trackSum ), sumCompare);
    for(ii=0; ii < numRows; ii++)
        dyStringPrintf(dy, "%s ", sums[ii].name);
    cartSetString(cart, group, dy->string);

    dyStringClear(dy);

    safef(group, sizeof group, "simOrder_%s", hel->name);
    int *order;
    if (numRows < 3)  // optimal leaf order crashes with 2 or 1
        {
        AllocArray(order, numRows);
        int ii;
        for(ii=0; ii < numRows; ii++)
            order[ii] = ii + 1;
        }
    else
        order = optimalLeafOrder(numRows, numCols, 0, rows, wigNames, NULL, NULL);
    for(ii=0; ii < numRows; ii++)
        dyStringPrintf(dy, "%s ", wigNames[order[ii] - 1]);

    cartSetString(cart, group, dy->string);
    }
}

