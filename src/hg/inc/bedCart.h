/* bedCart.h - cart settings for bed options */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef BEDCART_H
#define BEDCART_H

/*	Options to be found in trackDb settings	*/
#define OPT_ITEM_RGB "itemRgb"
/* itemRgb when not present is default Off, or when present can be Off or On */

boolean bedItemRgb(struct trackDb *tdb);

#endif /* BEDCART_H */
