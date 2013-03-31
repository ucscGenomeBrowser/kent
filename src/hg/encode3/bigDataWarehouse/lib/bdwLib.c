/* bdwLib - routines shared by various bigDataWarehouse programs.    See also bigDataWarehouse
 * module for tables and routines to access structs built on tables. */

#include "common.h"
#include "bdwLib.h"
#include "hex.h"
#include "openssl/sha.h"

char *bdwDatabase = "bigDataWarehouse";

void bdwMakeAccess(char *user, char *password, unsigned char access[BDW_SHA_SIZE])
/* Convert user + password + salt to an access code */
{
/* Salt it well with stuff that is reproducible but hard to guess, and some
 * one time true random stuff. Sneak in password and user too. */
unsigned char inputBuf[512];
memset(inputBuf, 0, sizeof(inputBuf));
int i;
for (i=0; i<ArraySize(inputBuf); i += 2)
    {
    inputBuf[i] = i ^ 0x3f;
    inputBuf[i+1] = -i*i;
    }
safef((char*)inputBuf, sizeof(inputBuf), "f186ed79bae%s8e364d73%s<*#$*(#)!DSDFOUIHLjksdf",
    user, password);
SHA512(inputBuf, sizeof(inputBuf), access);
}

#define bdwMaxEmailSize 128     /* Maximum size of an email handle */

int bdwCheckEmailSize(char *email)
/* Make sure email address not too long. Returns size or aborts if too long. */
{
int emailSize = strlen(email);
if (emailSize > bdwMaxEmailSize)
   errAbort("size of email address too long: %s", email);
return emailSize;
}

boolean bdwCheckAccess(char *user, char *password, struct sqlConnection *conn)
/* Make sure user exists and password checks out. */
{
/* Make escaped version of email string since it may be raw user input. */
int emailSize = bdwCheckEmailSize(user);
char escapedEmail[2*emailSize+1];
sqlEscapeString2(escapedEmail, user);

unsigned char access[BDW_SHA_SIZE];
bdwMakeAccess(user, password, access);

char query[256];
safef(query, sizeof(query), "select access from bdwUser where email='%s'", user);
struct sqlResult *sr = sqlGetResult(conn, query);
boolean gotMatch = FALSE;
char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (memcmp(row[0], access, sizeof(access)) == 0)
        gotMatch = TRUE;
    }
sqlFreeResult(&sr);
return gotMatch;
}

void bdwMustHaveAccess(char *user, char *password, struct sqlConnection *conn)
/* Check user has access and abort with an error message if not. */
{
if (!bdwCheckAccess(user, password, conn))
    errAbort("User/password combination doesn't give access to database");
}

void bdwMakeSid(char *user, unsigned char sid[BDW_SHA_SIZE])
/* Convert users to sid */
{
/* Salt it well with stuff that is reproducible but hard to guess, and some
 * one time true random stuff. Sneak in user too and time. */
unsigned char inputBuf[512];
memset(inputBuf, 0, sizeof(inputBuf));
int i;
for (i=0; i<ArraySize(inputBuf); i += 2)
    {
    inputBuf[i] = i ^ 0x29;
    inputBuf[i+1] = ~i;
    }
safef((char*)inputBuf, sizeof(inputBuf), 
	"186ED79BAEXzeusdioIsdklnw88e86cd73%s<*#$*(#)!DSDFOUIHLjksdf", user);
SHA512(inputBuf, sizeof(inputBuf), sid);
}

