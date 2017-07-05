/* gtexEqtlClusterTrack - Track of clustered expression QTL's from GTEx */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dystring.h"
#include "hgTracks.h"
#include "gtexInfo.h"
#include "gtexTissue.h"
#include "gtexUi.h"
#include "gtexEqtlCluster.h"

struct gtexEqtlClusterTrack
/* Track info for filtering (maybe later for draw, map) */
{
    struct gtexTissue *tissues; /*  Tissue names, descriptions */
    struct hash *tissueHash;    /* Tissue info, keyed by UCSC name, filtered by UI */
    double minEffect;           /* Effect size filter (abs value) */
    double minProb;             /* Probability filter */
};

static struct gtexEqtlCluster *loadOne(char **row)
/* Load up gtexEqtlCluster from array of strings. */
{
return gtexEqtlClusterLoad(row);
}

static boolean filterTissues(struct track *track, struct gtexEqtlClusterTrack *extras)
/* Check cart for tissue selection */
{
char *version = gtexVersionSuffix(track->table);
extras->tissues = gtexGetTissues(version);
extras->tissueHash = hashNew(0);
if (cartListVarExistsAnyLevel(cart, track->tdb, FALSE, GTEX_TISSUE_SELECT))
    {
    struct slName *selectedValues = cartOptionalSlNameListClosestToHome(cart, track->tdb,
                                                        FALSE, GTEX_TISSUE_SELECT);
    if (selectedValues != NULL)
        {
        struct slName *name;
        for (name = selectedValues; name != NULL; name = name->next)
            hashAdd(extras->tissueHash, name->name, name->name);
        return TRUE;
        }
    }
/* no filter */
struct gtexTissue *tis = NULL;
for (tis = extras->tissues; tis != NULL; tis = tis->next)
    hashAdd(extras->tissueHash, tis->name, tis->name);
return FALSE;
}

static void excludeTissue(struct gtexEqtlCluster *eqtl, int i)
/* Mark the tissue to exclude from display */
{
eqtl->expScores[i] = 0.0;
}

static boolean isExcludedTissue(struct gtexEqtlCluster *eqtl, int i)
/* Check if eQTL is excluded */
{
return (eqtl->expScores[i] == 0.0);
}

static boolean eqtlIncludeFilter(struct track *track, void *item)
/* Apply all filters (except gene) to eQTL item */
{
int i;
int excluded = 0;
double maxEffect = 0.0;
double maxProb = 0.0;
struct gtexEqtlCluster *eqtl = (struct gtexEqtlCluster *)item;
struct gtexEqtlClusterTrack *extras = (struct gtexEqtlClusterTrack *)track->extraUiData;
for (i=0; i<eqtl->expCount; i++)
    {
    if (hashFindVal(extras->tissueHash, eqtl->expNames[i]))
        {
        maxEffect = fmax(maxEffect, fabs(eqtl->expScores[i]));
        maxProb = fmax(maxProb, fabs(eqtl->expProbs[i]));
        }
    else
        {
        excludeTissue(eqtl, i);
        excluded++;
        }
    }
// exclude if no tissues match selector
//uglyf(", excluded: %d, maxEffect: %0.2f (min %0.2f), maxProb: %0.2f (min %0.2f)",
        //excluded, maxEffect, extras->minEffect, maxProb, extras->minProb);
if (excluded == eqtl->expCount || 
    // or if variant has no selected tissue where effect size is above cutoff
    maxEffect < extras->minEffect || 
    // or if variant has no selected tissue where probability is above cutoff
    maxProb < extras->minProb)
        return FALSE;
return TRUE;
}

static void gtexEqtlClusterLoadItems(struct track *track)
/* Load items in window and prune those that don't pass filter */
{
/* initialize track info */
struct gtexEqtlClusterTrack *extras;
AllocVar(extras);
track->extraUiData = extras;

// filter by gene via SQL
char cartVar[64];
safef(cartVar, sizeof cartVar, "%s.%s", track->track, GTEX_EQTL_GENE);
char *gene = cartNonemptyString(cart, cartVar);
char *where = NULL;
if (gene)
    {
    struct dyString *ds = dyStringNew(0);
    sqlDyStringPrintfFrag(ds, "%s = '%s'", GTEX_EQTL_GENE_FIELD, gene); 
    where = dyStringCannibalize(&ds);
    }
bedLoadItemWhere(track, track->table, where, (ItemLoader)loadOne);
//uglyf("DEBUG: loaded items: %d\n", slCount(track->items));

safef(cartVar, sizeof cartVar, "%s.%s", track->track, GTEX_EQTL_EFFECT);
extras->minEffect = fabs(cartUsualDouble(cart, cartVar, GTEX_EFFECT_MIN_DEFAULT));
safef(cartVar, sizeof cartVar, "%s.%s", track->track, GTEX_EQTL_PROBABILITY);
extras->minProb = cartUsualDouble(cart, cartVar, 0.0);
boolean hasTissueFilter = filterTissues(track, extras);
if (!hasTissueFilter && extras->minEffect == 0.0 && extras->minProb == 0.0)
    return;

// more filtering
filterItems(track, eqtlIncludeFilter, "include");
//uglyf(", items after filter: %d\n", slCount(track->items));
}

