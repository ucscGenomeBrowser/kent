/* spLoadRankProp - rankProp table */
#include "common.h"
#include "options.h"
#include "jksql.h"

static char const rcsid[] = "$Id: spLoadRankProp.c,v 1.1 2004/08/22 20:11:16 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"append", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean gAppend = FALSE;  /* append to table? */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spLoadRankProp - load swissprot rankProp table.\n"
  "usage:\n"
  "   spLoadRankProp database table rankPropFile\n"
  "Options:\n"
  "  -append - append to the table instead of recreating it\n"
  "\n"
  "Load a file in the format:\n"
  "   prot1Acc prot2Acc ranking eVal12 eVal21\n"
  "\n"
  "\n"
  );
}

char createString[] =
    "CREATE TABLE %s ("
    "    prot1Acc varchar(8) not null,	# swissprot accession of first protein\n"
    "    prot2Acc varchar(8) not null,	# swissprot accession id of second protein\n"
    "    ranking float not null,	# rankp ranking\n"
    "    eVal12 float not null,	# prot1 to prot2 e-value\n"
    "    eVal21 float not null,	# prot2 to prot1 e-value\n"
    "              #Indices\n"
    "    index(prot1Acc),\n"
    "    index(prot2Acc)\n"
    ");";

void spLoadRankProp(char *database, char *table, char *rankPropFile)
/* spLoadRankProp - load a rankProp table */
{
char query[1024];
struct sqlConnection *conn = sqlConnect(database);
safef(query, sizeof(query), createString, table);
if (gAppend)
    sqlMaybeMakeTable(conn, table, query);
else
    sqlRemakeTable(conn, table, query);
sqlLoadTabFile(conn, rankPropFile, table, 0);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong # args");
gAppend = optionExists("append");
spLoadRankProp(argv[1], argv[2], argv[3]);
return 0;
}
