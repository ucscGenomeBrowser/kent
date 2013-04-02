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
  "The user typically is an email address\n"
  "options:\n"
  "   -xxx=XXX\n"
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
sqlDisconnect(&conn);
uglyAbort("Not yet implemented");
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
