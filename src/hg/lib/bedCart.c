/*	bedCart.c - take care of parsing values from the
 *	bed trackDb optional settings and the same values that may be
 *	in the cart.
 */
#include "common.h"
#include "jksql.h"
#include "trackDb.h"
#include "cart.h"
#include "dystring.h"
#include "bedCart.h"

static char const rcsid[] = "$Id: bedCart.c,v 1.1 2004/11/23 22:21:28 hiram Exp $";

#if defined(NOT_YET)
extern struct cart *cart;      /* defined in hgTracks.c or hgTrackUi */
/*	This option isn't in the cart yet ... maybe later	*/
#endif

/******	itemRgb - not on by default **************************/
boolean bedItemRgb(struct trackDb *tdb)
{
char *Default="Off";
char *tdbDefault = trackDbSetting(tdb, OPT_ITEM_RGB);

if (tdbDefault)
    {
    if (differentWord(Default,tdbDefault))
	return TRUE;
    }

return FALSE;
}	/*	boolean bedItemRgb(struct trackDb *tdb)	*/
