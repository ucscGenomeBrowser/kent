/* hgFacetedBars - A stand alone to show a faceted barchart selection.. */
#include <sys/time.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "udc.h"
#include "knetUdc.h"
#include "genbank.h"
#include "tablesTables.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
char *database = NULL;
char *genome = NULL;

void doBody()
{
struct sqlConnection *conn = sqlConnect(database);
struct hash *emptyHash = hashNew(0);

/* Write out html to pull in the other files we use. */
webIncludeResourceFile("facets.css");
printf("\t\t<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js\"></script>");
printf("\t\t<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\"\n"
    "\t\t integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\"\n"
    "\t\t crossorigin=\"anonymous\">\n"
    "\n"
    "\t\t<!-- Latest compiled and minified CSS -->\n"
    "\t\t<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css\"\n"
    "\t\t integrity=\"sha384-Gn5384xqQ1aoWXA+058RXPxPg6fy4IWvTNh0E263XmFcJlSAwiGgFAW/dAiS6JXm\"\n"
    "\t\t crossorigin=\"anonymous\">\n"
    "\n"
    "\t\t<!-- Latest compiled and minified JavaScript -->\n"
    "\t\t<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js\"\n"
    "\t\t integrity=\"sha384-JZR6Spejh4U02d8jOt6vLEHfe/JQGiRRSQQxSfFWpi1MquVdAyjUar5+76PVCmYl\"\n"
    "\t\t crossorigin=\"anonymous\"></script>\n"
    );

/* Working within a form we save context */
printf("<form action=\"../cgi-bin/hgFacetedBars\" name=\"facetForm\" method=\"GET\">\n");
cartSaveSession(cart);

/* Fake up a 'track' for development */
char *trackName = "cellFacetsJk1";

/* Set up url that has enough context to get back to us.  This is very much a work in 
 * progress. */
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/hgFacetedBars?%s",
    cartSidUrlString(cart) );

/* If we got called by a click on a facet deal with that */
char *selOp = cartOptionalString(cart, "browseFiles_facet_op");
if (selOp)
    {
    char *selFieldName = cartOptionalString(cart, "browseFiles_facet_fieldName");
    char *selFieldVal = cartOptionalString(cart, "browseFiles_facet_fieldVal");
    if (selFieldName && selFieldVal)
	{
	char *selectedFacetValues=cartUsualString(cart, "cdwSelectedFieldValues", "");
	struct facetField *selList = deLinearizeFacetValString(selectedFacetValues);
	selectedListFacetValUpdate(&selList, selFieldName, selFieldVal, selOp);
	char *newSelectedFacetValues = linearizeFacetVals(selList);
	cartSetString(cart, "cdwSelectedFieldValues", newSelectedFacetValues);
	cartRemove(cart, "browseFiles_facet_op");
	cartRemove(cart, "browseFiles_facet_fieldName");
	cartRemove(cart, "browseFiles_facet_fieldVal");
	}
    }

/* Put up the big faceted search table */
webFilteredSqlTable(cart, conn, 
    "cell_count,organ,cell_type", trackName, "", 
    returnUrl, trackName, 32, 
    emptyHash, NULL, 
    FALSE, NULL, 100, 10, emptyHash, "organ,cell_class,stage,cell_type",
    NULL);

/* Clean up and go home. */
printf("</form>\n");
hashFree(&emptyHash);
sqlDisconnect(&conn);
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
/* Set some major global variable and attach us to current genome and DB. */
cart = theCart;
getDbAndGenome(cart, &database, &genome, oldVars);
initGenbankTableNames(database);

/* Set udcTimeout from cart */
int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

/* Wrap http/html text around main routine */
htmStart(stdout, "hgFacetedBars");
doBody();
htmEnd(stdout);
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
