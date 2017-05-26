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

void cdwServeTagStorm(struct sqlConnection *conn)
/* Serve up a cirm data warehouse meta.txt tagstorm as a .txt file */ 
{
char *dataSet = cartString(cart, "cdwDataSet");
char *format = cartString(cart, "format"); 
char metaFileName[PATH_LEN];
safef(metaFileName, sizeof(metaFileName), "%s/%s", dataSet, "meta.txt");
int fileId = cdwFileIdFromPathSuffix(conn, metaFileName);
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
