/* annoGratorTester -- exercise anno* lib modules (in kent/src as well as kent/src/hg) */

#include "annoGratorQuery.h"
#include "annoStreamPgSnp.h"
#include "annoFormatTab.h"
#include "dystring.h"

void usage()
/* explain usage and exit */
{
errAbort(
    "annoGratorTester - test program for anno* lib modules\n\n"
    "usage:\n"
    "    annoGratorTester db\n"
//    "options:\n"
    );
}

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void pgSnpDbToTabOut(char *db, char *table, char *outFile)
/* Get data from a pgSnp database table and print all fields to tab-sep output. */
{
struct sqlConnection *conn = sqlConnect(db);
struct annoStreamer *pgSnpIn = annoStreamPgSnpNewDb(conn, table);
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

int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage();
char *db = argv[1];
char *table = "pgNA12878";
pgSnpDbToTabOut(db, table, "stdout");
return 0;
}
