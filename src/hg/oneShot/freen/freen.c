/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"

static char const rcsid[] = "$Id: freen.c,v 1.55 2004/12/03 13:56:30 kent Exp $";

struct trackDb* loadTrackDb(struct sqlConnection *conn, char* where);
/* load list of trackDb objects, with optional where */

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *db)
/* Test some hair-brained thing. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row, *query;

uglyf("connected to %s\n", db);
query = "show tables like '%gap%'";
sr = sqlGetResult(conn, query);
uglyf("sr = %p\n", sr);
while ((row = sqlNextRow(sr)) != NULL)
    {
    uglyf("%s\n", row[0]);
    }
uglyf("done with rows\n");
sqlFreeResult(&sr);
uglyf("Freed result\n");
sqlDisconnect(&conn);
uglyf("disconnected\n");
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
