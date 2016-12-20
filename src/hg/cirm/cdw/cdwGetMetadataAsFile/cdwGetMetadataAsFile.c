/* cdwGetMetadataAsFile - A CGI script that gets the metadata for a particular dataset as a file.. */
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

/* Global vars */
struct cart *cart;	// User variables saved from click to click
struct hash *oldVars;	// Previous cart, before current round of CGI vars folded in
struct cdwUser *user;	// Our logged in user if any

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwGetMetadataAsFile - A CGI script that gets the metadata for a particular dataset as a file.\n"
  "usage:\n"
  "   cdwGetMetadataAsFile XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void setCdwUser(struct sqlConnection *conn)
/* set the global variable 'user' based on the current cookie */
{
char *userName = wikiLinkUserName();

// for debugging, accept the userName on the cgiSpoof command line
// instead of a cookie
if (hIsPrivateHost() && userName == NULL)
    userName = cgiOptionalString("userName");

if (userName != NULL)
    {
    /* Look up email vial hgCentral table */
    struct sqlConnection *cc = hConnectCentral();
    char query[512];
    sqlSafef(query, sizeof(query), "select email from gbMembers where userName='%s'", userName);
    char *email = sqlQuickString(cc, query);
    hDisconnectCentral(&cc);
    user = cdwUserFromEmail(conn, email);
    }
}

void cdwGetMetadataAsFile(struct sqlConnection *conn)
/* Determine the users's download format and generate a file that matches their
 * search criteria in the propper format */ 
{
/* Get values from cart variables and make up an RQL query string out of fields, where, limit
 * clauses, */

/* Fields clause */
char *fieldsVar = "cdwQueryFields";
char *fields = cartUsualString(cart, fieldsVar, "*");
struct dyString *rqlQuery = dyStringCreate("select %s from files ", fields);

/* Where clause */
char *whereVar = "cdwQueryWhere";
char *where = cartUsualString(cart, whereVar, "");
dyStringPrintf(rqlQuery, "where accession");
if (!isEmpty(where))
    dyStringPrintf(rqlQuery, " and (%s)", where);


/* Get the download format from the cart */ 
char *format = cartString(cart, "Download format");

/* Determine the users meta data access and generate a tagStorm tree of it. */ 
struct tagStorm *tags = cdwUserTagStorm(conn, user);


/* Identify and print the meta data that matches the users search. */
cdwPrintMatchingStanzas(rqlQuery->string, 1000000, tags, format);
}

void localWebWrap(struct cart *theCart)
/* We got the http stuff handled, and a cart. */
{
cart = theCart;
struct sqlConnection *conn = sqlConnect(cdwDatabase);
printf("Content-Type: text/plain\n\n");
setCdwUser(conn);
cdwGetMetadataAsFile(conn); 
sqlDisconnect(&conn);
}

char *excludeVars[] = {"submit", NULL};

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

