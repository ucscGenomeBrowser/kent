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
#include "hgConfig.h"
#include "facetedTable.h"
#include "facetedBar.h"
#include "htmlColor.h"


struct wrapContext
/* Context used by various wrappers. */
    {
    int colorIx;    /* Index of color in row */
    int valIx;	    /* Where value lives */
    double maxVal;/* Maximum of any total value */
    };

void wrapVal(struct fieldedTable *table, struct fieldedRow *row,
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper draws a SVG bar*/
{
struct wrapContext *wc = context;
char *color = "#000000";    // Black is default color if no color column
unsigned rgb;
int colorIx = wc->colorIx;
if (colorIx >= 0)
    {
    color = row->row[colorIx];
    /* try r,g,b */
    if (index(color, ','))
        {
        unsigned char r, g, b;
        parseColor(color, &r, &g, &b);
        htmlColorFromRGB(&rgb, r, g, b);
        color = htmlColorToCode(rgb);
        }
    }
double x = sqlDouble(val);
int width = 500, height = 13;  // These are units of ~2010 pixels or something
double barWidth = (double)width*x/wc->maxVal;
printf("<svg width=\"%g\" height=\"%d\">", barWidth, height);
printf("<rect width=\"%g\" height=\"%d\" style=\"fill:%s\">", 
    barWidth, height, color);
printf("</svg>");
printf(" %6.2f", x);
}

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
struct hash *wrapperHash = hashNew(0);

/* Load up table from tsv file */
char *requiredStatsFields[] = {"count",};
struct fieldedTable *statsTable = fieldedTableFromTabFile(statsFile, statsFile, 
    requiredStatsFields, ArraySize(requiredStatsFields));
struct fieldedTable *table = addChartVals(statsTable, chart);

/* Update facet selections from users input if any and get selected part of table */
struct facetedTable *facTab = facetedTableFromTable(table, trackName, facets);
facTab->mergeFacetsOk = trackDbSettingOn(tdb, "barChartMerge");
boolean trackDbSettingOn(struct trackDb *tdb, char *name);
/* Return true if a tdb setting is "on" "true" or "enabled". */

facetedTableUpdateOnClick(facTab, cart);
struct facetField **selectedFf = NULL;
struct fieldedTable *selected = facetedTableSelect(facTab, cart, &selectedFf);

/* Write html to make white background */
hInsideStyleToWhite();

/* Add a few extra status bits  and a way back home before facets and table display */
printf("&nbsp;&nbsp;<B>Bars selected:</B> %d of %d", selected->rowCount, table->rowCount);
printf("&nbsp;&nbsp;&nbsp;&nbsp;<b><a href=%s>Return to Genome Browser</href></a></b><p>\n", 
                     hgTracksPathAndSettings());

/* Set up url that has enough context to get back to us.  */
struct dyString *returnUrl = dyStringNew(0);
dyStringPrintf(returnUrl, "../cgi-bin/hgc?g=%s", trackName);
if (item != NULL)
    dyStringPrintf(returnUrl, "&i=%s", item);
dyStringPrintf(returnUrl, "&%s", cartSidUrlString(cart));

/* Working within a form we save context.  It'd be nice to work outside of form someday. */
printf("<form action=\"../cgi-bin/hgc\" name=\"facetForm\" ");
printf("method=\"GET\">\n");
printf("<div style=\"background-color:white\">");
cartSaveSession(cart);
cgiContinueHiddenVar("g");
cgiContinueHiddenVar("i");

char *valFieldName = "val";

boolean singleCell = !trackDbSettingOff(tdb, "singleCellColumnNames");

if (singleCell)
    {
    if (sameString(selected->fields[1], "count"))
	selected->fields[1] = cloneString("cell count");
    int i;
    for (i = selected->fieldCount -1; i >= 0; --i)
	{
	if (sameString(selected->fields[i], "val"))
	    {
	    selected->fields[i] = cloneString("read count");
	    break;
	    }
	}
    valFieldName = "read count";
    }

/* Set up context for functions that wrap output fields */
struct wrapContext context = {
                              .valIx = fieldedTableFindFieldIx(selected, valFieldName),
			      .colorIx = fieldedTableFindFieldIx(selected, "color"), 
			     };
context.maxVal = fieldedTableMaxInCol(selected, context.valIx);

/* Add wrapper function(s) */
hashAdd(wrapperHash, valFieldName, wrapVal);

/* Pick which fields to display.  We'll take the first field whatever it is
 * named, and also count, and the "val" field we added and wrapped. */
char displayList[256];
if (singleCell)
    safef(displayList, sizeof(displayList), "%s,cell count,read count", selected->fields[0]);
else
    safef(displayList, sizeof(displayList), "%s,count,val", selected->fields[0]);

facetedTableWriteHtml(facTab, cart, selected, selectedFf, displayList,
    returnUrl->string, 45, wrapperHash, &context, 7);

/* Clean up and go home. */
printf("</div></form>\n");
facetedTableFree(&facTab);
sqlDisconnect(&conn);
}

