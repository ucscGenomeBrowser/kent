/* Details page for Lorax multi-tree viewer track type */

/* Copyright (C) 2025 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hgc.h"
#include "jsHelper.h"

void doLorax(struct trackDb *tdb, char *item)
/* Display the Lorax multi-tree viewer. */
{
// Open page and get parameters from CGI
cartWebStart(cart, database, "%s: %s", genome, tdb->longLabel);
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");

// Print iframe (using tdb settings iframeUrl and iframeOptions)
printIframe(tdb, "");

// jsIncludeFile throws an error due to CSP when invoked via pop-up, but is necessary when
// "Enable pop-up when clicking items" is disabled in Genome Browser.
// When invoked via pop-up, hgTracks has already included lorax.js, so the error can be
// ignored because the file has already been loaded.
jsIncludeFile("lorax.js", NULL);

// jsInline still works in pop-up mode even though jsIncludeFile doesn't. (ASH has no idea why.)
jsInlineF("loraxView('%s', %d, %d, %d, %d);", chrom, winStart, winEnd, start, end);

printTrackHtml(tdb);
}
