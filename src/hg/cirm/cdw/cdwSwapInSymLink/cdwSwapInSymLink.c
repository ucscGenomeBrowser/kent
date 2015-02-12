/* cdwSwapInSymLink - Replace submission file with a symlink to file in warehouse.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwSwapInSymLink - Replace submission file with a symlink to file in warehouse.\n"
  "usage:\n"
  "   cdwSwapInSymLink startFileId endFileId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwSwapInSymLink(int startFileId, int endFileId)
/* cdwSwapInSymLink - Replace submission file with a symlink to file in warehouse.. */
{
struct sqlConnection *conn = cdwConnect();
struct cdwFile *ef, *efList = cdwFileAllIntactBetween(conn, startFileId, endFileId);
struct cdwSubmitDir *submitDir = NULL;
char query[PATH_LEN*2];
for (ef = efList; ef != NULL; ef = ef->next)
    {
    if (submitDir == NULL || submitDir->id != ef->submitDirId)
        {
	cdwSubmitDirFree(&submitDir);
	sqlSafef(query, sizeof(query), "select * from cdwSubmitDir where id=%d", ef->submitDirId);
	submitDir = cdwSubmitDirLoadByQuery(conn, query);
	if (submitDir == NULL)
	    errAbort("Can't find submitDir %u for %s", ef->submitDirId, ef->cdwFileName);
	}

    if (submitDir->url[0] == '/')  // Only do local files with absolute paths
        {
	char submitPath[PATH_LEN];
	safef(submitPath, sizeof(submitPath), "%s/%s", submitDir->url, ef->submitFileName);
	char cdwPath[PATH_LEN];
	safef(cdwPath, sizeof(cdwPath), "%s%s", cdwRootDir, ef->cdwFileName);
	verbose(1, "Making symlink %s->%s\n", submitPath, cdwPath);
	remove(submitPath);
	makeSymLink(cdwPath, submitPath);
	chmod(submitPath,0444);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwSwapInSymLink(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
