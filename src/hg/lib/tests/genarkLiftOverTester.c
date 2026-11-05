/* genarkLiftOverTester - check how genarkLiftOverDbs treats the list of accessions it is given.
 *
 * hgConvert asks this which GenArk assemblies a liftOver chain could reach.  Before #38328 the
 * caller pasted its accessions into one string and this function dropped that string straight
 * into a query marked NOSQLINJ, which is a promise that somebody upstream had made it safe.
 * Now the function takes an slName list and builds the query itself with sqlDyStringPrintf, so
 * the escaping happens where the values are, and a name that is not a GC accession never
 * reaches the database at all.
 *
 * None of that is visible.  A correctly escaped name and a badly escaped one give the same
 * page for every input a browser ever sends, because the accessions come from our own chain
 * files; the difference only appears for a value that was chosen to break out, which is
 * exactly the value that never turns up in testing.
 *
 * The genark table is data, and it grows and changes, so nothing here asserts that a
 * particular assembly is in it.  The test reads one accession out of the table and then asks
 * whether the function returns it, which is a property that stays true as the table changes.
 *
 * refs #38328 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "dbDb.h"
#include "genark.h"

static char *anAccession()
/* One accession that is in the genark table now, whichever one sorts first.  Reading it
 * rather than naming it keeps this test from going red the day GenArk drops an assembly. */
{
struct sqlConnection *conn = hConnectCentral();
char query[256];
sqlSafef(query, sizeof query, "select gcAccession from %s order by gcAccession limit 1",
         genarkTableName());
char *acc = sqlQuickString(conn, query);
hDisconnectCentral(&conn);
return acc;
}

static void report(char *what, struct slName *accList)
/* Ask, and say how many came back.  Names are not printed unless they came from the table,
 * since the table's contents are data that moves. */
{
struct dbDb *list = genarkLiftOverDbs(accList);
printf("  %-46s %d back\n", what, slCount(list));
}

int main(int argc, char *argv[])
{
char *real = anAccession();
if (isEmpty(real))
    errAbort("the genark table has no rows, so this test cannot say anything");

printf("an accession that is in the table\n");
report("just that one", slNameNew(real));

printf("\nnames that are not accessions\n");
struct slName *notAcc = slNameNew("hg38");
slNameAddHead(&notAcc, "mm39");
report("hg38 and mm39, so the query is never built", notAcc);

/* Starts with GC, so it passes the cheap test and reaches the query.  If it is not escaped
 * the quote ends the string literal and the rest is read as SQL.  What must happen is
 * nothing: no rows, no abort. */
struct slName *quoted = slNameNew("GC' or '1'='1");
report("GC' or '1'='1", quoted);

struct slName *semi = slNameNew("GCA_000001905.1'); select 1; --");
report("an accession with a statement after it", semi);

printf("\na list with both in it\n");
struct slName *mixed = slNameNew(real);
slNameAddHead(&mixed, "GC' or '1'='1");
slNameAddHead(&mixed, "notAnAccession");
report("one real, one quoted, one not an accession", mixed);

printf("\n");
return 0;
}
