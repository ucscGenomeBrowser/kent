/* cdwGroupUser - Change user group settings.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"

boolean primary;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwGroupUser - Change user group settings.\n"
  "usage:\n"
  "   cdwGroupUser group user1@email.add ... userN@email.add\n"
  "options:\n"
  "   -primary Make this the new primary group (where user's file's initially live).\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"primary", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean cdwUserInGroup(struct sqlConnection *conn, unsigned int userId, unsigned int groupId)
/* Return TRUE if user is in group */
{
char query[256];
sqlSafef(query, sizeof(query), "select count(*) from cdwGroupUser where userId=%u and groupId=%u",
    userId, groupId);
return sqlQuickNum(conn, query) > 0;
}

void cdwGroupUser(char *groupName, int userCount, char *userEmails[])
/* cdwGroupUser - Change user group settings.. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwGroup *group = cdwNeedGroupFromName(conn, groupName);
 
/* Build up array of all users, in the process aborting if user not
 * found */
struct cdwUser *users[userCount];
int i;
for (i=0; i<userCount; ++i)
    users[i] = cdwMustGetUserFromEmail(conn, userEmails[i]);

/* Now go through user by user adding things */
for (i=0; i<userCount; ++i)
    {
    struct cdwUser *user = users[i];
    char query[256];
    if (!cdwUserInGroup(conn, user->id, group->id))
        {
	sqlSafef(query, sizeof(query), "insert into cdwGroupUser (userId,groupId) values (%u,%u)",
	    user->id, group->id);
	sqlUpdate(conn, query);
	}
    if (primary || user->primaryGroup == 0)
	{
	sqlSafef(query, sizeof(query), "update cdwUser set primaryGroup=%u where id=%u",
		group->id, user->id);
	sqlUpdate(conn, query);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
primary = optionExists("primary");
cdwGroupUser(argv[1], argc-2, argv+2);
return 0;
}
