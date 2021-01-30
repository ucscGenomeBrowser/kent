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
#include "facetField.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
char *database = NULL;
char *genome = NULL;

struct facetedTable
/* Help manage a faceted table */
    {
    struct facetedTable *next;
    char *name;		/* Name of file or database table */
    char *varPrefix;	/* Prefix used on variables */
    };

struct facetedTable *facetedTableNew(char *name, char *varPrefix)
{
struct facetedTable *ft;
AllocVar(ft);
ft->name = cloneString(name);
ft->varPrefix = cloneString(varPrefix);
return ft;
}

void facetedTableFree(struct facetedTable **pFt)
/* Free up resources associated with faceted table */
{
struct facetedTable *ft = *pFt;
if (ft != NULL)
    {
    freeMem(ft->name);
    freeMem(ft->varPrefix);
    freez(pFt);
    }
}


void facetedTableWebInit()
/* Print out scripts and css that we need.  We should be in a page body or title. */
{
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
}

char *facetedTableSelOp(struct facetedTable *ft, struct cart *cart)
/* Look up selOp in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_op", ft->varPrefix);
return cartOptionalString(cart, var);
}

char *facetedTableSelField(struct facetedTable *ft, struct cart *cart)
/* Look up sel field in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_fieldName", ft->varPrefix);
return cartOptionalString(cart, var);
}

char *facetedTableSelVal(struct facetedTable *ft, struct cart *cart)
/* Look up sel val in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_fieldVal", ft->varPrefix);
return cartOptionalString(cart, var);
}

char *facetedTableSelList(struct facetedTable *ft, struct cart *cart)
/* Look up sel val in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_selList", ft->varPrefix);
return cartOptionalString(cart, var);
}

void facetedTableRemoveOpVars(struct facetedTable *ft, struct cart *cart)
/* Remove sel op/field/name vars from cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_op", ft->varPrefix);
cartRemove(cart, var);
safef(var, sizeof(var), "%s_facet_fieldVal", ft->varPrefix);
cartRemove(cart, var);
safef(var, sizeof(var), "%s_facet_fieldName", ft->varPrefix);
cartRemove(cart, var);
}

boolean facetedTableUpdateOnFacetClick(struct facetedTable *ft, struct cart *cart)
/* If we got called by a click on a facet deal with that and return TRUE, else do
 * nothing and return false */
{
char *selOp = facetedTableSelOp(ft, cart);
if (selOp)
    {
    char *selFieldName = facetedTableSelField(ft, cart);
    char *selFieldVal = facetedTableSelVal(ft, cart);
    if (selFieldName && selFieldVal)
	{
	char selListVar[256];
	safef(selListVar, sizeof(selListVar), "%s_facet_selList", ft->varPrefix);
	char *selectedFacetValues=cartUsualString(cart, selListVar, "");
	struct facetField *selList = deLinearizeFacetValString(selectedFacetValues);
	selectedListFacetValUpdate(&selList, selFieldName, selFieldVal, selOp);
	char *newSelectedFacetValues = linearizeFacetVals(selList);
	cartSetString(cart, selListVar, newSelectedFacetValues);
	facetedTableRemoveOpVars(ft, cart);
	}
    return TRUE;
    }
else
    return FALSE;
}


void doBody()
{
/* Fake up a 'track' for development */
char *trackName = "cellFacetsJk1";

struct sqlConnection *conn = sqlConnect(database);
struct hash *emptyHash = hashNew(0);
struct facetedTable *ft = facetedTableNew("the original", trackName);

/* Write out html to pull in the other files we use. */
facetedTableWebInit();

/* Working within a form we save context */
printf("<form action=\"../cgi-bin/hgFacetedBars\" name=\"facetForm\" method=\"GET\">\n");
cartSaveSession(cart);

/* Set up url that has enough context to get back to us.  This is very much a work in 
 * progress. */
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/hgFacetedBars?%s",
    cartSidUrlString(cart) );

/* Put up the big faceted search table */
printf("<div>\n");
char *statsFileName = "/gbdb/hg38/bbi/singleCellMerged/barChart.stats";
char *requiredStatsFields[] = {"cluster","count","mean","organ","cell_class","stage","cell_type"};
struct fieldedTable *table = fieldedTableFromTabFile(statsFileName, statsFileName, 
    requiredStatsFields, ArraySize(requiredStatsFields));
facetedTableUpdateOnFacetClick(ft, cart);

/* Look up sel val in cart */
char *selList = facetedTableSelList(ft, cart);
struct facetField *ffArray[table->fieldCount];
int selCount = 0;
facetFieldsFromFieldedTable(table, selList, ffArray, &selCount);
// uglyf("%d of %d selected in facets", selCount, table->rowCount);
webFilteredFieldedTable(cart, table, 
    "count,cluster,mean", returnUrl, trackName, 
    32, emptyHash, NULL,
    FALSE, NULL,
    table->rowCount, 7,
    NULL, emptyHash,
    ffArray, "organ,cell_class,stage,cell_type",
    NULL);

#ifdef OLD
webFilteredSqlTable(cart, conn, 
    "organ,cell_class,cell_type", trackName, "", 
    returnUrl, trackName, 32, 
    emptyHash, NULL, 
    FALSE, NULL, 50, 7, emptyHash, "organ,cell_class,stage,cell_type",
    NULL);
#endif /* OLD */
printf("</div>\n");


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
