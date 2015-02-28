/* cdwFreen - Temporary scaffolding frequently repurposed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "intValTree.h"
#include "cdw.h"
#include "cdwLib.h"
#include "tagStorm.h"
#include "rql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFreen - Temporary scaffolding frequently repurposed\n"
  "usage:\n"
  "   cdwFreen user\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


void cdwFreen(char *userEmail)
/* cdwFreen - Temporary scaffolding frequently repurposed. */
{
uglyTime(NULL);
struct sqlConnection *conn = cdwConnect();
struct cdwUser *user = cdwMustGetUserFromEmail(conn, userEmail);
uglyTime("Got user");
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwFile");
struct cdwFile *ef, *efList = cdwFileLoadByQuery(conn, query);
uglyTime("Got %d files", slCount(efList));
int count = 0;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    if (cdwCheckAccess(conn, ef, user, cdwAccessRead))
        ++count;
    }
uglyTime("Checked access, %d passed", count);
struct rbTree *groupedFiles = cdwFilesWithSharedGroup(conn, user->id);
uglyTime("Made group tree of %d elements", groupedFiles->n);
int recount = 0;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    if (cdwQuickCheckAccess(groupedFiles, ef, user, cdwAccessRead))
        ++recount;
    }
uglyTime("Recounted quickly acces to %d", recount);
uglyTime("One more time that's %lld", cdwCountAccessible(conn, user));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwFreen(argv[1]);
return 0;
}
