/* chainCart.h - cart settings for chain color options */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CHAINCART_H
#define CHAINCART_H

/*	Option strings for TrackUi settings	*/
#define MAX_OPT_STRLEN	128

#define OPT_CHROM_COLORS "chainColor"
#define OPT_CHROM_FILTER "chromFilter"
#define CHAIN_SCORE_MAXIMUM	2000000000
/*	TrackUi menu items, completing the sentence:
 *	Color chains by:
 */
#define CHROM_COLORS "Chromosome"
#define SCORE_COLORS "Normalized Score"
#define NO_COLORS "Black"

extern enum chainColorEnum chainFetchColorOption(struct cart *cart,
                                                 struct trackDb *tdb, boolean parentLevel);
/******	ColorOption - Chrom colors by default **************************/

#endif /* CHAINCART_H */
