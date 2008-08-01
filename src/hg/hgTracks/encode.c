/* encode.c - hgTracks routines that are specific to the ENCODE project */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgTracks.h"
#include "encode/encodeRna.h"
#include "encode.h"


char *encodeErgeName(struct track *tg, void *item)
/* return the actual data name, in form xx/yyyy cut off xx/ return yyyy */
{
char *name;
struct linkedFeatures *lf = item;
name = strstr(lf->name, "/");
if (name != NULL)
    name ++;
if (name != NULL)
    return name;
return "unknown";
}
  
void encodeErgeMethods(struct track *tg)
/* setup special methods for ENCODE dbERGE II tracks */
{
tg->itemName = encodeErgeName;
}

Color encodeStanfordNRSFColor(struct track *tg, void *item, struct hvGfx *hvg)
/* color by strand */
{
struct bed *thisItem = item;
int r = tg->color.r;
int g = tg->color.g;
int b = tg->color.b;

if (thisItem->strand[0] == '-') 
    {
    r = g;
    g = b;
    b = tg->color.r;
    }
return hvGfxFindColorIx(hvg, r, g, b);
}

void encodeStanfordNRSFMethods(struct track *tg)
/* custom methods for ENCODE Stanford NRSF data */
{
tg->itemColor = encodeStanfordNRSFColor;
tg->itemNameColor = encodeStanfordNRSFColor;
}



void loadEncodeRna(struct track *tg)
/* Load up encodeRna from database table to track items. */
{
bedLoadItem(tg, "encodeRna", (ItemLoader)encodeRnaLoad);
}

void freeEncodeRna(struct track *tg)
/* Free up encodeRna items. */
{
encodeRnaFreeList((struct encodeRna**)&tg->items);
}

Color encodeRnaColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of encodeRna track item. */
{
struct encodeRna *el = item;

if(el->isRmasked)     return MG_BLACK;
if(el->isTranscribed) return hvGfxFindColorIx(hvg, 0x79, 0xaa, 0x3d);
if(el->isPrediction)  return MG_RED;
return MG_BLUE;
}

char *encodeRnaName(struct track *tg, void *item)
/* Return RNA gene name. */
{
struct encodeRna *el = item;
char *full = el->name;
static char abbrev[64];
char *e;

strcpy(abbrev, skipChr(full));
subChar(abbrev, '_', ' ');
abbr(abbrev, " pseudogene");
if ((e = strstr(abbrev, "-related")) != NULL)
    strcpy(e, "-like");
return abbrev;
}

void encodeRnaMethods(struct track *tg)
/* Make track for rna genes . */
{
tg->loadItems = loadEncodeRna;
tg->freeItems = freeEncodeRna;
tg->itemName = encodeRnaName;
tg->itemColor = encodeRnaColor;
tg->itemNameColor = encodeRnaColor;
}

