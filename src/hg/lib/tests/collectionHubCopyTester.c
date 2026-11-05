/* collectionHubCopyTester - how many copies of a track collection's hub file a session load makes.
 *
 * Before #38273, every session load in every CGI copied each track collection's hub file to a
 * fresh trash name and registered the copy in hgcentral.hubStatus to get a hub id.  Nothing
 * removed the old row: 81% of the 3.1 million hubStatus rows on the RR were these.  Behind the
 * collectionHubCopyOnWrite gate, a session load copies nothing, and hgCollection, the one
 * program that writes the file, asks for its own copy instead.
 *
 * This is a perf test, so it counts work, not seconds: the hub files written to trash and the
 * hubStatus rows added by three session loads.  The makefile runs it twice, with the gate set
 * on and set off by name, and the off run is the old behaviour, kept while the gate exists.
 *
 * The source hub file sits under sessionDataDir, where a saved session's hub file lives; one in
 * trash would count as a copy the cart already owns, and is never copied under the gate.
 * Outside a CGI the trash directory is ".", so the tester runs in output/run, and the makefile
 * points sessionDataDir there too.  The hubStatus rows go to hgcentralregress and are removed at both
 * ends.  See makeHgCentralRegress.sh.
 *
 * refs #38273 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "portable.h"
#include "jksql.h"
#include "hdb.h"
#include "hgConfig.h"
#include "cart.h"
#include "hubConnect.h"
#include "customComposite.h"
#include "trashDir.h"

#define LOADS 3

static void cleanUp()
/* Take this test's hubStatus rows out again, at both ends of the run. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
sqlSafef(query, sizeof query,
         "delete from %s where hubUrl like '%s/%%' or hubUrl like '%s/%%'",
         defaultHubStatusTableName, trashDir(), cfgOption("sessionDataDir"));
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static int countHubStatusRows()
/* Return the number of hubStatus rows that name a trash file. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
sqlSafef(query, sizeof query,
         "select count(*) from %s where hubUrl like '%s/%%' or hubUrl like '%s/%%'",
         defaultHubStatusTableName, trashDir(), cfgOption("sessionDataDir"));
int count = sqlQuickNum(conn, query);
hDisconnectCentral(&conn);
return count;
}

static int countCopies()
/* Return the number of hub files under trash/hgComposite, which trashDirDateFile puts in a dated
 * subdirectory. */
{
int count = 0;
char copyDir[PATH_LEN];
safef(copyDir, sizeof copyDir, "%s/hgComposite", trashDir());
struct slName *dir, *dirList = listDir(copyDir, "*");
for (dir = dirList; dir != NULL; dir = dir->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof path, "%s/%s", copyDir, dir->name);
    count += slCount(listDir(path, "*.txt"));
    }
return count;
}

int main(int argc, char *argv[])
{
if (!sameString(cfgOption("central.db"), "hgcentralregress"))
    errAbort("this test writes rows, so it must be pointed at hgcentralregress, not %s",
             cfgOption("central.db"));
if (!startsWith("/", cfgOptionDefault("sessionDataDir", "")))
    errAbort("sessionDataDir must be set to an absolute directory for this test");
cleanUp();
char sourceDir[PATH_LEN], sourceHub[PATH_LEN];
safef(sourceDir, sizeof sourceDir, "%s/collectionSource", cfgOption("sessionDataDir"));
safef(sourceHub, sizeof sourceHub, "%s/hub.txt", sourceDir);
makeDirsOnPath(sourceDir);
FILE *f = mustOpen(sourceHub, "w");
fprintf(f, "hub collection\nshortLabel collection\nlongLabel collection\nuseOneFile on\n"
        "email regressTest@example.org\n\ngenome hg38\n");
carefulClose(&f);

struct cart *cart = cartOfNothing();
cartSetString(cart, customCompositeCartName "-hg38", sourceHub);
int i;
for (i = 0;  i < LOADS;  ++i)
    cartCopyLocalHubsOnSessionLoad(cart);

printf("collectionHubCopyOnWrite %s, %d session loads\n",
       cartCollectionHubCopyOnWrite() ? "on" : "off", LOADS);
printf("  hub files copied to trash   %d\n", countCopies());
printf("  hubStatus rows added        %d\n", countHubStatusRows());
printf("  the cart still names the saved file   %s\n",
       sameString(sourceHub, cartString(cart, customCompositeCartName "-hg38")) ? "yes" : "no");

cleanUp();
return 0;
}
