/* variation.h - hgTracks routines that are specific to the SNP and haplotype tracks */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgTracks.h"
#include "snp.h"
#include "snpMap.h"
#include "hui.h"
#include "spaceSaver.h"
#include "hgTracks.h"

char *snpSourceCart[snpSourceCount];
char *snpTypeCart[snpTypeCount];

void filterSnpItems(struct track *tg, boolean (*filter)(struct track *tg, void *item));
/* Filter out items from track->itemList. */

boolean snpSourceFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

boolean snpTypeFilterItem(struct track *tg, void *item);
/* Return TRUE if item passes filter. */

void loadSnpMap(struct track *tg);
/* Load up snpMarkers from database table to track items. */

void freeSnpMap(struct track *tg);
/* Free up snpMap items. */

Color snpMapColor(struct track *tg, void *item, struct vGfx *vg);
/* Return color of snpMap track item. */

void snpMapDrawItemAt(struct track *tg, void *item, 
	struct vGfx *vg, int xOff, int y, 
        double scale, MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single snpMap item at position. */

void snpMapMethods(struct track *tg);
/* Make track for snps. */

char *perlegenName(struct track *tg, void *item);
/* return the actual perlegen name, in form xx/yyyy cut off xx/ return yyyy */

int haplotypeHeight(struct track *tg, struct linkedFeatures *lf,
                    struct simpleFeature *sf);
/* if the item isn't the first or the last make it smaller */

void haplotypeMethods(struct track *tg);
/* setup special methods for haplotype track */

void perlegenMethods(struct track *tg);
/* setup special methods for Perlegen haplotype track */


