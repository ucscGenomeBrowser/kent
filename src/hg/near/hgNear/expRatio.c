/* Handle expression ratio type microarray data. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "hgNear.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: expRatio.c,v 1.7 2003/06/24 20:16:32 kent Exp $";


static char *expRatioCellVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Get comma separated list of values. */
{
return cloneString("coming soon");
}

static boolean expRatioExists(struct column *col, struct sqlConnection *conn)
/* This returns true if relevant tables exist. */
{
boolean tableOk = sqlTableExists(conn, col->table);
boolean posTableOk = sqlTableExists(conn, col->posTable);
boolean expTableOk = sqlTableExists(conn, col->expTable);
return tableOk && posTableOk && expTableOk;
}

static void hexOne(double val)
/* Convert val 0.0-1.0 to hex 00 to FF */
{
int hex = val * 0xFF;
if (hex > 0xFF) hex = 0xFF;
hPrintf("%02X", hex);
}

static char *colSchemeVarName(struct column *col)
/* Return variable name for use-blue. */
{
static char buf[128];
safef(buf, sizeof(buf), "near.col.%s.color", col->name);
return buf;
}

static char *scaleVarName(struct column *col)
/* Return variable name for use-blue. */
{
static char buf[128];
safef(buf, sizeof(buf), "near.col.%s.scale", col->name);
return buf;
}


static char *colorSchemeVals[] = {
/* Menu option for color scheme. */
   "red high/green low",
   "blue high/green low",
};

static void colorVal(double val, boolean useBlue)
/* Val is -1.0 to 1.0.  Print color in form #FF0000, normally
 * using green for minus values, red for plus values, but
 * optionally using blue for plus values. */
{
if (useBlue)
    {
    if (val < 0)
	{
	hPrintf("00");	    /* Red */
        hexOne(-val*0.7);    /* Green - much brighter than blue*/
	hPrintf("00");      /* Blue */
	}
    else
	{
	hPrintf("00");
        hPrintf("00");
        hexOne(val);
	}
    }
else 
    {
    if (val < 0)
	{
	hPrintf("00");	    /* Red */
        hexOne(-val*0.8);    /* Green - brighter than red*/
	hPrintf("00");      /* Blue */
	}
    else
	{
        hexOne(val);
	hPrintf("00");
        hPrintf("00");
	}
    }
}

static int expSubcellWidth = 16;

static void startExpCell()
/* Print out start of expression cell, which contains a table. */
{
hPrintf("<TD><TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR>");
}

static void endExpCell()
/* Print out end of expression cell, closing up internal table. */
{
hPrintf("</TR></TABLE></TD>");
}

static void restartExpCell()
/* End expression cell and begin a new one. */
{
endExpCell();
startExpCell();
}

static void printRatioShades(struct column *col, int repCount, 
	int *reps, int valCount, float *vals)
