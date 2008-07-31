/* hgDeleteChrom - output SQL commands to yank a chromosome from chromInfo and
   all positional tables.  Handy for those assemblies still in flux... */
#include "common.h"
#include "portable.h"
#include "dystring.h"
#include "hdb.h"
#include "jksql.h"
#include "options.h"

static char const rcsid[] = "$Id: hgDeleteChrom.c,v 1.1.278.1 2008/07/31 02:24:35 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgDeleteChrom - output SQL commands to delete a chrom from the database\n"
  "usage:\n"
  "   hgDeleteChrom database chrom [chrom...]\n"
  "This will print SQL commands to delete chrom(s) from chromInfo and all\n"
  "positional tables in database.  Those commands can then be executed in\n"
  "hgsql.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hgDeleteChrom(char *db, int chromCount, char *chromNames[])
/* hgDeleteChrom - output SQL commands to delete chrom(s) from db. */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
char **row = NULL;
int i;
for (i=0;  i < chromCount;  i++)
    {
    char *chrom = hgOfficialChromName(db, chromNames[i]);
    if (chrom == NULL)
	errAbort("Error: \"%s\" is not a chromosome in %s.",
		 chromNames[i], db);
    printf("delete from %s.chromInfo where chrom = \"%s\";\n", db, chrom);
    sr = sqlGetResult(conn, "SHOW TABLES");
    while((row = sqlNextRow(sr)) != NULL)
	{
	char *table = row[0];
	char tChrom[32];
	char rootName[128];
	struct hTableInfo *hti = NULL;
	if (sscanf(table, "chr%32[^_]_random_%128s", tChrom, rootName) == 2 ||
	    sscanf(table, "chr%32[^_]_%128s", tChrom, rootName) == 2)
	    hti = hFindTableInfoDb(db, chrom, rootName);
	else
	    hti = hFindTableInfoDb(db, chrom, table);;
	if (hti->isPos)
	    {
	    if (hti->isSplit)
		{
		char tableCmp[256];
		safef(tableCmp, sizeof(tableCmp), "%s_%s", chrom, hti->rootName);
		if (sameWord(table, tableCmp))
		    printf("drop table %s.%s;\n", db, table);
		}
	    else
		printf("delete from %s.%s where %s = \"%s\";\n",
		       db, table, hti->chromField, chrom);
	    }
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
hgDeleteChrom(argv[1], argc-2, argv+2);
return 0;
}
