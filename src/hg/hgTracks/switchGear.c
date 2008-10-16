/* Module for SwitchGear Genomics tracks. */

#include "common.h"
#include "hgTracks.h"
#include "switchGear.h"

struct slList *switchDbTssLoadConduit(char **row)
{
return (struct slList *)switchDbTssLoad(row);
}

boolean switchDbTssFilterPseudo(struct slList *slItem)
/* "Include pseudogenes" checkbox filter. */
{
struct switchDbTss *item = (struct switchDbTss *)slItem;
boolean includePseudo = cartUsualBoolean(cart, "switchDbTss.pseudo", FALSE);
if (item == NULL)
    return FALSE;
if (!includePseudo && (item->isPseudo == 1))
    return FALSE;
return TRUE;
}

struct linkedFeatures *lfFromSwitchDbTss(struct slList *item)
/* Translate a switchDbTss thing into a linkedFeatures. */
{
struct switchDbTss *tss = (struct switchDbTss *)item;
struct linkedFeatures *lf;
struct simpleFeature *sf;
AllocVar(lf);
lf->start = tss->chromStart;
lf->end = tss->chromEnd;
lf->tallStart = tss->chromStart;
lf->tallEnd = tss->chromEnd;
lf->filterColor = -1;
lf->orientation = orientFromChar(tss->strand[0]);
lf->grayIx = grayInRange((int)tss->confScore, 0, 100);
strncpy(lf->name, tss->name, sizeof(lf->name));
AllocVar(sf);
sf->start = tss->chromStart;
sf->end = tss->chromEnd;
sf->grayIx = lf->grayIx;
lf->components = sf;
lf->codons = CloneVar(sf);
return lf;
}

void loadItemsSwitchDbTss(struct track *tg)
/* Load switchDbTss items into a linkedFeatures list. */
{
char optionScoreStr[128]; /* Option -  score filter */
safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter",
      tg->tdb->tableName);
if (!cartVarExists(cart, optionScoreStr))
    cartSetInt(cart, optionScoreStr, SWITCHDBTSS_FILTER); 
loadLinkedFeaturesWithLoaders(tg, switchDbTssLoadConduit, lfFromSwitchDbTss, 
			      "confScore", NULL, switchDbTssFilterPseudo);
}

Color switchDbTssItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to switchDbTss item */
{
struct linkedFeatures *thisItem = item;
int grayIx = thisItem->grayIx;
if (grayIx == 1)
    grayIx++;
return shadesOfBrown[grayIx];
}

void switchDbTssMethods(struct track *tg)
/* Methods for switchDbTss track uses mostly linkedFeatures stuff. */
{
linkedFeaturesMethods(tg);
tg->loadItems = loadItemsSwitchDbTss;
tg->itemColor = switchDbTssItemColor;
}