/* Print out representatives in shades of color in table background. */
{
int i;
float scale = col->expScale;
float val;
startExpCell();
for (i=0; i<repCount; ++i)
    {
    int ix = reps[i];
    if (ix > valCount)
        errAbort("Representative larger than biggest experiment in %s", col->name);
    if (ix == -1)
        {
	restartExpCell();
	}
    else
	{
	val = vals[ix];
	if (val <= -9999)
	    hPrintf("<TD WIDTH=%d>&nbsp;</TD>", expSubcellWidth);
	else
	    {
	    hPrintf("<TD WIDTH=%d BGCOLOR=\"#", expSubcellWidth);
	    colorVal(val*scale, col->expRatioUseBlue);
	    hPrintf("\">&nbsp;</TD>");
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
hPrintf("%s", s);
for (i=0; i<repCount; ++i)
    {
    int ix = reps[i];
    if (ix == -1)
        {
	restartExpCell();
	hPrintf("%s", s);
	}
    }
endExpCell();
}

void expRatioCellPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Print out html for expRatio cell. */
{
int i, numExpts = col->representativeCount;
struct bed *bed;
struct sqlResult *sr;
char **row, query[256];
char expName[64];

safef(query, sizeof(query), "select value from %s where name = '%s'", 
	col->table, gp->name);
if (sqlQuickQuery(conn, query, expName, sizeof(expName)) == NULL)
    {
    replicate("n/a", col->representativeCount, col->representatives);
    return;
    }
safef(query, sizeof(query), "select expScores from %s where name = '%s'",
	col->posTable, expName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    replicate("n/a???", col->representativeCount, col->representatives);
else 
    {
    int idCount, valCount;
    float *vals;
    sqlFloatDynamicArray(row[0], &vals, &valCount);
    printRatioShades(col, col->representativeCount, col->representatives, 
    	valCount, vals);
    }
sqlFreeResult(&sr);
}

static char **getExperimentNames(char *database, char *table, int expCount, int *expIds)
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

void expRatioLabelPrint(struct column *col)
/* Print out labels of various experiments. */
{
int i, numExpts = col->representativeCount;
boolean doGreen = FALSE;
char **experiments = getExperimentNames("hgFixed", col->expTable, numExpts,
	col->representatives);
startExpCell();
for (i=0; i<numExpts; ++i)
    {
    char *label = experiments[i], c;
    if (label == NULL)
        {
	restartExpCell();
	}
    else
	{
	hPrintf("<TD VALIGN=BOTTOM ALIGN=MIDDLE");
	if (doGreen)
	    hPrintf(" BGCOLOR=\"#FFC8C8\"");
	hPrintf(">");
	hPrintf("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>\n");
	while ((c = *label++) != 0)
	    {
	    char cString[2], *s;
	    if (c == ' ')
		{
		s = "&nbsp;";
		}
	    else
		{
		s = cString;
		s[0] = c;
		s[1] = 0;
		}
	    hPrintf(" <TR ><TD ALIGN=MIDDLE WIDTH=%d", expSubcellWidth);
	    hPrintf(">%s</TD></TR>\n", s);
	    }
	hPrintf("</TABLE></TD>\n");
	doGreen = 1 - doGreen;	/* Toggle value */
	}
    }
endExpCell();

/* CLean up */
for (i=0; i<numExpts; ++i)
   freeMem(experiments[i]);
freeMem(experiments);
}

static void expRatioConfigControls(struct column *col)
/* Print out configuration column */
{
hPrintf("<TD>");

/* Make color drop-down. */
    {
    char *varName = colSchemeVarName(col);
    char *checked = cartUsualString(cart, varName, colorSchemeVals[0]);
    hPrintf("colors: ");
    cgiMakeDropList(varName, colorSchemeVals, ArraySize(colorSchemeVals), checked);
    }

/* Make scale drop-down. */
    {
    char *varName = scaleVarName(col);
    char *val = cartUsualString(cart, varName, "1.0");
    hPrintf("scale: ");
    cgiMakeTextVar(varName, val, 3);
    }
hPrintf("</TD>");
}


void setupColumnExpRatio(struct column *col, char *parameters)
/* Set up expression ration type column. */
{
/* Hash up name value pairs and extract some we care about. */
char *repList = hashFindVal(col->settings, "representatives");
char *expMax = hashFindVal(col->settings, "max");

col->table = cloneString(nextWord(&parameters));
col->posTable = cloneString(nextWord(&parameters));
col->expTable = cloneString(nextWord(&parameters));
if (col->posTable == NULL)
    errAbort("missing parameters from type line of %s", col->name);    


/* Convert list of ascii number in repList to an array of binary
 * numbers in col->representatives. */
if (repList != NULL)
    {
    char *repStrings[256];
    char *dupe = cloneString(repList);
    int repCount = chopString(dupe, ",", repStrings, ArraySize(repStrings));
    int *reps, i;
    AllocArray(reps, repCount);
    for (i=0; i<repCount; ++i)
        reps[i] = sqlSigned(repStrings[i]);
    col->representativeCount = repCount;
    col->representatives = reps;
    freez(&dupe);
    }
else
    {
    errAbort("Missing required representatives list in %s", col->name);
    }

if (expMax != NULL)
    col->expScale = 1.0/atof(expMax);
else
    col->expScale = 1.0/3.0;

/* Figure out color scheme. */
    {
    char *varName = colSchemeVarName(col);
    char *val = cartUsualString(cart, varName, colorSchemeVals[0]);
    col->expRatioUseBlue = !sameString(val, colorSchemeVals[0]);
    }

/* Figure out scale. */
    {
    char *varName = scaleVarName(col);
    char *val = cartUsualString(cart, varName, "1 x");
    double fVal = atof(val);
    if (fVal == 0) fVal = 1.0;
    col->expScale *= fVal;
    }

col->exists = expRatioExists;
col->cellVal = expRatioCellVal;
col->cellPrint = expRatioCellPrint;
col->labelPrint = expRatioLabelPrint;
col->configControls = expRatioConfigControls;
}
