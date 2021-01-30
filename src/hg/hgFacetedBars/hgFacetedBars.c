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
#include "sqlNum.h"

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

struct wrapContext
/* Context used by various wrappers. */
    {
    int nameIx;	    /* First col by convention is name */
    int countIx;    /* Index of count in row */
    int meanIx;    /* The field that has total read count */
    int colorIx;    /* Index of color in row */
    double maxMean;/* Maximum of any total value */
    };

void wrapMean(struct fieldedTable *table, struct fieldedRow *row,
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper draws a SVG bar*/
{
struct wrapContext *wc = context;
char *color = "#000000";
int colorIx = wc->colorIx;
if (colorIx >= 0)
    color = row->row[colorIx];
double mean = sqlDouble(val);
int width = 500, height = 13;  // These are units of ~2010 pixels or something
double barWidth = (double)width*mean/wc->maxMean;
printf("<svg width=\"%g\" height=\"%d\">", barWidth, height);
printf("<rect width=\"%g\" height=\"%d\" style=\"fill:%s\">", 
    barWidth, height, color);
printf("</svg>");
printf(" %6.2f", mean);
}

double fieldedTableMaxInCol(struct fieldedTable *table, int colIx)
/* Figure out total and count columns from context and use them to figure
 * out maximum mean value */
{
boolean firstTime = TRUE;
double max = 0.0;
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    double val = sqlDouble(fr->row[colIx]);
    if (firstTime)
	{
        max = val;
	firstTime = FALSE;
	}
    else if (max < val)
        max = val;
    }
return max;
}

#ifdef OLD
double calcMaxMean(struct fieldedTable *table, struct wrapContext *context)
/* Go through text table figureing out mean for each row based on 
 * context fields.  Return max of the means */
{
boolean firstTime = TRUE;
double max = 0.0;
struct fieldedRow *fr;
int countIx = context->countIx;
int totalIx = context->totalIx;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    double val = sqlDouble(fr->row[totalIx])/sqlDouble(fr->row[countIx]);
    if (firstTime)
	{
        max = val;
	firstTime = FALSE;
	}
    else if (max < val)
        max = val;
    }
return max;
}
#endif /* OLD */

void doBody()
{
/* Fake up a 'track' for development */
char *trackName = "cellFacetsJk1";

struct sqlConnection *conn = sqlConnect(database);
struct hash *emptyHash = hashNew(0);
struct hash *wrapperHash = hashNew(0);
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

/* Put up the big faceted search table in a new div */

/* Load up table from tsv file */
char *statsFileName = "/gbdb/hg38/bbi/singleCellMerged/barChart.stats";
char *requiredStatsFields[] = {"cluster","count","organ","cell_class","stage","cell_type"};
struct fieldedTable *table = fieldedTableFromTabFile(statsFileName, statsFileName, 
    requiredStatsFields, ArraySize(requiredStatsFields));

/* Update facet selections from users input if any */
facetedTableUpdateOnFacetClick(ft, cart);

/* Do facet selection calculations */
char *selList = facetedTableSelList(ft, cart);
struct facetField *ffArray[table->fieldCount];
struct fieldedTable *selected = facetFieldsFromFieldedTable(table, selList, ffArray);


/* Set up wrappers for some of output fields */
struct wrapContext context = {.nameIx = 0,
		              .countIx = fieldedTableFindFieldIx(table, "count"),
		              .meanIx = fieldedTableFindFieldIx(table, "mean_total"),
			      .colorIx = fieldedTableFindFieldIx(table, "color"), 
			     };
context.maxMean = fieldedTableMaxInCol(table, context.meanIx);

/* Put up faceted search and table of visible fields. */
hashAdd(wrapperHash, "mean_total", wrapMean);
webFilteredFieldedTable(cart, selected, 
    "count,cluster,mean_total", returnUrl, trackName, 
    32, wrapperHash, &context,
    FALSE, NULL,
    selected->rowCount, 7,
    NULL, emptyHash,
    ffArray, "organ,cell_class,stage,cell_type",
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
