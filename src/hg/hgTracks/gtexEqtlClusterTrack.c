/* gtexEqtlClusterTrack - Track of clustered expression QTL's from GTEx */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dystring.h"
#include "hgTracks.h"
#include "gtexInfo.h"
#include "gtexTissue.h"
#include "gtexEqtlCluster.h"

struct gtexEqtlClusterTrack
/* Track info */
{
    int tissueCount;            /* Tissue count */
    struct hash *tissueHash;    /* Tissue info, keyed by UCSC name */
};

static struct gtexEqtlCluster *loadOne(char **row)
/* Load up gtexEqtlCluster from array of strings. */
{
return gtexEqtlClusterLoad(row);
}

static void gtexEqtlClusterLoadItems(struct track *track)
/* Load all items (and motifs if table is present) in window */
{
struct gtexEqtlClusterTrack *extras;
AllocVar(extras);
track->extraUiData = extras;
char *version = gtexVersionSuffix(track->table);
struct gtexTissue *tissues = gtexGetTissues(version);
struct hash *tisHash = hashNew(0);
extras->tissueHash = tisHash;
struct gtexTissue *tis = NULL;
for (tis = tissues; tis != NULL; tis = tis->next)
    hashAdd(extras->tissueHash, tis->name, tis);
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

static void gtexEqtlClusterMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName,
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box on item and label with list of tissues with colors and effect size */
{
char *title = itemName;
uglyf("<br>title: %s\n", title);
if (tg->limitedVis != tvDense)
    {
    // construct list of tissues with colors and effect sizes for mouseover
    struct gtexEqtlCluster *eqtl = (struct gtexEqtlCluster *)item;
    //struct gtexEqtlClusterTrack *extras = (struct gtexEqtlClusterTrack *)tg->extraUiData;
    //struct hash *tissueHash = extras->tissueHash;
    struct dyString *ds = dyStringNew(0);
    int i;
    for (i=0; i<eqtl->expCount; i++)
        {
        double effect= eqtl->expScores[i];
        dyStringPrintf(ds,"%s(%s%0.2f)%s", eqtl->expNames[i], effect < 0 ? "" : "+", effect, 
                        i < eqtl->expCount - 1 ? ", " : "");
        //struct gtexTissue *tis = (struct gtexTissue *)hashFindVal(tissueHash, eqtl->expNames[i]);
        //unsigned color = tis ? tis->color : 0;       // BLACK
        //char *name = tis ? tis->name : "unknown";
        //#dyStringPrintf(ds,"<tr><td style='color: #%06X;'>*</td><td>%s</td><td>%s%0.2f</td></tr>\n", 
                                //color, name, effect < 0 ? "" : "+", effect); 
        }
    title = dyStringCannibalize(&ds);
    }
uglyf("<br>title2: %s\n", title);
genericMapItem(tg, hvg, item, title, itemName, start, end, x, y, width, height);
}

void gtexEqtlClusterMethods(struct track *track)
/* BED5+5 with target, expNames,expScores, expProbs */
{
track->loadItems = gtexEqtlClusterLoadItems;
track->itemName  = gtexEqtlClusterItemName;
track->itemColor = gtexEqtlClusterItemColor;
track->mapItem   = gtexEqtlClusterMapItem;
}
