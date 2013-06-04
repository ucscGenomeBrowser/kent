/* sqlAddCol - Add new data to a column in database table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "sqlAddCol - Add new data to a column in database table\n"
  "usage:\n"
  "   sqlAddCol database table column keyVal.txt\n"
  "Where keyVal.tab is a file where the first word is the key for the row,\n"
  "and the rest of the line if the val field to fill in for that column of that\n"
  "row\n"
  "options:\n"
  "   -key=field What field to use for key.  Defaults to the primary key, then\n"
  "              'name', then 'acc', then 'id'.\n"
  "   -new=type  Add new column of given type.  Type can be 'int', 'varchar(255)',\n"
  "              and other legal SQL types.\n"
  "   -index     Add index too\n"
  "   -unique    Add unique index too\n"
  );
}

static struct optionSpec options[] = {
   {"key", OPTION_STRING},
   {"new", OPTION_STRING},
   {"index", OPTION_BOOLEAN},
   {"unique", OPTION_BOOLEAN},
   {NULL, 0},
};

void addNewColumn(struct sqlConnection *conn, char *table, char *column, char *type)
/* Add a new column to the table. */
{
char sql[512];
sqlSafef(sql, sizeof(sql), "ALTER TABLE %s ADD COLUMN %s %s not null", table, column, type);
sqlUpdate(conn, sql);
}


void sqlAddCol(char *database, char *table, char *column, char *keyValFile)
/* sqlAddCol - Add new data to a column in database table. */
{
struct sqlConnection *conn = sqlConnect(database);
char sql[512];
struct lineFile *lf;
char *key = NULL;
char *word, *line;
if (optionExists("new"))
    addNewColumn(conn, table, column, optionVal("new", NULL));
if (sqlFieldIndex(conn, table, column) < 0)
    errAbort("Column %s doesn't exists in %s.%s. Use new option to add a new column",
    	column, database, table);

/* Figure out key field. */
key = optionVal("key", NULL);
if (key == NULL)
    {
    key = sqlGetPrimaryKey(conn, table);
    if (key == NULL)
	{
	if (sqlFieldIndex(conn, table, "name") >= 0)
	     key = "name";
	else if (sqlFieldIndex(conn, table, "acc") >= 0)
	     key = "acc";
	else if (sqlFieldIndex(conn, table, "id") >= 0)
	     key = "id";
	else
	     errAbort("Please specify a key field for %s.%s", database, table);
	}
    }
else
    {
    if (sqlFieldIndex(conn, table, key) < 0)
        errAbort("Key %s doesn't exist in %s.%s", key, database, table);
    }

lf = lineFileOpen(keyValFile, TRUE);
while (lineFileNextReal(lf, &line))
    {
    word = nextWord(&line);
    sqlSafef(sql, sizeof(sql), "update %s set %s = '%s' where %s = '%s'",
    	table, column, line, key, word);
    sqlUpdate(conn, sql);
    }
lineFileClose(&lf);

if (optionExists("unique"))
    {
    sqlSafef(sql, sizeof(sql), "create unique index %s on %s (%s)",
    	column, table, column);
    sqlUpdate(conn, sql);
    }
else if (optionExists("index"))
    {
    sqlSafef(sql, sizeof(sql), "create index %s on %s (%s)",
    	column, table, column);
    sqlUpdate(conn, sql);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
sqlAddCol(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