static char *gtexEqtlClusterItemName(struct track *track, void *item)
/* Load all items (and motifs if table is present) in window */
{
struct gtexEqtlCluster *eqtl = (struct gtexEqtlCluster *)item;
struct dyString *ds = dyStringNew(0);
if (eqtl->expCount == 1)
    {
    dyStringPrintf(ds, "%s in %s", eqtl->target, eqtl->expNames[0]);
    }
else
    {
    int i, included;
    for (i=0, included=0; i<eqtl->expCount; i++)
        if (!isExcludedTissue(eqtl, i))
            included++;
    dyStringPrintf(ds, "%s in %d tissues", eqtl->target, included);
    }
return dyStringCannibalize(&ds);
}

static Color gtexEqtlClusterItemColor(struct track *track, void *item, struct hvGfx *hvg)
/* Color by highest effect in list (blue -, red +), with brighter for higher effect (teal, fuschia) */
{
struct gtexEqtlCluster *eqtl = (struct gtexEqtlCluster *)item;
double maxEffect = 0.0;
int i;
for (i=0; i<eqtl->expCount; i++)
    {
    if (isExcludedTissue(eqtl, i))
        continue;
    double effect = eqtl->expScores[i];
    if (fabs(effect) > fabs(maxEffect))
        maxEffect = effect;
    }
double cutoff = 2.0;
if (maxEffect < 0.0)
    {
    /* down-regulation displayed as blue */
    if (maxEffect < 0.0 - cutoff)
        return MG_CYAN;
    return MG_BLUE;
    }
/* up-regulation displayed as red */
if (maxEffect > cutoff)
    return MG_MAGENTA;
return MG_RED;
}

static void gtexEqtlClusterMapItem(struct track *track, struct hvGfx *hvg, void *item, char *itemName,
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box on item and label with list of tissues with colors and effect size */
{
char *title = itemName;
//uglyf("<br>title: %s\n", title);
if (track->limitedVis != tvDense)
    {
    // construct list of tissues with colors and effect sizes for mouseover
    struct gtexEqtlCluster *eqtl = (struct gtexEqtlCluster *)item;
    //struct gtexEqtlClusterTrack *extras = (struct gtexEqtlClusterTrack *)track->extraUiData;
    //struct hash *tissueHash = extras->tissueHash;
    struct dyString *ds = dyStringNew(0);
    dyStringPrintf(ds, "%s/%s: ", eqtl->name, eqtl->target);
    int i;
    for (i=0; i<eqtl->expCount; i++)
        {
        if (isExcludedTissue(eqtl, i))
            continue;
        double effect= eqtl->expScores[i];
        dyStringPrintf(ds, "%s(%s%0.2f)%s", eqtl->expNames[i], effect < 0 ? "" : "+", effect, 
                        i < eqtl->expCount - 1 ? ", " : "");
        //struct gtexTissue *tis = (struct gtexTissue *)hashFindVal(tissueHash, eqtl->expNames[i]);
        //unsigned color = tis ? tis->color : 0;       // BLACK
        //char *name = tis ? tis->name : "unknown";
        //#dyStringPrintf(ds,"<tr><td style='color: #%06X;'>*</td><td>%s</td><td>%s%0.2f</td></tr>\n", 
                                //color, name, effect < 0 ? "" : "+", effect); 
        }
    title = dyStringCannibalize(&ds);
    }
//uglyf("<br>title2: %s\n", title);
genericMapItem(track, hvg, item, title, itemName, start, end, x, y, width, height);
}

void gtexEqtlClusterMethods(struct track *track)
/* BED5+5 with target, expNames,expScores, expProbs */
{
track->loadItems = gtexEqtlClusterLoadItems;
track->itemName  = gtexEqtlClusterItemName;
track->itemColor = gtexEqtlClusterItemColor;
track->mapItem   = gtexEqtlClusterMapItem;
}
