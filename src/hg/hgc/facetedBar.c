#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "web.h"
#include "hvGfx.h"
#include "trashDir.h"
#include "hCommon.h"
#include "hui.h"
#include "asParse.h"
#include "hgc.h"
#include "trackHub.h"
#include "hgColors.h"
#include "fieldedTable.h"
#include "tablesTables.h"
#include "facetField.h"

#include "barChartBed.h"
#include "barChartData.h"
#include "barChartSample.h"
#include "barChartUi.h"
#include "hgConfig.h"
#include "facetedBar.h"


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
printf("<style>");
printf("body.cgi {\n");
printf("   background: #F0F0F0;\n");
printf("}\n");
printf("table.hgInside {\n");
printf("   background: #FFFFFF;\n");
printf("}\n");
printf("</style>");
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
    int colorIx;    /* Index of color in row */
    int valIx;	    /* Where value lives */
    double maxVal;/* Maximum of any total value */
    };

void wrapVal(struct fieldedTable *table, struct fieldedRow *row,
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper draws a SVG bar*/
{
struct wrapContext *wc = context;
char *color = "#000000";
int colorIx = wc->colorIx;
if (colorIx >= 0)
    color = row->row[colorIx];
double x = sqlDouble(val);
int width = 500, height = 13;  // These are units of ~2010 pixels or something
double barWidth = (double)width*x/wc->maxVal;
printf("<svg width=\"%g\" height=\"%d\">", barWidth, height);
printf("<rect width=\"%g\" height=\"%d\" style=\"fill:%s\">", 
    barWidth, height, color);
printf("</svg>");
printf(" %6.2f", x);
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

struct fieldedTable *addChartVals(struct fieldedTable *table, struct barChartBed *chart)
/* All a new column with chart values */
{
if (chart->expCount != table->rowCount)
    errAbort("Mismatch in field counts between %s (%d) and barChartBed (%d)",
	table->name, table->rowCount, chart->expCount);
int oldFieldCount = table->fieldCount;
int newFieldCount = oldFieldCount+1;
char *newRow[newFieldCount];
int i;
for (i=0; i<oldFieldCount; ++i)
    newRow[i] = table->fields[i];
newRow[oldFieldCount] = "val";
struct fieldedTable *newTable = fieldedTableNew(table->name, newRow, newFieldCount);
struct fieldedRow *fr;
int rowIx = 0;
for (fr = table->rowList; fr != NULL; fr = fr->next, ++rowIx)
    {
    for (i=0; i<oldFieldCount; ++i)
	newRow[i] = fr->row[i];
    char buf[16];
    safef(buf, sizeof(buf), "%g", chart->expScores[rowIx]);
    newRow[oldFieldCount] = lmCloneString(newTable->lm, buf);
    fieldedTableAdd(newTable, newRow, newFieldCount, fr->id);
    }
return newTable;
}

void facetedBarChart(char *item, struct barChartBed *chart, struct trackDb *tdb, 
    double maxVal, char *statsFile, char *facets, char *metric)
{
char *trackName = tdb->track;
struct sqlConnection *conn = sqlConnect(database);
struct hash *emptyHash = hashNew(0);
struct hash *wrapperHash = hashNew(0);
struct facetedTable *ft = facetedTableNew(tdb->shortLabel, trackName);

/* Write out html to pull in the other files we use. */
facetedTableWebInit();

/* Set up url that has enough context to get back to us.  */
struct dyString *returnUrl = dyStringNew(0);
dyStringPrintf(returnUrl, "../cgi-bin/hgc?g=%s", trackName);
if (item != NULL)
    dyStringPrintf(returnUrl, "&i=%s", item);
dyStringPrintf(returnUrl, "&%s", cartSidUrlString(cart));

/* Working within a form we save context */
printf("<form action=\"../cgi-bin/hgc\" name=\"facetForm\" ");
printf("method=\"GET\">\n");
printf("<div style=\"background-color:white\">");
cartSaveSession(cart);
cgiContinueHiddenVar("g");
cgiContinueHiddenVar("i");

/* Put up the big faceted search table in a new div */

/* Load up table from tsv file */
char *requiredStatsFields[] = {"count",};
struct fieldedTable *statsTable = fieldedTableFromTabFile(statsFile, statsFile, 
    requiredStatsFields, ArraySize(requiredStatsFields));
struct fieldedTable *table = addChartVals(statsTable, chart);

/* Update facet selections from users input if any */
facetedTableUpdateOnFacetClick(ft, cart);

/* Do facet selection calculations */
char *selList = facetedTableSelList(ft, cart);
struct facetField *ffArray[table->fieldCount];
struct fieldedTable *selected = facetFieldsFromFieldedTable(table, selList, ffArray);



/* Set up wrappers for some of output fields */
struct wrapContext context = {.nameIx = 0,
		              .countIx = fieldedTableFindFieldIx(table, "count"),
		              .valIx = fieldedTableFindFieldIx(table, "val"),
			      .colorIx = fieldedTableFindFieldIx(table, "color"), 
			     };
context.maxVal = fieldedTableMaxInCol(table, context.valIx);

/* Put up faceted search and table of visible fields. */
char displayList[256];
safef(displayList, sizeof(displayList), "%s,count,val", table->fields[0]);
hashAdd(wrapperHash, "val", wrapVal);
webFilteredFieldedTable(cart, selected, 
    displayList, returnUrl->string, trackName, 
    32, wrapperHash, &context,
    FALSE, NULL,
    selected->rowCount, 7,
    NULL, emptyHash,
    ffArray, facets,
    NULL);

/* Clean up and go home. */
printf("</div></form>\n");
hashFree(&emptyHash);
sqlDisconnect(&conn);
}

