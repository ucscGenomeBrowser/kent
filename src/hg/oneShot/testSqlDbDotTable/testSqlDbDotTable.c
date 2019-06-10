/* testSqlDbDotTable - Invoke various jksql functions with db.table as table to make sure db.table notation is supported. */
#include "common.h"
#include "hdb.h"
#include "options.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testSqlDbDotTable - Invoke various jksql functions with db.table as table to make sure db.table notation is supported.\n"
  "usage:\n"
  "   testSqlDbDotTable db db.table\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void testSqlDbDotTable(char *db, char *dbDotTable)
/* testSqlDbDotTable - Invoke various jksql functions with db.table as table to make sure db.table notation is supported. */
{
struct sqlConnection *conn = hAllocConn(db);

printf("%s %s.\n", dbDotTable, (sqlTableExists(conn, dbDotTable) ? "exists" : "does not exist."));
printf("%s wild %s.\n", dbDotTable, (sqlTableWildExists(conn, dbDotTable) ? "exists" : "does not exist."));

struct slName *list = sqlListFields(conn, dbDotTable);
printf("%s has %d items from sqlListFields.\n", dbDotTable, slCount(list));

list = sqlFieldNames(conn, dbDotTable);
printf("%s has %d items from sqlFieldNames.\n", dbDotTable, slCount(list));

char *column = "chrom";
if (list)
    {
    if (sameString(list->name, "bin") && list->next)
        column = list->next->name;
    else
        column = list->name;
    }

printf("%s %s a %s column.\n", dbDotTable,
       (sqlColumnExists(conn, dbDotTable, column) ? "has" : "does not have"), column);

struct sqlResult *sr = sqlDescribe(conn, dbDotTable);
printf("%s returned %d columns from sqlDescribe.\n", dbDotTable, sqlCountColumns(sr));
sqlFreeResult(&sr);

printf("%s %s a row where %s = 'chr1'\n", dbDotTable,
       (sqlRowExists(conn, dbDotTable, column, "chr1") ? "has" : "does not have"), column);

printf("%s has %d columns.\n", dbDotTable, sqlCountColumnsInTable(conn, dbDotTable));

printf("%s has %d rows IfExists.\n", dbDotTable, sqlTableSizeIfExists(conn, dbDotTable));
printf("%s has %d rows.\n", dbDotTable, sqlTableSize(conn, dbDotTable));

printf("%s data size is %lu.\n", dbDotTable, sqlTableDataSizeFromSchema(conn, db, dbDotTable));

printf("%s index size is %lu.\n", dbDotTable, sqlTableIndexSizeFromSchema(conn, db, dbDotTable));

printf("%s created like '%s'.\n", dbDotTable, sqlGetCreateTable(conn, dbDotTable));

printf("%s index of '%s' is %d.\n", dbDotTable, column, sqlFieldIndex(conn, dbDotTable, column));

printf("%s last updated %s (%lu).\n", dbDotTable, sqlTableUpdate(conn, dbDotTable),
       sqlTableUpdateTime(conn, dbDotTable));

printf("%s primary key is %s.\n", dbDotTable, sqlGetPrimaryKey(conn, dbDotTable));

struct slName *sample = sqlRandomSampleConn(conn, dbDotTable, column, 1);
printf("%s.%s random sample: '%s' .\n", dbDotTable, column, sample ? sample->name : "NULL");

struct sqlFieldInfo *fiList = sqlFieldInfoGet(conn, dbDotTable);
printf("%s sqlFieldInfoGet returned %d items.\n", dbDotTable, slCount(fiList));

char *enumColumn = NULL;
struct sqlFieldInfo *fi;
for (fi = fiList;  fi != NULL;  fi = fi->next)
    {
    if (startsWith("enum", fi->type))
        {
        enumColumn = fi->field;
        break;
        }
    }
if (enumColumn)
    {
    char **enumRows = sqlGetEnumDef(conn, dbDotTable, enumColumn);
    printf("%s sqlGetEnumDef(%s) returned %s rows.\n", dbDotTable, enumColumn,
           (enumRows ? "some" : "no"));
    }
else
    printf("%s does not have an enum column, can't test sqlGetEnumDef.\n", dbDotTable);

//#*** Need write privileges... be careful!
if (0)
    {
    char dbTableCopyName[strlen(dbDotTable)+64];
    safef(dbTableCopyName, sizeof dbTableCopyName, "%sUnlikelySuffix", dbDotTable);
    sqlCopyTable(conn, dbDotTable, dbTableCopyName);
    printf("%s copied to %s.\n", dbDotTable, dbTableCopyName);
    char dbTableRenameName[strlen(dbDotTable)+64];
    safef(dbTableRenameName, sizeof dbTableRenameName, "%sRenamedSuffix", dbDotTable);
    sqlRenameTable(conn, dbTableCopyName, dbTableRenameName);
    printf("%s renamed to %s.\n", dbTableCopyName, dbTableRenameName);
    sqlDropTable(conn, dbTableRenameName);
    printf("%s dropped.\n", dbTableRenameName);
    char query[1024];
    sqlSafef(query, sizeof query, "select * from %s limit 1", dbDotTable);
    printf("%s%s made.\n", dbTableRenameName,
           (sqlMaybeMakeTable(conn, dbTableRenameName, query) ? "" : " not"));
    sqlDropTable(conn, dbTableRenameName);
    printf("%s dropped.\n", dbTableRenameName);
    }

char *connDb = sqlGetDatabase(conn);
if (differentStringNullOk(db, connDb))
    errAbort("Connection db changed from '%s' to '%s'!", db, connDb);
printf("Done.\n");
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
testSqlDbDotTable(argv[1], argv[2]);
return 0;
}
