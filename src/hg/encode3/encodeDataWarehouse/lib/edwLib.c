/* edwLib - routines shared by various encodeDataWarehouse programs.    See also encodeDataWarehouse
 * module for tables and routines to access structs built on tables. */

#include "common.h"
#include "edwLib.h"
#include "hex.h"
#include "openssl/sha.h"
#include "base64.h"

char *edwDatabase = "encodeDataWarehouse";
char *edwRootDir = "/scratch/kent/encodeDataWarehouse/";

static void makeShaBase64(unsigned char *inputBuf, int inputSize, char out[EDW_ACCESS_SIZE])
/* Make zero terminated printable cryptographic hash out of in */
{
unsigned char shaBuf[48];
SHA384(inputBuf, inputSize, shaBuf);
char *base64 = base64Encode((char*)shaBuf, sizeof(shaBuf));
memcpy(out, base64, EDW_ACCESS_SIZE);
out[EDW_ACCESS_SIZE-1] = 0; 
freeMem(base64);
}

void edwMakeAccess(char *user, char *password, char access[EDW_ACCESS_SIZE])
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
makeShaBase64(inputBuf, sizeof(inputBuf), access);
}

#define edwMaxEmailSize 128     /* Maximum size of an email handle */

int edwCheckEmailSize(char *email)
/* Make sure email address not too long. Returns size or aborts if too long. */
{
int emailSize = strlen(email);
if (emailSize > edwMaxEmailSize)
   errAbort("size of email address too long: %s", email);
return emailSize;
}

boolean edwCheckAccess(struct sqlConnection *conn, char *user, char *password, 
    char retSid[EDW_ACCESS_SIZE])
/* Make sure user exists and password checks out. */
{
/* Make escaped version of email string since it may be raw user input. */
int emailSize = edwCheckEmailSize(user);
char escapedEmail[2*emailSize+1];
sqlEscapeString2(escapedEmail, user);

char access[EDW_ACCESS_SIZE];
edwMakeAccess(user, password, access);

char query[256];
safef(query, sizeof(query), "select access,sid from edwUser where email='%s'", user);
struct sqlResult *sr = sqlGetResult(conn, query);
boolean gotMatch = FALSE;
char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (memcmp(row[0], access, sizeof(access)) == 0)
	{
	memcpy(retSid, row[1], EDW_ACCESS_SIZE);
        gotMatch = TRUE;
	}
    }
sqlFreeResult(&sr);
return gotMatch;
}

void edwMustHaveAccess(struct sqlConnection *conn, char *user, char *password,
    char retSid[EDW_ACCESS_SIZE])
/* Check user has access and abort with an error message if not. */
{
if (!edwCheckAccess(conn, user, password, retSid))
    errAbort("User/password combination doesn't give access to database");
}

void edwMakeSid(char *user, char sid[EDW_ACCESS_SIZE])
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
makeShaBase64(inputBuf, sizeof(inputBuf), sid);
}

