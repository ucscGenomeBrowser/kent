/*	chainCart.c - take care of parsing and combining values from the
 *	chain trackDb optional settings and the same values that may be
 *	in the cart.
 */
#include "common.h"
#include "jksql.h"
#include "trackDb.h"
#include "cart.h"
#include "dystring.h"
#include "hui.h"
#include "chainCart.h"

static char const rcsid[] = "$Id: chainCart.c,v 1.3 2009/03/18 18:27:00 hiram Exp $";

enum chainColorEnum chainFetchColorOption(struct cart *cart,
    struct trackDb *tdb, boolean compositeLevel)
/******	ColorOption - Chrom colors by default **************************/
{
char *chainColor = NULL;
enum chainColorEnum ret;

chainColor = trackDbSettingClosestToHomeOrDefault(tdb, OPT_CHROM_COLORS,
    CHROM_COLORS);
/* trackDb can be chainColor=normalizedScore, translate into pull-down
	menu equivalent */
if (sameWord(TDB_NORMALIZE_SCORE,chainColor))
    chainColor = SCORE_COLORS;
/* allow cart to override trackDb setting */
ret = chainColorStringToEnum(
	cartUsualStringClosestToHome(cart, tdb, compositeLevel,
	    OPT_CHROM_COLORS, chainColor));
return(ret);
}	/*	enum chainColorEnum chainFetchColorOption()	*/
