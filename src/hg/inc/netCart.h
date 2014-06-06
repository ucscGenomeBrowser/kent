/* netCart.h - cart settings for net color and level options */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef NETCART_H
#define NETCART_H

#include "chainCart.h"	/* borrowing CHROM_COLORS from chainCart */

#define NET_COLOR	"netColor"
/* suffix for variable identification, and trackDb variable name */
#define GRAY_SCALE	"Gray scale"
/* value on pull-down menu for color */
#define TDB_GRAY_SCALE	"grayScale"
/* trackDb option string value */

#define NET_LEVEL	"netLevel"
/* suffix for variable identification, and trackDb variable name */
#define NET_LEVEL_0	"All levels"
#define NET_LEVEL_1	"level 1 only"
#define NET_LEVEL_2	"level 2 only"
#define NET_LEVEL_3	"level 3 only"
#define NET_LEVEL_4	"level 4 only"
#define NET_LEVEL_5	"level 5 only"
#define NET_LEVEL_6	"level 6 only"
/* pull-down menu items */

enum netColorEnum netFetchColorOption(struct cart *cart, struct trackDb *tdb,
                                      boolean parentLevel);
/******	netColorOption - Chrom colors by default **************************/

enum netLevelEnum netFetchLevelOption(struct cart *cart, struct trackDb *tdb,
                                      boolean parentLevel);
/******	netLevelOption - net level 0 (All levels) by default    ***********/

#endif /* NETCART_H */
