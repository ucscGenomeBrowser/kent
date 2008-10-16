/* hgExp - help browse expression data. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "gifLabel.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hgExp.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgExp.c,v 1.15 2008/10/16 18:08:30 hiram Exp $";

static int expSubcellWidth = 21;

static char *colorSchemeVals[] = {
/* Menu option for color scheme. */
   "red high/green low",
   "yellow high/blue low",
};

void hgExpColorDropDown(struct cart *cart, char *varName)
/* Make color drop-down. */
{
char *checked = cartUsualString(cart, varName, colorSchemeVals[0]);
cgiMakeDropList(varName, 
	colorSchemeVals, ArraySize(colorSchemeVals), checked);
}

boolean hgExpRatioUseBlue(struct cart *cart, char *varName)
/* Return TRUE if should use blue instead of red
 * in the expression ratios. */
{
char *val = cartUsualString(cart, varName, colorSchemeVals[0]);
return !sameString(val, colorSchemeVals[0]);
}

char **hgExpGetNames(char *database, char *table, 
	int expCount, int *expIds, int skipSize)
/* Create array filled with experiment names. */
{
char **names, *name;
int i;
struct sqlConnection *conn = sqlConnect(database);
char query[256], nameBuf[128];
int maxLen = 0, len;

/* Read into array and figure out longest name. */
AllocArray(names, expCount);
for (i=0; i<expCount; ++i)
    {
    int ix = expIds[i];
    if (ix == -1)
        names[i] = NULL;
    else
	{
	safef(query, sizeof(query), "select name from %s where id = %d", 
	    table, expIds[i]);
	if ((name = sqlQuickQuery(conn, query, nameBuf, sizeof(nameBuf))) == NULL)
	    name = "unknown";
	else
	    name += skipSize;
	names[i] = cloneString(name);
	len = strlen(name);
	if (len > maxLen) maxLen = len;
	}
    }
sqlDisconnect(&conn);

/* Right justify names. */
for (i=0; i<expCount; ++i)
    {
    char *name = names[i];
    if (name != NULL)
        {
	safef(nameBuf, sizeof(nameBuf), "%*s", maxLen, name);
	freeMem(name);
	names[i] = cloneString(nameBuf);
	}
    }
return names;
}

static int countNonNull(char **row, int maxCount)
/* Count number of non-null rows. */
{
int i;
for (i=0; i<maxCount; ++i)
    {
    if (row[i] == NULL)
        break;
    }
return i;
}

void hgExpLabelPrint(char *colName, char *subName, int skipName,
	char *url, int representativeCount, int *representatives,
	char *expTable, int gifStart)
/* Print out labels of various experiments. */
{
int i;
int groupSize;
char gifName[128];
char **experiments = hgExpGetNames("hgFixed", 
	expTable, representativeCount, representatives, skipName);
int height = gifLabelMaxWidth(experiments, representativeCount);

for (i=0; i<representativeCount; i += groupSize+1)
    {
    printf("<TD VALIGN='BOTTOM' WIDTH=%d>", expSubcellWidth);
    groupSize = countNonNull(experiments+i, representativeCount-i);
    safef(gifName, sizeof(gifName), "../trash/nea_%s_%s%d.gif", 
    	colName, subName, ++gifStart);
    gifLabelVerticalText(gifName, experiments+i, groupSize, height);
    if (url != NULL)
       printf("<A HREF=\"%s\">", url); 
    printf("<IMG BORDER=0 SRC=\"%s\">", gifName);
    if (url != NULL)
	printf("</A>");
    printf("</TD>");
    }

/* Clean up */
for (i=0; i<representativeCount; ++i)
   freeMem(experiments[i]);
freeMem(experiments);
}


boolean hgExpLoadVals(struct sqlConnection *lookupConn,
	struct sqlConnection *dataConn,
	char *lookupTable, char *name, char *dataTable,
	int *retValCount, float **retVals)
