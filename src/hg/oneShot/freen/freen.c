/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"

static char const rcsid[] = "$Id: freen.c,v 1.54 2004/11/30 17:22:26 kent Exp $";

struct trackDb* loadTrackDb(struct sqlConnection *conn, char* where);
/* load list of trackDb objects, with optional where */

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *a)
/* Test some hair-brained thing. */
{
struct trackDb *tdbList, *tdb;
struct sqlConnection *conn;
hSetDb(a);
conn = hAllocConn();
// tdbList = loadTrackDb(conn, NULL);
// tdbList = trackDbLoadWhere(conn, "trackDb_kent", NULL);
tdbList = hTrackDb(NULL);

for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (startsWith("blat", tdb->tableName))
        printf("%s %s %s\n", tdb->tableName, tdb->type, tdb->shortLabel);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
