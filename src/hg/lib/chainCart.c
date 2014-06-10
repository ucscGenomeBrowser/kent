/*	chainCart.c - take care of parsing and combining values from the
 *	chain trackDb optional settings and the same values that may be
 *	in the cart.
 */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "jksql.h"
#include "trackDb.h"
#include "cart.h"
#include "dystring.h"
#include "hui.h"
#include "chainCart.h"


enum chainColorEnum chainFetchColorOption(struct cart *cart,
                                          struct trackDb *tdb, boolean parentLevel)
/****** ColorOption - Chrom colors by default **************************/
{
char *chainColor = NULL;
enum chainColorEnum ret;

chainColor = trackDbSettingClosestToHomeOrDefault(tdb, OPT_CHROM_COLORS,
    CHROM_COLORS);
/* allow cart to override trackDb setting */
ret = chainColorStringToEnum(cartUsualStringClosestToHome(cart, tdb, parentLevel,
                                                          OPT_CHROM_COLORS, chainColor));
return(ret);
}       /*      enum chainColorEnum chainFetchColorOption()     */
