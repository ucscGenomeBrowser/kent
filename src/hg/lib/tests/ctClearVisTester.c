/* ctClearVisTester - a new BLAT result track does not start hidden.
 *
 * Custom track ids are deterministic from the track name, so a BLAT result track made again
 * under the same name gets the id of an earlier one.  If that earlier track was hidden and then
 * deleted, its visibility cart variable outlived it and started the new track hidden: with
 * "I'm feeling lucky" the user landed on hgTracks and the result was not there.  Since #38086
 * hgc's buildBigPsl calls customTracksClearCartVis on the tracks it creates.
 *
 * This runs it on a cart with no database behind it and prints what is left.
 *
 * refs #38086 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cart.h"
#include "trackDb.h"
#include "customTrack.h"

static struct customTrack *newCt(char *track)
/* Make a custom track with just enough of a trackDb to have a name, or none if track is NULL. */
{
struct customTrack *ct;
AllocVar(ct);
if (track != NULL)
    {
    AllocVar(ct->tdb);
    ct->tdb->track = track;
    }
return ct;
}

static void showVar(struct cart *cart, char *var)
/* Print a cart variable or that it is gone. */
{
char *val = cartOptionalString(cart, var);
printf("  %-24s %s\n", var, val == NULL ? "(gone)" : val);
}

int main(int argc, char *argv[])
{
struct cart *cart = cartOfNothing();
cartSetString(cart, "ct_blat_1234", "hide");      /* a deleted track of the same name */
cartSetString(cart, "ct_blat_5678", "hide");      /* another track, still in the list */
cartSetString(cart, "ct_blat_1234.priority", "3");
cartSetString(cart, "knownGene", "pack");

struct customTrack *newCts = newCt("ct_blat_1234");
slAddTail(&newCts, newCt(NULL));                 /* a track with no trackDb is skipped */

printf("before\n");
showVar(cart, "ct_blat_1234");
showVar(cart, "ct_blat_5678");
showVar(cart, "ct_blat_1234.priority");
showVar(cart, "knownGene");

customTracksClearCartVis(cart, newCts);

printf("after the new ct_blat_1234 is made\n");
showVar(cart, "ct_blat_1234");
showVar(cart, "ct_blat_5678");
showVar(cart, "ct_blat_1234.priority");
showVar(cart, "knownGene");
return 0;
}
