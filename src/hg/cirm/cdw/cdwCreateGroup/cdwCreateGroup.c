/* cdwCreateGroup - Create a new group for access control.. */
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
  "cdwCreateGroup - Create a new group for access control.\n"
  "usage:\n"
  "   cdwCreateGroup name \"Descriptive sentence or two.\"\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwCreateGroup(char *name, char *description)
/* cdwCreateGroup - Create a new group for access control.. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwGroup *oldGroup = cdwGroupFromName(conn, name);
if (oldGroup != NULL)
    errAbort("Group exists: %s - %s", oldGroup->name, oldGroup->description);
char query[2*1024];
sqlSafef(query, sizeof(query), "insert cdwGroup (name, description) values ('%s', '%s');",
	name, description);
sqlUpdate(conn, query);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwCreateGroup(argv[1], argv[2]);
return 0;
}
