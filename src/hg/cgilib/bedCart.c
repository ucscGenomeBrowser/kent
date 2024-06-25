/*	bedCart.c - take care of parsing values from the
 *	bed trackDb optional settings and the same values that may be
 *	in the cart.
 */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "jksql.h"
#include "trackDb.h"
#include "cart.h"
#include "dystring.h"
#include "bedCart.h"
#include "hgConfig.h"


#if defined(NOT_YET)
extern struct cart *cart;      /* defined in hgTracks.c or hgTrackUi */
/*	This option isn't in the cart yet ... maybe later	*/
#endif

/******	itemRgb - not on by default **************************/
boolean bedItemRgb(struct trackDb *tdb)
{
char *Default="Off";	/* anything different than this will turn it on */
char *tdbDefault = (char *)NULL;

if (tdb)
    {
    tdbDefault = trackDbSettingClosestToHome(tdb, OPT_ITEM_RGB);

    // If the hg.conf statement is set on this server to activate the new behavior:
    // only default to "on" if:
    // - "color" is not present at all
    // - itemRgb=off is not set
    if (cfgOptionBooleanDefault("alwaysItemRgb", FALSE) && 
            (trackDbSettingClosestToHome(tdb, "color")==NULL) && 
            !sameOk(Default,tdbDefault))
        return TRUE;
    }

if (tdbDefault)
    {
    if (differentWord(Default,tdbDefault))
	return TRUE;
    }

return FALSE;
}	/*	boolean bedItemRgb(struct trackDb *tdb)	*/
