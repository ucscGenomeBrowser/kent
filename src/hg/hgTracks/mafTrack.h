/* mafTrack.h - MAF track display */

/* Copyright (C) 2009 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef MAFTRACK_H
#define MAFTRACK_H

#ifndef MAF_H
#include "maf.h"
#endif

#ifndef QUICKLIFT_H
#include "quickLift.h"
#endif

struct mafPriv
{
void *list;
struct customTrack *ct;
};

struct mafPriv *getMafPriv(struct track *track);

/* zoom level where summary file is used */
static inline char *quickLiftSummaryTable(char *summary)
/* The summary table of a quickLifted wigMaf track, in the assembly the track came from.  The
 * quickLift hub rewrites every file setting, summary among them, into a path under the hub
 * (trackSettingIsFile()), but a wigMaf summary is always a table, so it is the last part. */
{
char *slash = strrchr(summary, '/');
return (slash == NULL) ? summary : slash + 1;
}

static inline boolean inSummaryMode(struct cart *cart, struct trackDb *tdb, int winSize)
{
// The summary of a quickLifted maf track belongs to the assembly the track came from, so it
// is read in that assembly's coordinates and lifted, the way the blocks are
// (liftedSummariesToHash() in wigMafTrack.c).  Only a table can be read that way.  A bigMaf
// summary is a file, not a table, so a lifted bigMaf reads the real blocks and lifts them.
// With browser.quickLiftMafSummary off, every lifted maf reads the blocks.  refs #38513
if (quickLiftIsLifted(tdb))
    {
    if (!quickLiftMafSummaryEnabled(cart) || startsWithWord("bigMaf", tdb->type) ||
        (trackDbSetting(tdb, "summary") == NULL))
        return FALSE;
    }

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
