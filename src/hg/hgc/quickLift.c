/* Details page for QuickLift track type */

/* Copyright (C) 2025 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hgc.h"
#include "jsHelper.h"
#include "htmshell.h"

void doQuickLiftChain(struct trackDb *tdb, char *item)
/* Deal with click on quickLift chain track.*/
{
char title[256];
safef(title, sizeof(title), "QuickLift Chain Details");
char header[256];
safef(header, sizeof(header), "%s %s", title, item);
cartWebStart(cart, database, "%s", header);
printf("doQuickLift\n");
htmlHorizontalLine();
printTrackHtml(tdb);
}
