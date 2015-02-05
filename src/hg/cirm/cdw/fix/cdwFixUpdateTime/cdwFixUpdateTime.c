/* cdwFixUpdateTime - Change update times in warehouse to match those in cdwFile table.. */
#include "common.h"
#include <sys/types.h>
#include <utime.h>
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "errAbort.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFixUpdateTime - Change update times in warehouse to match those in cdwFile table.\n"
  "usage:\n"
  "   cdwFixUpdateTime now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwFixUpdateTime(char *dummy)
/* cdwFixUpdateTime - Change update times in warehouse to match those in cdwFile table.. */
{
struct sqlConnection *conn = cdwConnect();
char query[512];
sqlSafef(query, sizeof(query), 
    "select cdwFileName,updateTime from cdwFile where endUploadTime != 0");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *cdwFileName = row[0];
    long long updateTime = sqlLongLong(row[1]);
    uglyf("Updating %s to %lld\n", cdwFileName, updateTime);
    struct utimbuf utb = {cdwNow(), updateTime};
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", cdwRootDir, cdwFileName);
    int err = utime(path, &utb);
    if (err == -1)
        errnoWarn("Couldn't set update time for %s code %d", path, err);
    }
sqlFreeResult(&sr);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwFixUpdateTime(argv[1]);
return 0;
}
