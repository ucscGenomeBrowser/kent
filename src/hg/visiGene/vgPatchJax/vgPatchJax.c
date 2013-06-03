/* vgPatchJax - Patch Jackson labs part of visiGene database. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "ra.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgPatchJax - Patch Jackson labs part of visiGene database\n"
  "usage:\n"
  "   vgPatchJax database dir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void vgPatchJax(char *database, char *dir)
/* vgPatchJax - Patch Jackson labs part of visiGene database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct fileInfo *raList, *ra;
struct dyString *query = dyStringNew(0);

raList = listDirX(dir, "*.ra", TRUE);
for (ra = raList; ra != NULL; ra = ra->next)
    {
    struct hash *hash = raReadSingle(ra->name);
    char *submitSet = hashMustFindVal(hash, "submitSet");
    char *year = hashMustFindVal(hash, "year");
    dyStringClear(query);
    sqlDyStringPrintf(query,
    	"update submissionSet set year=%s "
	"where name = '%s'"
	, year, submitSet);
    sqlUpdate(conn, query->string);
    }

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
vgPatchJax(argv[1], argv[2]);
return 0;
}
