/* edwResetPassword - Reset password for users.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwResetPassword - Reset password for users.\n"
  "usage:\n"
  "   edwResetPassword user newpassword\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwResetPassword(char *user, char *newPassword)
/* edwResetPassword - Reset password for users.. */
{
struct sqlConnection *conn = sqlConnect(edwDatabase);
char *escapedUser = sqlEscapeString(user);
char query[256];
safef(query, sizeof(query), "select id from edwUser where name = '%s'", escapedUser);
int userId = sqlQuickNum(conn, query);
if (userId == 0)
    errAbort("User %s does not exist.", user);

/* Make up hash around our password */
char access[EDW_ACCESS_SIZE];
edwMakeAccess(newPassword, access);

/* Save it in DB */
safef(query, sizeof(query), "update edwUser set access='%s'", access);
sqlUpdate(conn, query);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwResetPassword(argv[1], argv[2]);
return 0;
}
