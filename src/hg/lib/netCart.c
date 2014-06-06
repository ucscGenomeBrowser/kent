/*	netCart.c - take care of parsing and combining values from the
 *	net trackDb optional settings and the same values that may be
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
#include "netCart.h"


enum netColorEnum netFetchColorOption(struct cart *cart, struct trackDb *tdb,
	boolean parentLevel)
/******	netColorOption - Chrom colors by default **************************/
{
char *netColor = NULL;
enum netColorEnum ret;

netColor = trackDbSettingClosestToHomeOrDefault(tdb, NET_COLOR, CHROM_COLORS);
/* trackDb can be netColor=Chromosome or netColor=grayScale,
   need to translate grayScale into "Gray scale" */
if (sameWord(TDB_GRAY_SCALE,netColor))
    netColor = GRAY_SCALE;
/* and then, allow cart to override trackDb */
ret = netColorStringToEnum(
        cartUsualStringClosestToHome(cart, tdb, parentLevel, NET_COLOR, netColor));

return(ret);
}	/*	enum netColorEnum netFetchColorOption()	*/


enum netLevelEnum netFetchLevelOption(struct cart *cart, struct trackDb *tdb,
	boolean parentLevel)
/******	netLevelOption - net level 0 (All levels) by default    ***********/
{
char *netLevel = NULL;
enum netLevelEnum ret;

netLevel = trackDbSettingClosestToHomeOrDefault(tdb, NET_LEVEL, "0");
/* trackDb can be netLevel=0 thru netLevel=1, need to translate that
   number into enum strings */
/* assume "All levels" first */
ret = netLevelStringToEnum(NET_LEVEL_0);
switch(sqlUnsigned(netLevel))
    {
    case 0: break;
    case 1: ret = netLevelStringToEnum(NET_LEVEL_1); break;
    case 2: ret = netLevelStringToEnum(NET_LEVEL_2); break;
    case 3: ret = netLevelStringToEnum(NET_LEVEL_3); break;
    case 4: ret = netLevelStringToEnum(NET_LEVEL_4); break;
    case 5: ret = netLevelStringToEnum(NET_LEVEL_5); break;
    case 6: ret = netLevelStringToEnum(NET_LEVEL_6); break;
    default: warn("trackDb setting %s=%s for track %s is unrecognized, should be in range [0-6]",
	NET_LEVEL, netLevel, tdb->track);
	break;
    }

/* allow cart to override trackDb */
ret = netLevelStringToEnum(
        cartUsualStringClosestToHome(cart, tdb, parentLevel, NET_LEVEL, netLevelEnumToString(ret)));

return(ret);
}	/*	enum netLevelEnum netFetchLevelOption()	*/
