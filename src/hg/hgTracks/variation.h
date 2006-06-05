/* variation.h - hgTracks routines that are specific to the SNP and haplotype tracks */

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
#include "spaceSaver.h"
#include "ld.h"
#include "hash.h"
#include "ldUi.h"
#include "gfxPoly.h"
#include "memgfx.h"
#include "cnpIafrate.h"
#include "cnpSebat.h"
#include "cnpSharp.h"
#include "cnpSharpSample.h"
#include "cnpSharpCutoff.h"

/****** snpMap *******/

boolean snpMapSourceFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

boolean snpMapTypeFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

void loadSnpMap(struct track *tg);
/* Load up snpMarkers from database table to track items. */

void freeSnpMap(struct track *tg);
/* Free up snpMap items. */

Color snpMapColor(struct track *tg, void *item, struct vGfx *vg);
/* Return color of snpMap track item. */

void snpMapDrawItemAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, 
		      double scale, MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single snpMap item at position. */

void snpMapMethods(struct track *tg);
/* Make track for snps. */

/****** snp ******/

void filterSnpItems(struct track *tg, boolean (*filter)(struct track *tg, void *item));
/* Filter out items from track->itemList. */

boolean snpSourceFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

boolean snpMolTypeFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

boolean snpClassFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

boolean snpValidFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

boolean snpFuncFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

void loadSnp(struct track *tg);
void loadSnp125(struct track *tg);
/* Load up snps from database table to track items. */

void freeSnp(struct track *tg);
/* Free up snp items. */

Color snpColor(struct track *tg, void *item, struct vGfx *vg);
Color snp125Color(struct track *tg, void *item, struct vGfx *vg);
/* Return color of snp track item. */

void snpDrawItemAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, 
		   double scale, MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single snp item at position. */

void snpMethods(struct track *tg);
void snp125Methods(struct track *tg);
/* Make track for snps. */

/***** haplotypes *****/

char *perlegenName(struct track *tg, void *item);
/* return the actual perlegen name, in form xx/yyyy cut off xx/ return yyyy */

int haplotypeHeight(struct track *tg, struct linkedFeatures *lf, struct simpleFeature *sf);
/* if the item isn't the first or the last make it smaller */

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
    char           *name;      /* chromStart as a string */
    unsigned        n;         /* count of snps with valid LD values */
    unsigned        sumValues; /* sum of valid LD values */
};

void ldLoadItems(struct track *tg);
/* loadItems loads up items for the chromosome range indicated.   */

int ldTotalHeight(struct track *tg, enum trackVisibility vis);
/* Return total height. Called before and after drawItems. 
 * Must set height, lineHeight, heightPer */ 

void ldDrawItems(struct track *tg, int seqStart, int seqEnd,
		  struct vGfx *vg, int xOff, int yOff, int width, 
		  MgFont *font, Color color, enum trackVisibility vis);
/* Draw item list, one per track. */

void ldDrawItemAt(struct track *tg, void *item, struct vGfx *vg, 
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

Color cnpIafrateColor(struct track *tg, void *item, struct vGfx *vg);
/* green for GAIN, red for LOSS, blue for both */

void cnpIafrateMethods(struct track *tg);
/* methods for cnpIafrate */


void cnpSebatLoadItems(struct track *tg);
/* loader for cnpSebat table */

void cnpSebatFreeItems(struct track *tg);

Color cnpSebatColor(struct track *tg, void *item, struct vGfx *vg);
/* green for GAIN, red for LOSS, blue for both */

void cnpSebatMethods(struct track *tg);
/* methods for cnpSharp */


void cnpSharpLoadItems(struct track *tg);
/* loader for cnpSharp table */

void cnpSharpFreeItems(struct track *tg);

Color cnpSharpColor(struct track *tg, void *item, struct vGfx *vg);
/* green for GAIN, red for LOSS, blue for both */

void cnpSharpMethods(struct track *tg);
/* methods for cnpSharp */


void cnpFosmidLoadItems(struct track *tg);
/* loader for cnpFosmid table */

void cnpFosmidFreeItems(struct track *tg);

Color cnpFosmidColor(struct track *tg, void *item, struct vGfx *vg);
/* green for I, red for D */

void cnpFosmidMethods(struct track *tg);
/* methods for cnpFosmid */

Color delConradColor(struct track *tg, void *item, struct vGfx *vg);
/* always red */

void delConradMethods(struct track *tg);
/* methods for delConrad */

Color delMccarrollColor(struct track *tg, void *item, struct vGfx *vg);
/* always red */

void delMccarrollMethods(struct track *tg);
/* methods for delMccarroll */

Color delHindsColor(struct track *tg, void *item, struct vGfx *vg);
/* always red */

void delHindsMethods(struct track *tg);
/* methods for delHinds */
