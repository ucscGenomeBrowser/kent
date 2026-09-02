/* snapshotReaper - garbage-collect abandoned anonymous shareable-view snapshots.
 *
 * Shareable "view snapshot" sessions (see hg/lib/snapshotSession.c) are lightweight named sessions
 * created to back durable share links (e.g. a BLAT alignment).  Anonymous ones live under the
 * reserved user "l" with a "__"-prefixed name.  This reaper deletes those whose lastUse is older
 * than the TTL, along with their durable sessionData files, giving share links a "durable while
 * used, reaped when abandoned" lifetime.  It never touches real (non-"__") or non-anonymous
 * sessions.  Intended to run from the same cron as the trash cleaner.
 *
 * Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "hgConfig.h"
#include "snapshotSession.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snapshotReaper - delete abandoned anonymous view-snapshot sessions and their durable files.\n"
  "usage:\n"
  "   snapshotReaper [options]\n"
  "options:\n"
  "   -ttlDays=N   Reap anonymous snapshots not opened within N days.\n"
  "                Default: the hg.conf setting snapshot.ttlDays, else %d (~4 years).\n"
  "   -dryRun      Report how many would be reaped without deleting anything.\n",
  snapshotDefaultTtlDays);
}

static struct optionSpec options[] = {
    {"ttlDays", OPTION_INT},
    {"dryRun", OPTION_BOOLEAN},
    {NULL, 0},
};

int main(int argc, char *argv[])
{
optionInit(&argc, argv, options);
if (argc != 1)
    usage();
int ttlDays = snapshotDefaultTtlDays;
char *cfgTtl = cfgOption("snapshot.ttlDays");
if (isNotEmpty(cfgTtl))
    ttlDays = atoi(cfgTtl);
ttlDays = optionInt("ttlDays", ttlDays);        // command line wins over hg.conf
if (ttlDays < 1)
    errAbort("snapshotReaper: ttlDays must be at least 1 (got %d)", ttlDays);
boolean dryRun = optionExists("dryRun");

struct sqlConnection *conn = hConnectCentral();
int n = snapshotReapAnon(conn, ttlDays, dryRun);
hDisconnectCentral(&conn);
verbose(1, "%s %d abandoned anonymous snapshot(s) older than %d days.\n",
        dryRun ? "Would reap" : "Reaped", n, ttlDays);
return 0;
}
