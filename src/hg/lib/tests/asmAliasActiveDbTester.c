/* asmAliasActiveDbTester - a db name with its own active dbDb row is not swapped for an alias.
 *
 * The asmAlias table maps a name to the assembly it is equivalent to.  Some names in it were
 * also assemblies of their own (GRCz12ab, calJac240_pri, monDom5), so asking for one of them
 * swapped it for its accession, and a session on that db fell back to hg38.  Since #38444,
 * asmAliasFindUnlessGenArk keeps a name that has an active dbDb row, as it already kept a
 * GenArk accession.  asmAliasFind itself still translates; it is printed beside each case to
 * show the alias row is there, so a kept name means the dbDb check kept it.
 *
 * The live asmAlias table no longer holds those rows (the otto update now leaves them out), so
 * the test makes its own rows in hgcentralregress and removes them at both ends.  See
 * makeHgCentralRegress.sh beside this file.
 *
 * refs #38444 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgConfig.h"
#include "asmAlias.h"

/* Names nobody would give a real assembly. */
#define ACTIVE_DB "regress38444active"
#define INACTIVE_DB "regress38444inactive"
#define NO_DB "regress38444nodb"
#define PLAIN_NAME "regress38444plain"
#define GENARK_ACC "GCA_938444001.1"
#define TARGET "GCF_938444999.1"

static void cleanUp()
/* Take this test's rows out again, at both ends of the run. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
sqlSafef(query, sizeof query, "delete from dbDb where name like 'regress38444%%'");
sqlUpdate(conn, query);
sqlSafef(query, sizeof query, "delete from asmAlias where browser = '%s'", TARGET);
sqlUpdate(conn, query);
sqlSafef(query, sizeof query, "delete from genark where gcAccession = '%s'", GENARK_ACC);
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static void setUp()
/* Every name gets an alias row pointing at TARGET; only some get a dbDb row. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
sqlSafef(query, sizeof query, "insert into dbDb (name, active) values ('%s', 1), ('%s', 0)",
         ACTIVE_DB, INACTIVE_DB);
sqlUpdate(conn, query);
sqlSafef(query, sizeof query, "insert into asmAlias values ('%s','%s'), ('%s','%s'), "
         "('%s','%s'), ('%s','%s')", ACTIVE_DB, TARGET, INACTIVE_DB, TARGET, NO_DB, TARGET,
         GENARK_ACC, TARGET);
sqlUpdate(conn, query);
sqlSafef(query, sizeof query, "insert into genark (gcAccession, hubUrl) values ('%s', '%s')",
         GENARK_ACC, "GCA/938/444/001/GCA_938444001.1/hub.txt");
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static void showCase(char *what, char *name)
/* Print what each lookup makes of name. */
{
printf("%s\n", what);
printf("  %-28s %s\n", "asmAliasFind", asmAliasFind(name));
printf("  %-28s %s\n", "asmAliasFindUnlessGenArk", asmAliasFindUnlessGenArk(name));
}

int main(int argc, char *argv[])
{
if (!sameString(cfgOption("central.db"), "hgcentralregress"))
    errAbort("this test writes rows, so it must be pointed at hgcentralregress, not %s",
             cfgOption("central.db"));
cleanUp();
setUp();

showCase("an active dbDb row and an alias row: kept", ACTIVE_DB);
showCase("an inactive dbDb row and an alias row: swapped", INACTIVE_DB);
showCase("no dbDb row, an alias row: swapped", NO_DB);
showCase("no rows at all: unchanged", PLAIN_NAME);
showCase("a GenArk accession with an alias row: kept", GENARK_ACC);
showCase("a hub_ name: unchanged", "hub_1234_" ACTIVE_DB);

cleanUp();
return 0;
}
