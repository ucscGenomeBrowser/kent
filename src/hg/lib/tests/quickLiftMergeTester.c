/* quickLiftMergeTester - a second quickLift updates a track already in the hub.
 *
 * Before #38198 the quickLift hub writer skipped any track whose name was already in the hub
 * file.  The first lift writes a container with all of its children, the ones switched off as
 * well, so from then on those names were taken: a subtrack switched on after the first lift
 * never reached the target assembly, and nor did a visibility change on a track already lifted.
 * Now writeMergedHubFile merges each lift into the file: a generated stanza replaces the old one
 * of the same track, in the slot the old one held so parents stay ahead of their children, and a
 * track not generated this time is kept, so tracks still accumulate across lifts.
 *
 * This writes the hub a first lift would leave, merges a second lift into it, and prints the
 * file.  The second lift turns child2 on, changes the container's visibility, and adds a new
 * track; trackB is not in it and must survive.
 *
 * refs #38198 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "dystring.h"
#include "obscure.h"
#include "trackHub.h"

static char *firstLift =
"hub quickLiftHubhg19\n"
"shortLabel QuickLift from hg19\n"
"longLabel QuickLift from hg19\n"
"useOneFile on\n"
"email genome-www@soe.ucsc.edu\n"
"\n"
"genome hg19\n"
"\n"
"track container\n"
"quickLifted on\n"
"compositeTrack on\n"
"visibility dense\n"
"\n"
"track child1\n"
"quickLifted on\n"
"parent container on\n"
"visibility dense\n"
"\n"
"track child2\n"
"quickLifted on\n"
"parent container off\n"
"visibility dense\n"
"\n"
"track trackB\n"
"quickLifted on\n"
"visibility pack\n"
"\n";

static char *secondLift =
"track container\n"
"quickLifted on\n"
"compositeTrack on\n"
"visibility full\n"
"\n"
"track child1\n"
"quickLifted on\n"
"parent container on\n"
"visibility dense\n"
"\n"
"track child2\n"
"quickLifted on\n"
"parent container on\n"
"visibility dense\n"
"\n"
"track trackC\n"
"quickLifted on\n"
"visibility squish\n"
"\n";

int main(int argc, char *argv[])
{
if (argc != 2)
    errAbort("usage: quickLiftMergeTester hubFile");
char *hubFile = argv[1];
FILE *f = mustOpen(hubFile, "w");
fputs(firstLift, f);
carefulClose(&f);

struct dyString *dy = dyStringCreate("%s", secondLift);
writeMergedHubFile(hubFile, "hg19", dy);

char *text;
readInGulp(hubFile, &text, NULL);
fputs(text, stdout);
return 0;
}
