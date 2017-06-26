/* gtexEqtlClusterTrack - Track of clustered expression QTL's from GTEx */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dystring.h"
#include "hgTracks.h"
#include "gtexEqtlCluster.h"

static struct gtexEqtlCluster *loadOne(char **row)
/* Load up gtexEqtlCluster from array of strings. */
{
return gtexEqtlClusterLoad(row);
}

static void gtexEqtlClusterLoadItems(struct track *track)
/* Load all items (and motifs if table is present) in window */
{
bedLoadItem(track, track->table, (ItemLoader)loadOne);
}

static char *gtexEqtlClusterItemName(struct track *track, void *item)
/* Load all items (and motifs if table is present) in window */
{
struct gtexEqtlCluster *cluster = (struct gtexEqtlCluster *)item;
struct dyString *ds = dyStringNew(0);
if (cluster->expCount == 1)
    {
    dyStringPrintf(ds, "%s in %s", cluster->target, cluster->expNames[0]);
    }
else
    {
    dyStringPrintf(ds, "%s in %d tissues", cluster->target, cluster->expCount);
    }
return dyStringCannibalize(&ds);
}

static Color gtexEqtlClusterItemColor(struct track *track, void *item, struct hvGfx *hvg)
/* Color by highest effect in list (blue -, red +), with brighter for higher effect (teal, fuschia) */
{
struct gtexEqtlCluster *cluster = (struct gtexEqtlCluster *)item;
int maxEffect = 0;
int i;
for (i=0; i<cluster->expCount; i++)
    {
    int effect = ceil(cluster->expScores[i]);
    if (abs(effect) > abs(maxEffect))
        maxEffect = effect;
    }
int cutoff = 2;
if (maxEffect < 0)
    {
    /* down-regulation displayed as blue */
    if (maxEffect < 0 - cutoff)
        return MG_CYAN;
    return MG_BLUE;
    }
/* up-regulation displayed as red */
if (maxEffect > cutoff)
    return MG_MAGENTA;
return MG_RED;
}

void gtexEqtlClusterMethods(struct track *track)
/* BED5+5 with target, expNames,expScores, expProbs */
{
track->loadItems = gtexEqtlClusterLoadItems;
track->itemName  = gtexEqtlClusterItemName;
track->itemColor = gtexEqtlClusterItemColor;
//track->mapItem   = gtexEqtlClusterMapItem;
}
