/* variation.h - hgTracks routines that are specific to the SNP and haplotype tracks */

#ifndef VARIATION_H
#define VARIATION_H

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgTracks.h"
#include "snp.h"
#include "snpMap.h"
#include "hui.h"
#include "snpUi.h"
#include "snp125.h"
#include "snp125Ui.h"
#include "snp132Ext.h"
#include "spaceSaver.h"
#include "ld.h"
#include "ld2.h"
#include "hash.h"
#include "ldUi.h"
#include "gfxPoly.h"
#include "memgfx.h"
#include "cnpIafrate.h"
#include "cnpIafrate2.h"
#include "cnpSebat.h"
#include "cnpSebat2.h"
#include "cnpSharp.h"
#include "cnpSharp2.h"
#include "cnpRedon.h"
#include "cnpLocke.h"
#include "cnpSharpSample.h"
#include "cnpSharpCutoff.h"
#include "hgTracks.h"

/****** snpMap *******/

boolean snpMapSourceFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

boolean snpMapTypeFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

void loadSnpMap(struct track *tg);
/* Load up snpMarkers from database table to track items. */

void freeSnpMap(struct track *tg);
/* Free up snpMap items. */

Color snpMapColor(struct track *tg, void *item, struct hvGfx *hvg);
/* Return color of snpMap track item. */

void snpMapDrawItemAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, 
		      double scale, MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single snpMap item at position. */

void drawDiamond(struct hvGfx *hvg, 
		 int xl, int yl, int xt, int yt, int xr, int yr, int xb, int yb, 
		 Color fillColor, Color outlineColor);
/* Draw diamond shape. */

void mapDiamondUi(struct hvGfx *hvg, int xl, int yl, int xt, int yt, 
			 int xr, int yr, int xb, int yb, 
			 char *name, char *shortLabel, char *trackName);
/* Print out image map rectangle that invokes hgTrackUi. */

Color getOutlineColor(struct track *tg, int itemCount);
/* get outline color from cart and set outlineColor*/

void mapTrackBackground(struct track *tg, struct hvGfx *hvg, int xOff, int yOff);
/* Print out image map rectangle that invokes hgTrackUi. */

void initColorLookup(struct track *tg, struct hvGfx *hvg, boolean isDprime);

void ldAddToDenseValueHash(struct hash *ldHash, unsigned a, char charValue);
/* Add new values to LD hash or update existing values.
   Values are averaged along the diagonals. */

void ldDrawDenseValueHash(struct hvGfx *hvg, struct track *tg, int xOff, int yOff, 
			  double scale, Color outlineColor, struct hash *ldHash);
/* Draw all dense LD values */


void snpMapMethods(struct track *tg);
/* Make track for snps. */

/****** snp ******/

void snpMethods(struct track *tg);
void snp125Methods(struct track *tg);
/* Make track for snps. */

/***** haplotypes *****/

char *perlegenName(struct track *tg, void *item);
/* return the actual perlegen name, in form xx/yyyy cut off xx/ return yyyy */

void haplotypeMethods(struct track *tg);
/* setup special methods for haplotype track */

void perlegenMethods(struct track *tg);
/* setup special methods for Perlegen haplotype track */


/****** LD *****/

/* 10 shades from black to fully saturated of red/green/blue */
#define LD_DATA_SHADES 10
extern Color ldShadesPos[LD_DATA_SHADES];
extern Color ldHighLodLowDprime;
extern Color ldHighDprimeLowLod;
extern int colorLookup[256];

struct ldStats 
/* collect stats for drawing LD values in dense mode */
{
    struct ldStats *next;
    unsigned        chromStart;
    unsigned        n;         /* count of snps with valid LD values */
    unsigned        sumValues; /* sum of valid LD values */
};

void ldLoadItems(struct track *tg);
/* loadItems loads up items for the chromosome range indicated.   */

int ldTotalHeight(struct track *tg, enum trackVisibility vis);
/* Return total height. Called before and after drawItems. 
 * Must set height, lineHeight, heightPer */ 

void ldDrawItems(struct track *tg, int seqStart, int seqEnd,
		  struct hvGfx *hvg, int xOff, int yOff, int width, 
		  MgFont *font, Color color, enum trackVisibility vis);
/* Draw item list, one per track. */

