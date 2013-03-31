/* bdwCreateUser - Create a new user from email/password combo.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "bdwLib.h"
#include "hex.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bdwCreateUser - Create a new user from email/password combo.\n"
  "usage:\n"
  "   bdwCreateUser user password\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void bdwCreateUser(char *email, char *password)
/* bdwCreateUser - Create a new user from email/password combo.. */
{
verbose(2, "bdwCreateUser(user=%s, password=%s)\n", email, password);

/* Make escaped version of email string since it may be raw user input. */
int emailSize = bdwCheckEmailSize(email);
char escapedEmail[2*emailSize+1];
sqlEscapeString2(escapedEmail, email);

/* Now make sure email is not already in user table. */
struct sqlConnection *conn = sqlConnect(bdwDatabase);
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select count(*) from bdwUser where email = '%s'", escapedEmail);
if (sqlQuickNum(conn, query->string) > 0)
    errAbort("User %s already exists", email);

/* Make up hash around our user name */
char sid[BDW_ACCESS_SIZE];
bdwMakeSid(email, sid);
verbose(2, "sid=%s\n", sid);

/* Make up hash around our password */
char access[BDW_ACCESS_SIZE];
bdwMakeAccess(email, password, access);
verbose(2, "access=%s\n", access);

/* Do database insert. */
dyStringClear(query);
dyStringPrintf(query, "insert into bdwUser (sid, access, email) values('%s', '%s', '%s')",
    sid, access, escapedEmail);
sqlUpdate(conn, query->string);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bdwCreateUser(argv[1], argv[2]);
return 0;
}
