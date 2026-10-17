/* dotBracketTrack - overlay an RNA secondary structure, written as a dot-bracket
 * string in a named bigBed column, onto a type bigBed track.  The string covers the
 * spliced item, one character per base of the blocks, in transcript orientation.
 * Spaces pad bases with no prediction.
 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef DOTBRACKETTRACK_H
#define DOTBRACKETTRACK_H

/* The trackDb setting names and the cart suffix are in hg/inc/hui.h, because
 * hgTrackUi needs them for the checkbox and cannot include this header. */

void dotBracketMethods(struct track *tg);
/* Set up a type bigBed track to overlay a dot-bracket string from a named column. */

#endif /* DOTBRACKETTRACK_H */