void ldDrawItemAt(struct track *tg, void *item, struct hvGfx *hvg, 
		  int xOff, int yOff, double scale, 
		  MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single item.  Required for genericDrawItems */

void ldFreeItems(struct track *tg);
/* Free item list. */

void ldMethods(struct track *tg);
/* setup special methods for Linkage Disequilibrium track */

/****** CNP / Structural Variants ******/

void cnpIafrateLoadItems(struct track *tg);
/* loader for cnpIafrate table */

void cnpIafrateFreeItems(struct track *tg);

Color cnpIafrateColor(struct track *tg, void *item, struct hvGfx *hvg);
/* green for GAIN, red for LOSS, blue for both */

void cnpIafrateMethods(struct track *tg);
/* methods for cnpIafrate */

void cnpIafrate2LoadItems(struct track *tg);
/* loader for cnpIafrate2 table */

void cnpIafrate2FreeItems(struct track *tg);

Color cnpIafrate2Color(struct track *tg, void *item, struct hvGfx *hvg);
/* green for GAIN, red for LOSS, blue for both */

void cnpIafrate2Methods(struct track *tg);
/* methods for cnpIafrate2 */

void cnpSebatLoadItems(struct track *tg);
/* loader for cnpSebat table */

void cnpSebatFreeItems(struct track *tg);

Color cnpSebatColor(struct track *tg, void *item, struct hvGfx *hvg);
/* green for GAIN, red for LOSS, blue for both */

void cnpSebatMethods(struct track *tg);
/* methods for cnpSebat */

void cnpSebat2LoadItems(struct track *tg);
/* loader for cnpSebat2 table */

void cnpSebat2FreeItems(struct track *tg);

Color cnpSebat2Color(struct track *tg, void *item, struct hvGfx *hvg);
/* green for GAIN, red for LOSS */

void cnpSebat2Methods(struct track *tg);
/* methods for cnpSebat2 */

void cnpSharpLoadItems(struct track *tg);
/* loader for cnpSharp table */

void cnpSharpFreeItems(struct track *tg);

Color cnpSharpColor(struct track *tg, void *item, struct hvGfx *hvg);
/* green for GAIN, red for LOSS, blue for both */

void cnpSharpMethods(struct track *tg);
/* methods for cnpSharp */

void cnpSharp2LoadItems(struct track *tg);
/* loader for cnpSharp2 table */

void cnpSharp2FreeItems(struct track *tg);

Color cnpSharp2Color(struct track *tg, void *item, struct hvGfx *hvg);
/* green for GAIN, red for LOSS, blue for both */

void cnpSharp2Methods(struct track *tg);
/* methods for cnpSharp2 */

void cnpFosmidLoadItems(struct track *tg);
/* loader for cnpFosmid table */

void cnpFosmidFreeItems(struct track *tg);

Color cnpFosmidColor(struct track *tg, void *item, struct hvGfx *hvg);
/* green for I, red for D */

void cnpFosmidMethods(struct track *tg);
/* methods for cnpFosmid */

void cnpRedonLoadItems(struct track *tg);
/* loader for cnpRedon table */

void cnpRedonFreeItems(struct track *tg);

Color cnpRedonColor(struct track *tg, void *item, struct hvGfx *hvg);
/* always gray */

void cnpRedonMethods(struct track *tg);
/* methods for cnpRedon */

void cnpLockeLoadItems(struct track *tg);
/* loader for cnpLocke table */

void cnpLockeFreeItems(struct track *tg);

Color cnpLockeColor(struct track *tg, void *item, struct hvGfx *hvg);

void cnpLockeMethods(struct track *tg);
/* methods for cnpLocke */

Color cnpTuzunColor(struct track *tg, void *item, struct hvGfx *hvg);
/* always gray */

void cnpTuzunMethods(struct track *tg);
/* methods for cnpTuzun */

Color delConradColor(struct track *tg, void *item, struct hvGfx *hvg);
/* always red */

void delConradMethods(struct track *tg);
/* methods for delConrad */

Color delConrad2Color(struct track *tg, void *item, struct hvGfx *hvg);
/* always red */

void delConrad2Methods(struct track *tg);
/* methods for delConrad2 */

Color delMccarrollColor(struct track *tg, void *item, struct hvGfx *hvg);
/* always red */

void delMccarrollMethods(struct track *tg);
/* methods for delMccarroll */

Color delHindsColor(struct track *tg, void *item, struct hvGfx *hvg);
/* always red */

void delHindsMethods(struct track *tg);
/* methods for delHinds */

#endif
