/* cdwFix5 - Fix problem where .vcf.gz files lost .gz suffix but stayed compressed.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFix5 - Fix problem where .vcf.gz files lost .gz suffix but stayed compressed.\n"
  "usage:\n"
  "   cdwFix5 now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwFix5()
/* cdwFix5 - Fix problem where .vcf.gz files lost .gz suffix but stayed compressed.. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
char query[512];
sqlSafef(query, sizeof(query), "select * from cdwFile where submitFileName like '%%.vcf.gz'");
struct cdwFile *ef, *efList = cdwFileLoadByQuery(conn, query);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    if (!endsWith(ef->cdwFileName, ".vcf.gz"))
        {
	uglyf("%d needs fix\n", ef->id);
	char oldPath[PATH_LEN], newPath[PATH_LEN];
	safef(oldPath, sizeof(oldPath), "%s%s", cdwRootDir, ef->cdwFileName);
	safef(newPath, sizeof(newPath), "%s.gz", oldPath);
	uglyf("mustRename(%s,%s)\n", oldPath, newPath);
	mustRename(oldPath, newPath);
	char newCdwName[PATH_LEN];
	safef(newCdwName, sizeof(newCdwName), "%s.gz", ef->cdwFileName);
	char query[PATH_LEN*2];
	sqlSafef(query, sizeof(query), "update cdwFile set cdwFileName='%s' where id=%u", 
	    newCdwName, ef->id);
	uglyf("update %s\n", query);
	sqlUpdate(conn, query);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwFix5();
return 0;
}
