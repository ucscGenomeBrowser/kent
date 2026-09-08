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

/* An explicit setting in the stanza wins, either way.  The "color" test below is only
 * about whether to turn itemRgb on by DEFAULT, so it must not be reached first: a stanza
 * that says both "itemRgb on" and "color" wants its items from the file's own RGB column
 * and its labels from color, which is what it got before 88d620e6c82 folded the two
 * tests into one early return. */
if (trackDbSettingOff(tdb, OPT_ITEM_RGB))
    return FALSE;

if (trackDbSettingOn(tdb, OPT_ITEM_RGB))
    return TRUE;

if (trackDbSettingClosestToHome(tdb, "color") != NULL)
    return FALSE;

if ((cfgOptionBooleanDefault("alwaysItemRgb", TRUE) == FALSE))
    return FALSE;

return TRUE;
}
