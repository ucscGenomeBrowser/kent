/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "jksql.h"
#include "mysqlTableStatus.h"

static char const rcsid[] = "$Id: freen.c,v 1.43 2004/03/17 02:19:52 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *in)
/* Test some hair-brained thing. */
{
struct sqlConnection *conn = sqlConnect(in);
struct sqlConnection *conn2 = sqlConnect(in);
struct sqlResult *sr;
char query[256], **row;
sr = sqlGetResult(conn, "show table status");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct mysqlTableStatus mst;
    mysqlTableStatusStaticLoad(row, &mst);
    printf("%s\t%d\t%s\t%d\t%d\n", mst.name, mst.rowCount, 
    	mst.updateTime, sqlDateToUnixTime(mst.updateTime),
	sqlTableUpdateTime(conn2, mst.name) );
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
