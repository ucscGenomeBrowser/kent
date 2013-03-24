/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "ra.h"
#include "basicBed.h"
#include "mdb.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen val desiredVal\n");
}


void freen(char *input)
/* Test some hair-brained thing. */
{
struct sqlConnection *conn = sqlConnect("hg19");
struct mdb *mdb = mdbLoadByQuery(conn, "select * from metaDb order by binary obj,var");
printf("Got %d mdb\n", slCount(mdb));

mdb = mdbLoadByQuery(conn, "select obj,var,val from metaDb");
printf("Got %d mdb\n", slCount(mdb));

struct mdbObj *list = mdbObjsQueryAll(conn, "metaDb");
printf("Got %d mdbObjs\n", slCount(list));

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
