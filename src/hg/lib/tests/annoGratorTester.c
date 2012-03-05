/* annoGratorTester -- exercise anno* lib modules (in kent/src as well as kent/src/hg) */

#include "annoGratorQuery.h"
#include "annoStreamDb.h"
#include "annoFormatTab.h"
#include "dystring.h"
#include "pgSnp.h"

// Names of tests:
static const char *pgSnpDbToTabOut = "pgSnpDbToTabOut";
static const char *pgSnpKgDbToTabOut = "pgSnpKgDbToTabOut";

void usage()
/* explain usage and exit */
{
errAbort(
    "annoGratorTester - test program for anno* lib modules\n\n"
    "usage:\n"
    "    annoGratorTester db [testName]\n"
//    "options:\n"
    "testName can be one of the following:\n"
    "    %s\n"
    "    %s\n"
    , pgSnpDbToTabOut, pgSnpKgDbToTabOut
    );
}

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void dbToTabOut(char *db, char *table, struct asObject *asObj, char *outFile)
/* Get data from a pgSnp database table and print all fields to tab-sep output. */
{
struct sqlConnection *conn = sqlConnect(db);
struct annoStreamer *pgSnpIn = annoStreamDbNew(conn, table, asObj);
struct annoFormatter *tabOut = annoFormatTabNew(outFile);
struct dyString *dbDotTwoBit = dyStringCreate("/hive/data/genomes/%s/%s.2bit", db, db);
struct twoBitFile *tbf = twoBitOpen(dbDotTwoBit->string);
struct annoGratorQuery *query = annoGratorQueryNew(db, NULL, tbf, pgSnpIn, NULL, tabOut);
annoGratorQuerySetRegion(query, "chr1", 705881, 752721);
annoGratorQueryExecute(query);
annoGratorQueryFree(&query);
sqlDisconnect(&conn);
dyStringFree(&dbDotTwoBit);
}

void twoDbToTabOut(char *db, char *table1, struct asObject *asObj1,
		   char *table2, struct asObject *asObj2, char *outFile)
/* Integrate data from a pgSnp database table and a knownGene db table
 * and print all fields to tab-sep output. */
{
struct sqlConnection *conn = sqlConnect(db);
struct annoStreamer *primary = annoStreamDbNew(conn, table1, asObj1);
struct sqlConnection *conn2 = sqlConnect(db);
struct annoStreamer *kgIn = annoStreamDbNew(conn2, table2, asObj2);
struct annoGrator *geneGrator = annoGratorGenericNew(primary, kgIn);
struct annoFormatter *tabOut = annoFormatTabNew(outFile);

struct dyString *dbDotTwoBit = dyStringCreate("/hive/data/genomes/%s/%s.2bit", db, db);
struct twoBitFile *tbf = twoBitOpen(dbDotTwoBit->string);
struct annoGratorQuery *query = annoGratorQueryNew(db, NULL, tbf, primary, geneGrator, tabOut);

annoGratorQuerySetRegion(query, "chr1", 705881, 752721);
annoGratorQueryExecute(query);

annoGratorQueryFree(&query);
sqlDisconnect(&conn);
sqlDisconnect(&conn2);
dyStringFree(&dbDotTwoBit);
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2 || argc > 3)
    usage();
char *db = argv[1];
char *test = NULL;
boolean doAllTests = (argc == 2);
if (!doAllTests)
    {
    if (sameString(argv[2], pgSnpDbToTabOut) ||
	sameString(argv[2], pgSnpKgDbToTabOut))
	test = cloneString(argv[2]);
    else
	{
	warn("Unrecognized test name '%s'\n", argv[2]);
	usage();
	}
    }

// First test: some rows of a pgSnp table
char *table1 = "pgNA12878";
struct asObject *asObj1 = pgSnpAsObj();
if (doAllTests || sameString(test, pgSnpDbToTabOut))
    dbToTabOut(db, table1, asObj1, "stdout");

// Second test: some rows of a pgSnp table integrated with knownGene
if (doAllTests || sameString(test, pgSnpKgDbToTabOut))
    {
    char *table2 = "knownGene";
    struct asObject *asObj2 = asParseFile("../knownGene.as");
    twoDbToTabOut(db, table1, asObj1, table2, asObj2, "stdout");
    }

return 0;
}
