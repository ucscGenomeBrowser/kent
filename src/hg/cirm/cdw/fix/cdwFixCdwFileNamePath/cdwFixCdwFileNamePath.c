/* cdwFixCdwFileNamePath - Fix case where root path included in cdwFileName. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFixCdwFileNamePath - Fix case where root path included in cdwFileName\n"
  "usage:\n"
  "   cdwFixCdwFileNamePath now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwFixCdwFileNamePath(char *dummy)
/* cdwFixCdwFileNamePath - Fix case where root path included in cdwFileName. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
char query[2*PATH_LEN];
sqlSafef(query, sizeof(query), 
    "select * from cdwFile where cdwFileName like '%s%%'", cdwRootDir);
struct cdwFile *file, *fileList = cdwFileLoadByQuery(conn, query);
uglyf("%d files that start with %s\n", slCount(fileList), cdwRootDir);
int skipSize = strlen(cdwRootDir);
for (file = fileList; file != NULL; file = file->next)
    {
    char *newName = file->cdwFileName + skipSize;
    sqlSafef(query, sizeof(query), "update cdwFile set cdwFileName='%s' where id=%u",
	newName, file->id);
    uglyf("%s\n", query);
    sqlUpdate(conn, query);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwFixCdwFileNamePath(argv[1]);
return 0;
}
