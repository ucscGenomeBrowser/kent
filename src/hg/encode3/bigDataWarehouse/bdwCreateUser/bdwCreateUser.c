/* bdwCreateUser - Create a new user from email/password combo.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "hex.h"
#include "bdwLib.h"

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
unsigned char sid[BDW_SHA_SIZE];
char hexedSid[2*ArraySize(sid)+1];
bdwMakeSid(email, sid);
hexBinaryString(sid, sizeof(sid), hexedSid, sizeof(hexedSid));

/* Make up hash around our password */
unsigned char access[BDW_SHA_SIZE];
char hexedAccess[2*ArraySize(access)+1];
verbose(2, "bdwCreateUser(user=%s, password=%s)\n", email, password);
bdwMakeAccess(email, password, access);
hexBinaryString(access, sizeof(access), hexedAccess, sizeof(hexedAccess));
verbose(2, "hexedAccess=%s\n", hexedAccess);

/* Do database insert. */
dyStringClear(query);
dyStringPrintf(query, "insert into bdwUser (sid, access, email) values(0x%s, 0x%s, '%s')",
    hexedSid, hexedAccess, escapedEmail);
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
