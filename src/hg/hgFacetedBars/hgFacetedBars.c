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
#include "facetedTable.h"
#include "sqlNum.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
char *database = NULL;
char *genome = NULL;

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

/* Load up table from tsv file */
char *statsFileName = "/gbdb/hg38/bbi/singleCellMerged/barChart.stats";
char *requiredStatsFields[] = {"count","organ","cell_class","stage","cell_type"};
struct fieldedTable *table = fieldedTableFromTabFile(statsFileName, statsFileName, 
    requiredStatsFields, ArraySize(requiredStatsFields));
char *keyField = table->fields[0];

struct sqlConnection *conn = sqlConnect(database);
struct hash *emptyHash = hashNew(0);
struct hash *wrapperHash = hashNew(0);
struct facetedTable *ft = facetedTableFromTable(table, 
				trackName, "organ,cell_class,stage,cell_type");

/* Working within a form we save context */
printf("<form action=\"../cgi-bin/hgFacetedBars\" name=\"facetForm\" method=\"GET\">\n");
cartSaveSession(cart);

/* Process user input and get selected subtable */
facetedTableUpdateOnClick(ft, cart);
struct fieldedTable *selected = facetedTableSelect(ft, cart);

/* Set up url that has enough context to get back to us.  This is very much a work in 
 * progress. */
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/hgFacetedBars?%s",
    cartSidUrlString(cart) );

char viewFields[256];
safef(viewFields, sizeof(viewFields), "count,%s,mean_total", keyField);


/* Set up wrappers for some of output fields */
struct wrapContext context = {.nameIx = 0,
		              .countIx = fieldedTableFindFieldIx(table, "count"),
		              .meanIx = fieldedTableFindFieldIx(table, "mean_total"),
			      .colorIx = fieldedTableFindFieldIx(table, "color"), 
			     };
context.maxMean = fieldedTableMaxInCol(table, context.meanIx);

/* Put up faceted search and table of visible fields. */
hashAdd(wrapperHash, "mean_total", wrapMean);
 
facetedTableWriteHtml(ft, cart, selected, viewFields, returnUrl, 32, wrapperHash, &context, 10);

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
