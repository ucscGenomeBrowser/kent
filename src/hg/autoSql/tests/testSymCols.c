/* Program to test symbolic columns (enum and set) in autoSql.  Requires
 * a writable database. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "output/symTest.h"


void usage()
{
errAbort("dbLinkTest - test symbolic columns (enum and set) in autoSql\n");
}

void writeTabFile(struct symTest *rows, char *outFile)
/* write out rows to tab file */
{
struct symTest *row;
FILE *fh = mustOpen(outFile, "w");
for (row = rows; row != NULL; row = row->next)
    symTestTabOut(row, fh);
carefulClose(&fh);
}

void testTabFile(char *inFile, char *outFile)
/* test reading and writing a tab-separated file */
{
struct symTest *rows = symTestLoadAllByTab(inFile);
writeTabFile(rows, outFile);
symTestFreeList(&rows);
}

void testTabToCommaFile(char *inFile, char *outFile)
/* create a comma file from a tab file */
{
struct symTest *rows = symTestLoadAllByTab(inFile);
struct symTest *row;
FILE *fh = mustOpen(outFile, "w");
for (row = rows; row != NULL; row = row->next)
    symTestCommaOut(row, fh);
carefulClose(&fh);
symTestFreeList(&rows);
}

void testCommaToTabFile(char *inFile, char *outFile)
/* create a tab file from a comma file */
{
char *buf;
readInGulp(inFile, &buf, NULL);

struct symTest *rows = NULL;
char *recPtr = buf;
while (*recPtr != '\0')
    slSafeAddHead(&rows, symTestCommaIn(&recPtr, NULL));
freeMem(buf);

slReverse(&rows);
writeTabFile(rows, outFile);
symTestFreeList(&rows);
}

char *loadSql(char *sqlFile)
/* load sql from file, removing comments */
{
struct lineFile *lf = lineFileOpen(sqlFile, TRUE);
struct dyString *sql = dyStringNew(0);
char *line;
while (lineFileNextReal(lf, &line)) 
    {
    char *h = strchr(line, '#');
    if (h != NULL)
        *h = '\0';
    sqlDyStringPrintf(sql, "%-s", line); // trusting the input
    }
lineFileClose(&lf);
return dyStringCannibalize(&sql);
}

void testDb(char *testDb, char *testTbl, char* sqlFile,
            char *inFile, char *outFile)
/* test loading and querying a database */
{
char *createSql = loadSql(sqlFile);
struct sqlConnection *conn = sqlConnect(testDb);

/* create a load */
sqlRemakeTable(conn, testTbl, createSql);
freeMem(createSql);
sqlLoadTabFile(conn, inFile, testTbl, 0);

/* query and save to file */
struct symTest *rows = sqlQueryObjs(conn, (sqlLoadFunc)symTestLoad,
                                    sqlQueryMust|sqlQueryMulti,
                                    "select * from %s", testTbl);
writeTabFile(rows, outFile);
symTestFreeList(&rows);
sqlDisconnect(&conn);
}

void doTests()
/* test symbolic columns (enum and set) in autoSql */
{
testTabFile("input/symColsTest.tab", "output/symColsTest.tab");
testTabToCommaFile("output/symColsTest.tab",  "output/symColsTest.comma");
testCommaToTabFile("output/symColsTest.comma",  "output/symColsTestComma.tab");
testDb("test", "symTest", "output/symTest.sql", "input/symColsTest.tab",
       "output/symColsTestDb.tab");
}

int main(int argc, char *argv[])
{
if(argc != 1)
    usage();
doTests();
return 0;
}