/* Load up and return expression bed record.  Return NULL
 * if none of given name exist. */
{
char query[256];
char expName[64];
struct sqlResult *sr;
char **row;
boolean ok = FALSE;
safef(query, sizeof(query), "select value from %s where name = '%s'", 
	lookupTable, name);
if (sqlQuickQuery(lookupConn, query, expName, sizeof(expName)) == NULL)
    return FALSE;
safef(query, sizeof(query), "select expScores from %s where name = '%s'",
	dataTable, expName);
sr = sqlGetResult(dataConn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    sqlFloatDynamicArray(row[0], retVals, retValCount);
    ok = TRUE;
    }
sqlFreeResult(&sr);
return ok;
}


static void hexOne(double val)
/* Convert val 0.0-1.0 to hex 00 to FF */
{
int hex = val * 0xFF;
if (hex > 0xFF) hex = 0xFF;
printf("%02X", hex);
}

static void colorVal(double val, double scale, boolean useBlue, boolean useGrays, boolean logGrays)
/* Val is -1.0 to 1.0.  Print color in form #FF0000, normally
 * using green for minus values, red for plus values, but
 * optionally using blue for minus values and yellow for plus values. */
{
if (useGrays)
    {
    if (val < 1)
        printf("000000");
    else
	{
	val = (logGrays) ? log(val) * scale : val * scale;
	hexOne(val);
	hexOne(val);
	hexOne(val);
	}
    }
else 
    {
    val *= scale;
    if (useBlue)
	{
	if (val < 0)
	    {
	    val = -val;
	    printf("00");
	    printf("00");
	    hexOne(val);
	    }
	else
	    {
	    val *= 0.7;
	    hexOne(val);    /* Red */
	    hexOne(val);     /* Green */
	    printf("00");   /* Blue */
	    }
	}
    else 
	{
	if (val < 0)
	    {
	    printf("00");	    /* Red */
	    hexOne(-val*0.8);    /* Green - brighter than red*/
	    printf("00");      /* Blue */
	    }
	else
	    {
	    hexOne(val);
	    printf("00");
	    printf("00");
	    }
	}
    }
}

static void startExpCell()
/* Print out start of expression cell, which contains a table. */
{
printf("<TD><TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR>");
}

static void endExpCell()
/* Print out end of expression cell, closing up internal table. */
{
printf("</TR></TABLE></TD>");
}

static void restartExpCell()
/* End expression cell and begin a new one. */
{
endExpCell();
startExpCell();
}

static void printRatioShades(char *colName, int repCount, 
	int *reps, int valCount, float *vals, 
	boolean colorBlindColors,
	boolean useGrays, boolean logGrays, float scale)
/* Print out representatives in shades of color in table background. */
{
int i;
float val;
startExpCell();
for (i=0; i<repCount; ++i)
    {
    int ix = reps[i];
    if (ix > valCount)
        errAbort("Representative larger than biggest experiment in %s", colName);
    if (ix == -1)
        {
	restartExpCell();
	}
    else
	{
	val = vals[ix];
	if (val <= -9999)
	    printf("<TD WIDTH=%d>&nbsp;</TD>", expSubcellWidth);
	else
	    {
	    printf("<TD WIDTH=%d BGCOLOR='#", expSubcellWidth);
	    colorVal(val, scale, colorBlindColors, useGrays, logGrays);
	    printf("'>&nbsp;</TD>");
	    }
	}
    }
endExpCell();
}

static void replicate(char *s, int repCount, int *reps)
/* Replicate s in cells of table */
{
int i;
startExpCell();
printf("%s", s);
for (i=0; i<repCount; ++i)
    {
    int ix = reps[i];
    if (ix == -1)
        {
	restartExpCell();
	printf("%s", s);
	}
    }
endExpCell();
}

void hgExpCellPrint(char *colName, char *geneId, 
	struct sqlConnection *lookupConn, char *lookupTable,
	struct sqlConnection *dataConn, char *dataTable,
	int representativeCount, int *representatives,
	boolean useBlue, boolean useGrays, boolean logGrays, float scale)
/* Print out html for expression cell in table. */
{
int valCount;
float *vals = NULL;

if (hgExpLoadVals(lookupConn, dataConn, lookupTable, geneId, dataTable, 
	&valCount, &vals))
    {
    printRatioShades(colName, representativeCount, representatives, 
    	valCount, vals, useBlue, useGrays, logGrays, scale);
    freez(&vals);
    }
else
    {
    replicate("<TD>n/a</TD>", representativeCount, representatives);
    }
}

