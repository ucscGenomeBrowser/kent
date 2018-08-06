/* cdwServeTagStorm - serve up a tagStorm file in .txt format. */
#include <mysql/mysql.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "sqlSanity.h"
#include "htmshell.h"
#include "portable.h"
#include "trashDir.h"
#include "tagStorm.h"
#include "cart.h"
#include "cdw.h"
#include "hdb.h"
#include "hui.h"
#include "cdw.h"
#include "cdwValid.h"
#include "web.h"
#include "cdwLib.h"
#include "wikiLink.h"
#include "filePath.h"
#include "obscure.h" 
#include "rql.h" 

typedef MYSQL_RES *	STDCALL ResGetter(MYSQL *mysql);

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
struct cdwUser *user;	// Our logged in user if any

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwServeTagStorm - A CGI script that serves the meta.txt file for a CDW dataset as a text file.\n"
  "usage:\n"
  "   cdwGetMetadataAsFile XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static boolean cdwCheckAccessFromFileId(struct sqlConnection *conn, int fileId, struct cdwUser *user, int accessType)
/* Determine if user can access file given a file ID */
{
struct cdwFile *ef = cdwFileFromId(conn, fileId);
boolean ok = cdwCheckAccess(conn, ef, user, accessType);
cdwFileFree(&ef);
return ok;
}

static boolean cdwCheckFileAccess(struct sqlConnection *conn, int fileId, struct cdwUser *user)
/* Determine if the user can see the specified file. Return True if they can and False otherwise. */ 
{
return cdwCheckAccessFromFileId(conn, fileId, user, cdwAccessRead);
}

void setCdwUser(struct sqlConnection *conn)
/* set the global variable 'user' based on the current cookie */
{
char *userName = wikiLinkUserName();

// for debugging, accept the userName on the cgiSpoof command line
// instead of a cookie
if (!cgiIsOnWeb() && userName == NULL)
    userName = cgiOptionalString("userName");

if (userName != NULL)
    user = cdwUserFromUserName(conn, userName);
}

void cdwServeTagStorm(struct sqlConnection *conn)
/* Serve up a cirm data warehouse meta.txt tagstorm as a .txt file */ 
{
char *dataSet = cartString(cart, "cdwDataSet");
char *format = cartString(cart, "format"); 
char metaFileName[PATH_LEN];
safef(metaFileName, sizeof(metaFileName), "%s/%s", dataSet, "meta.txt");
int fileId = cdwFileIdFromPathSuffix(conn, metaFileName);
if (!cdwCheckFileAccess(conn, fileId, user))
    {
    // errAbort currently looks bad because our content-type is text/plain
    // but the default errAbort/warn handlers seem to think we have html output.
    printf("\nUnauthorized access to %s\n", metaFileName);
    exit(1);
    }
char *path = cdwPathForFileId(conn, fileId);
if (sameString(format,"tsv"))
    {
    fflush(stdout);
    char command[3*PATH_LEN];
    safef(command, sizeof(command), "./tagStormToTab %s stdout", path);
    mustSystem(command);
    }
if (sameString(format,"csv"))
    {
    fflush(stdout);
    char command[3*PATH_LEN];
    safef(command, sizeof(command), "./tagStormToCsv %s stdout", path);
    mustSystem(command);
    }
else if (sameString(format,"text"))
    {
    struct lineFile *lf = lineFileOpen(path, TRUE);
    char *line;
    while (lineFileNext(lf, &line, NULL))
	{
	printf("%s\n",line); 
	}
    lineFileClose(&lf);
    }
else
    {
    printf("Unknown format: %s\n", format);
    }
}

void localWebWrap(struct cart *theCart)
/* We got the http stuff handled, and a cart. */
{
cart = theCart;
struct sqlConnection *conn = sqlConnect(cdwDatabase);
printf("Content-Type: text/plain\n\n");
setCdwUser(conn);
cdwServeTagStorm(conn); 
sqlDisconnect(&conn);
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{

boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
oldVars = hashNew(0);
cartEmptyShellNoContent(localWebWrap, hUserCookie(), excludeVars, oldVars);
return 0;
}
