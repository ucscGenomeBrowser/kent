/* quickLiftContainerTester - a quickLifted container is written shown, once.
 *
 * trackDbString writes the stanza a quickLift hub carries for a lifted track.  For a container
 * it writes "superTrack on show", and then dumpTdbAndChildren walks the source's settings, which
 * carry the source assembly's own superTrack setting.  That reads "on hide" whenever the user
 * has not opened the container, and the hub reader keeps the later line, so before #37969 the
 * container came across hidden and so did every track in it.  Now the source setting is
 * dropped before the walk.
 *
 * This prints every superTrack line of the stanza for a container whose source setting is
 * "on hide", and for one whose setting is "on show".
 *
 * refs #37969 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "cart.h"
#include "trackDb.h"
#include "hui.h"

struct dyString *trackDbString(struct cart *cart, struct trackDb *tdb);

static void showContainer(char *sourceSetting)
/* Write the stanza for a container whose source superTrack setting is sourceSetting, and print
 * its superTrack lines. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = "regress37969Container";
tdb->shortLabel = "container";
tdb->longLabel = "a container";
tdb->visibility = tvHide;
tdb->settingsHash = hashNew(0);
hashAdd(tdb->settingsHash, "superTrack", cloneString(sourceSetting));
hashAdd(tdb->settingsHash, "shortLabel", "container");
tdbMarkAsSuperTrack(tdb);

struct dyString *dy = trackDbString(cartOfNothing(), tdb);
printf("source setting \"superTrack %s\"\n", sourceSetting);
struct slName *line, *lineList = slNameListFromString(dy->string, '\n');
int count = 0;
for (line = lineList; line != NULL; line = line->next)
    if (startsWith("superTrack", line->name))
        {
        printf("  %s\n", line->name);
        ++count;
        }
printf("  %d superTrack line%s\n", count, count == 1 ? "" : "s");
}

int main(int argc, char *argv[])
{
showContainer("on hide");
showContainer("on show");
return 0;
}
