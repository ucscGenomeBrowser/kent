/* toDev64 - A program that copies data from the old hgwdev database to the new hgwdev database.. */
#include "common.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: toDev64.c,v 1.3 2006/05/19 01:58:08 kent Exp $";

boolean chrom = FALSE, prefix=FALSE, test=FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "toDev64 - A program that copies data from the old hgwdev database to the\n"
  "new hgwdev database.\n"
  "usage:\n"
  "   toDev64 database table\n"
  "options:\n"
  "   -chrom - if set will copy tables split by chromosome\n"
  "   -prefix - if set will copy all tables that start with 'table'\n"
  "   -test - if set it not will actually do the copy. (It just\n"
  "             prints the commands it will execute.)\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_BOOLEAN},
   {"prefix", OPTION_BOOLEAN},
   {"test", OPTION_BOOLEAN},
   {NULL, 0},
};


boolean isAllGoodChars(char *s)
/* Return TRUE if all characters are alphanumeric or _ */
{
char c;
while ((c = *s++) != 0)
    if (!isalnum(c) && c != '_')
         return FALSE;
return TRUE;
}

void execAndCheck(char *command)
/* Do a system on command, and abort with error message if
 * it fails */
{
puts(command);
if (!test)
    {
    int err = system(command);
    if (err != 0)
        errAbort("system call failed error code %d", err);
    }
}

void copyTable(char *oldHost, char *oldDir, char *newDir,
	char *database, char *table, boolean chrom, boolean prefix)
/* Copy table, maybe a bunch of related tables, from old to new. */
{
struct dyString *dy = dyStringNew(0);
if (sameWord(database, "mysql"))
    errAbort("Sorry, contact cluster-admin to work on accounts database.");
if (!isAllGoodChars(database) || !isAllGoodChars(table))
    errAbort("Database and table can only have alphanumeric chars or _");
if (!isAllGoodChars(oldHost) )
    errAbort("Host names can only have alphanumeric chars or _");

/* Compose and execute rsync command. */
dyStringAppend(dy, "rsync --rsh=rsh -av --progress ");
dyStringPrintf(dy, "%s:%s/%s/", oldHost, oldDir, database);
if (chrom)
    dyStringPrintf(dy, "chr*_");
dyStringPrintf(dy, "%s", table);
if (prefix)
    {
    if (strlen(table) < 3)
        errAbort("Must give at least three letters to table name with prefix option.");
    dyStringPrintf(dy, "*");
    }
else
    dyStringPrintf(dy, ".*");
dyStringPrintf(dy, " %s/%s/", newDir, database);
execAndCheck(dy->string);

/* Compose and execute chown command */
dyStringClear(dy);
dyStringPrintf(dy, "chown mysql.mysql %s/%s", newDir, database);
execAndCheck(dy->string);


/* Compose and execute flush command */
dyStringClear(dy);
dyStringPrintf(dy, 
	"mysqladmin --user=hguser --password=hguserstuff flush-tables");
execAndCheck(dy->string);
}

void toDev64(char *database, char *table)
/* toDev64 - A program that copies data from the old hgwdev database 
 * to the new hgwdev database.. */
{
copyTable("hgwdevold", "/var/lib/mysql", 
	"/var/lib/mysql", database, table, chrom, prefix);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
chrom = optionExists("chrom");
prefix = optionExists("prefix");
test = optionExists("test");
if (argc != 3)
    usage();
toDev64(argv[1], argv[2]);
return 0;
}
