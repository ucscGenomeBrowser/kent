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

/******	itemRgb - on by default **************************/
boolean bedItemRgb(struct trackDb *tdb)
{
if (tdb == NULL)
   return TRUE;

if ((trackDbSettingClosestToHome(tdb, "color") != NULL) || trackDbSettingOff(tdb, OPT_ITEM_RGB))
    return FALSE;

if ((cfgOptionBooleanDefault("alwaysItemRgb", TRUE) == FALSE))
    return FALSE;

return TRUE;
}
