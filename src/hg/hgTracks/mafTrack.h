/* mafTrack.h - MAF track display */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef MAFTRACK_H
#define MAFTRACK_H

#ifndef MAF_H
#include "maf.h"
#endif

struct mafPriv
{
void *list;
struct customTrack *ct;
};

struct mafPriv *getMafPriv(struct track *track);

/* zoom level where summary file is used */
static inline boolean inSummaryMode(struct cart *cart, struct trackDb *tdb, int winSize)
{
char *snpTable = trackDbSetting(tdb, "snpTable");
unsigned summaryWindowSize = cartOrTdbInt(cart, tdb, "summaryWindowSize", 1000000);

boolean windowBigEnough =  (winSize > summaryWindowSize);
boolean doSnpMode = (snpTable != NULL) && cartOrTdbBoolean(cart, tdb, MAF_SHOW_SNP,FALSE);
return windowBigEnough && !doSnpMode;
}


/* zoom level that displays synteny breaks and nesting brackets */
#define MAF_DETAIL_VIEW 30000

void drawMafRegionDetails(struct mafAli *mafList, int height,
        int seqStart, int seqEnd, struct hvGfx *hvg, int xOff, int yOff,
        int width, MgFont *font, Color color, Color altColor,
        enum trackVisibility vis, boolean isAxt, boolean chainBreaks,
	boolean doSnpMode);
/* Draw wiggle/density plot based on scoring things on the fly. */

void drawMafChain(struct hvGfx *hvg, int xOff, int yOff, int width, int height,
                        boolean isDouble);
/* draw single or double chain line between alignments in MAF display */

#endif
