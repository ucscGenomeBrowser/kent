/* variation.h - hgTracks routines that are specific to the SNP and haplotype tracks */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgTracks.h"
#include "snp.h"
#include "snpMap.h"
#include "hui.h"
#include "snpUi.h"
#include "spaceSaver.h"
#include "hgTracks.h"
#include "ld.h"
#include "gfxPoly.h"

void filterSnpItems(struct track *tg, boolean (*filter)(struct track *tg, void *item));
/* Filter out items from track->itemList. */

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
/* Load up snps from database table to track items. */

void freeSnp(struct track *tg);
/* Free up snp items. */

Color snpColor(struct track *tg, void *item, struct vGfx *vg);
/* Return color of snp track item. */

void snpDrawItemAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, 
		   double scale, MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single snp item at position. */

void snpMethods(struct track *tg);
/* Make track for snps. */

char *perlegenName(struct track *tg, void *item);
/* return the actual perlegen name, in form xx/yyyy cut off xx/ return yyyy */

int haplotypeHeight(struct track *tg, struct linkedFeatures *lf, struct simpleFeature *sf);
/* if the item isn't the first or the last make it smaller */

void haplotypeMethods(struct track *tg);
/* setup special methods for haplotype track */

void perlegenMethods(struct track *tg);
/* setup special methods for Perlegen haplotype track */


/* 20 shades from black to fully saturated of red/green/blue for
 * expression data. */
#define LD_DATA_SHADES 16
extern Color ldShadesOfGreen[LD_DATA_SHADES+1];
extern Color ldShadesOfRed[LD_DATA_SHADES+1];
extern Color ldShadesOfBlue[LD_DATA_SHADES+1];
extern boolean ldColorsMade; /* Have the shades of Green, Red, and Blue been allocated? */
extern int maxLdRgbShade;

void makeLdShades(struct vGfx *vg);
/* Allocate the LD shades of Red, Green and Blue */

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
