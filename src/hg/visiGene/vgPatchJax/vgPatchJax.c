/* vgPatchJax - Patch Jackson labs part of visiGene database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "portable.h"
#include "ra.h"

static char const rcsid[] = "$Id: vgPatchJax.c,v 1.1 2005/09/01 17:37:14 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgPatchJax - Patch Jackson labs part of visiGene database\n"
  "usage:\n"
  "   vgPatchJax database raTabDir\n"
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
struct fileInfo *oneRa, *raList = listDirX(dir, "*.ra", TRUE);
struct dyString *update = dyStringNew(0);
for (oneRa = raList; oneRa != NULL; oneRa = oneRa->next)
    {
    struct hash *ra = raReadSingle(oneRa->name);
    char *submitSet = hashFindVal(ra, "submitSet");
    char *pubUrl = hashFindVal(ra, "pubUrl");
    if (submitSet != NULL && pubUrl != NULL)
        {
	dyStringClear(update);
	dyStringPrintf(update, "update submissionSet set pubUrl = '");
	dyStringAppend(update, pubUrl);
	dyStringPrintf(update, "' where name='%s'", submitSet);
	sqlUpdate(conn, update->string);
	}
    hashFree(&ra);
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
