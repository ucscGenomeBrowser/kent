/* edwCreateUser - Create a new user from email/password combo.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "edwLib.h"
#include "hex.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwCreateUser - Create a new user from email/password combo.\n"
  "usage:\n"
  "   edwCreateUser user password\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwCreateUser(char *email, char *password)
/* edwCreateUser - Create a new user from email/password combo.. */
{
verbose(2, "edwCreateUser(user=%s, password=%s)\n", email, password);

/* Make escaped version of email string since it may be raw user input. */
int emailSize = edwCheckEmailSize(email);
char escapedEmail[2*emailSize+1];
sqlEscapeString2(escapedEmail, email);

/* Now make sure email is not already in user table. */
struct sqlConnection *conn = sqlConnect(edwDatabase);
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select count(*) from edwUser where email = '%s'", escapedEmail);
if (sqlQuickNum(conn, query->string) > 0)
    errAbort("User %s already exists", email);

/* Make up hash around our user name */
char sid[EDW_ACCESS_SIZE];
edwMakeSid(email, sid);
verbose(2, "sid=%s\n", sid);

/* Make up hash around our password */
char access[EDW_ACCESS_SIZE];
edwMakeAccess(email, password, access);
verbose(2, "access=%s\n", access);

/* Do database insert. */
dyStringClear(query);
dyStringPrintf(query, "insert into edwUser (sid, access, email) values('%s', '%s', '%s')",
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
edwCreateUser(argv[1], argv[2]);
return 0;
}
