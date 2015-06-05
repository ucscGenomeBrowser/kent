/* cdwGroupUser - Change user group settings.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"

boolean clPrimary;
boolean clRemove;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwGroupUser - Change user group settings.\n"
  "usage:\n"
  "   cdwGroupUser group user1@email.add ... userN@email.add\n"
  "options:\n"
  "   -primary Make this the new primary group (where user's file's initially live).\n"
  "   -remove Remove users from group rather than add\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"primary", OPTION_BOOLEAN},
   {"remove", OPTION_BOOLEAN},
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
    boolean inGroup = cdwUserInGroup(conn, user->id, group->id);
    if (!inGroup && !clRemove)
        {
	sqlSafef(query, sizeof(query), "insert into cdwGroupUser (userId,groupId) values (%u,%u)",
	    user->id, group->id);
	sqlUpdate(conn, query);
	}
    else if (inGroup && clRemove)
        {
	sqlSafef(query, sizeof(query), "delete from cdwGroupUser where userId=%u and groupId=%u",
	    user->id, group->id);
	sqlUpdate(conn, query);
	}

    /* Deal with primary group */
    if (clRemove)
        {
	if (group->id == user->primaryGroup)
	    {
	    /* If possible revert to another group.  Otherwise will end up primary group 0 which
	     * is ok too. */
	    sqlSafef(query, sizeof(query), 
		"select groupId from cdwGroupUser where userId=%u and groupId != %u",
		    user->id, group->id);
	    int newPrimary = sqlQuickNum(conn, query);
	    sqlSafef(query, sizeof(query), "update cdwUser set primaryGroup=%d where id=%u", 
		newPrimary, user->id);
	    sqlUpdate(conn, query);
	    }
	}
    else
	{
	if (clPrimary || user->primaryGroup == 0)
	    {
	    sqlSafef(query, sizeof(query), "update cdwUser set primaryGroup=%u where id=%u",
		    group->id, user->id);
	    sqlUpdate(conn, query);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
clPrimary = optionExists("primary");
clRemove = optionExists("remove");
cdwGroupUser(argv[1], argc-2, argv+2);
return 0;
}
