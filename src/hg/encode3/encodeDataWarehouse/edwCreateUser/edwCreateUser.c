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
  "   edwCreateUser user password email\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwCreateUser(char *user, char *password, char *email)
/* edwCreateUser - Create a new user from email/password combo.. */
{
verbose(2, "edwCreateUser(user=%s, password=%s, email=%s)\n", user, password, email);

/* Now make sure user is not already in user table. */
char *escapedUser = sqlEscapeString(user);
struct sqlConnection *conn = sqlConnect(edwDatabase);
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select count(*) from edwUser where name = '%s'", escapedUser);
if (sqlQuickNum(conn, query->string) > 0)
    errAbort("User %s already exists", user);

/* Make sure that email not already there.  If so remind user of his name. */
char *escapedEmail = sqlEscapeString(email);
dyStringClear(query);
dyStringPrintf(query, "select name from edwUser where email = '%s'", escapedEmail);
char *oldUserName = sqlQuickString(conn, query->string);
if (oldUserName)
    errAbort("email %s is already associated with user %s", email, oldUserName);

/* Make up hash around our user name */
char sid[EDW_ACCESS_SIZE];
edwMakeSid(email, sid);
verbose(2, "sid=%s\n", sid);

/* Make up hash around our password */
char access[EDW_ACCESS_SIZE];
edwMakeAccess(password, access);
verbose(2, "access=%s\n", access);

/* Do database insert. */
dyStringClear(query);
dyStringPrintf(query, 
    "insert into edwUser (name, sid, access, email) values('%s', '%s', '%s', '%s')",
    escapedUser, sid, access, escapedEmail);
sqlUpdate(conn, query->string);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
edwCreateUser(argv[1], argv[2], argv[3]);
return 0;
}
