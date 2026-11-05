/* asForDbTester - check that a GenArk accession is not looked for in MySQL.
 *
 * asForDb() fetches a track's autoSql, and to do that it may need a database connection.  The
 * database it is handed is usually an assembly name, but on a quickLifted track it is the
 * SOURCE assembly, and for a GenArk that arrives as a bare accession -- GCA_000001905.1 --
 * with no MySQL database of that name anywhere.  #38272 added the isGenArk() test, so the
 * connection is not attempted for one.
 *
 * Without it the CGI does not return an empty field list, it dies: hAllocConnTrack on a name
 * MySQL has never heard of aborts, and the reader gets an error page instead of a details
 * page.  That is invisible in the sense that matters here -- nothing about the track itself
 * looks wrong, and the failure only appears for the combination of a quickLift and a GenArk
 * source.
 *
 * The discriminating case is the last one.  A name that is neither a GenArk accession nor a
 * real database still aborts, which is what shows that the GenArk case is being let through
 * deliberately rather than by connection failures having become harmless.
 *
 * The accession is read from the genark table at run time.  GenArk grows and its rows change,
 * and a test that names an assembly goes red the day that assembly is dropped.
 *
 * It has to run against a config that sets genarkHubPrefix, because isGenArk() answers by
 * looking the accession up through that prefix and says no when it is unset.  A developer's
 * own ~/.hg.conf need not have it; the shared cgi-bin one does, which is why the makefile
 * points this at the conf a CGI actually reads.  Without that the GenArk case fails for a
 * reason that has nothing to do with the fix, which is how this test read the first time.
 *
 * refs #38272 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "errCatch.h"
#include "trackDb.h"
#include "asParse.h"
#include "genark.h"
#include "hui.h"

static struct trackDb *aTrack()
/* A plain table-backed track, so that asForDb has to decide whether to open a connection. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = cloneString("testTrack");
tdb->table = cloneString("testTrack");
tdb->type = cloneString("bed 9 .");
tdb->settings = cloneString("");
tdb->settingsHash = trackDbSettingsFromString(tdb, tdb->settings);
return tdb;
}

static void tryDb(char *what, char *database)
/* Ask for the autoSql with this database name and say whether the call survived. */
{
struct errCatch *errCatch = errCatchNew();
struct asObject *as = NULL;
if (errCatchStart(errCatch))
    as = asForDb(aTrack(), database);
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    /* The message carries a MySQL error number and the server's own wording, neither of
     * which this pins.  What matters is whether the abort came from trying to reach a
     * database of that name. */
    boolean aboutTheDb = stringIn(database, errCatch->message->string) != NULL;
    printf("  %-42s %s\n", what,
           aboutTheDb ? "aborted, trying to reach a database of that name"
                      : "aborted for some other reason");
    }
else
    printf("  %-42s %s\n", what,
           as == NULL ? "returned, no autoSql" : "returned one");
errCatchFree(&errCatch);
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn = hConnectCentral();
char query[256];
sqlSafef(query, sizeof query, "select gcAccession from %s order by gcAccession limit 1",
         genarkTableName());
char *acc = sqlQuickString(conn, query);
hDisconnectCentral(&conn);
if (isEmpty(acc))
    errAbort("the genark table has no rows, so this test cannot say anything");

printf("asForDb with\n");
tryDb("a GenArk accession", acc);
tryDb("an assembly that is a real database", "hg38");
tryDb("a name that is none of those", "notAnAssemblyAnywhere");
printf("\n");
return 0;
}
