/* Test functions in hdb.c */

#include "common.h"
#include "hdb.h"
#include "dbDb.h"
#include "liftOverChain.h"
#include "liftOver.h"

bool testLiftOverDatabase(char *db)
{
struct dbDb *dbList, *dbDb;

dbList = hGetLiftOverDatabases();
for (dbDb = dbList; dbDb != NULL; dbDb = dbDb->next)
    if (sameString(dbDb->name, db))
        return TRUE;
return FALSE;
}

int main(int argc, char *argv[])
{
    hSetDb("hg15");
    printf("hg15 organism ID = %d\n", hOrganismID("hg15"));
    printf("hg15 scientific name = %s\n", hScientificName("hg15"));

    if (testLiftOverDatabase("hg15"))
        printf("hg15 has a liftOver chain\n");

    return 0;
}
