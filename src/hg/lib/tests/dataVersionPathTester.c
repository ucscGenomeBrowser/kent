/* dataVersionPathTester - check which local files a track's dataVersion setting may read.
 *
 * A track's dataVersion setting is usually a version string, but for otto tracks it is the
 * path of a local file whose contents are the version, and hgTrackUi opens it and prints
 * what it finds.  A track hub is written by somebody else, so on a hub track that setting is
 * an instruction from a stranger to read a named file on our server and put it on the page.
 *
 * #38268 narrowed it: a hub track may name a file only under /gbdb, which is public data
 * mirrored on hgdownload, and only by a plain path, since a ".." component makes the name
 * mean something outside that tree.  Curated-hub assemblies need the /gbdb exception, hs1
 * and its neighbours being served as hubs, and so do quickLifted tracks, whose version file
 * lives on the source assembly.
 *
 * The failure is invisible in the ordinary sense and loud in the worst case: a refused path
 * and an accepted one both leave a page that looks reasonable, and the difference only shows
 * as somebody else's file contents appearing where a version number belongs.
 *
 * What is printed here is the CLASSIFICATION and never the file, for the obvious reason.
 * The three outcomes are distinguishable without looking at any contents: a refused path
 * comes back as the path itself, unread; an accepted path that does not exist comes back
 * NULL; an accepted path that exists comes back as something else entirely.
 *
 * refs #38268 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "trackDb.h"
#include "hui.h"

#define DB "hg38"

static struct trackDb *tdbWith(char *track, char *dataVersion)
/* A track carrying one dataVersion setting.  A track name beginning hub_ is what makes the
 * rest of the code treat it as somebody else's track. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = cloneString(track);
tdb->table = cloneString(track);
tdb->type = cloneString("bigBed 9 .");
char settings[512];
safef(settings, sizeof settings, "dataVersion %s\n", dataVersion);
tdb->settings = cloneString(settings);
tdb->settingsHash = trackDbSettingsFromString(tdb, tdb->settings);
return tdb;
}

static void try(char *what, char *track, char *dataVersion)
/* Ask what a track with this setting would print as its version, and say which of the three
 * things happened without showing any file. */
{
struct trackDb *tdb = tdbWith(track, dataVersion);
char *got = checkDataVersion(DB, tdb);
char *verdict;
if (got == NULL)
    verdict = "opened it, and there was no such file";
else if (sameString(got, dataVersion))
    verdict = "refused, the path came back unread";
else
    verdict = "OPENED AND READ IT";
printf("  %-34s %-34s %s\n", what, dataVersion, verdict);
}

int main(int argc, char *argv[])
{
char *hub = "hub_1_ottoTrack";
char *native = "ottoTrack";

printf("a hub track, which is somebody else's instruction\n");
try("under /gbdb, no such file", hub, "/gbdb/hg38/noSuchDataVersion.txt");
try("a file outside /gbdb", hub, "/etc/passwd");
try("climbing out of /gbdb", hub, "/gbdb/hg38/../../etc/passwd");
try("a prefix that only looks right", hub, "/gbdbNot/hg38/x.txt");
try("not a path at all", hub, "v27 2026-01-01");

printf("\na native track, which is ours\n");
try("no such file", native, "/no/such/dir/version.txt");
try("not a path at all", native, "v27 2026-01-01");

printf("\n");
return 0;
}
