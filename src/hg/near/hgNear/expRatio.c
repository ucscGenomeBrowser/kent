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

static char const rcsid[] = "$Id: expRatio.c,v 1.4 2003/06/22 07:46:36 kent Exp $";


static char *expRatioCellVal(struct column *col, char *geneId, 
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

void colorVal(double val, boolean useBlue)
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

static boolean expRatioUseBlue = FALSE;
static int expSubcellWidth = 16;

void printRatioShades(struct column *col, int repCount, int *reps, int valCount, float *vals)
/* Print out representatives in shades of color in table background. */
{
int i;
float scale = col->expScale;
float val;
hPrintf("<TD><TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR>");
for (i=0; i<repCount; ++i)
    {
    int ix = reps[i];
    if (ix > valCount)
        errAbort("Representative larger than biggest experiment in %s", col->name);
    val = vals[ix];
    if (val <= -9999)
        hPrintf("<TD WIDTH=%d>&nbsp;</TD>", expSubcellWidth);
    else
	{
	hPrintf("<TD WIDTH=%d BGCOLOR=\"#", expSubcellWidth);
	colorVal(val*scale, FALSE);
	hPrintf("\">&nbsp;</TD>");
//	hPrintf("<TD>%d:%3.2f </TD", ix, val*scale);
	}
    }
hPrintf("</TR></TABLE></TD>");
}

void expRatioCellPrint(struct column *col, char *geneId, 
	struct sqlConnection *conn)
/* Print out html for expRatio cell. */
{
int i, numExpts = col->representativeCount;
struct bed *bed;
struct sqlResult *sr;
char **row, query[256];
char expName[64];

safef(query, sizeof(query), "select value from %s where name = '%s'", 
	col->table, geneId);
if (sqlQuickQuery(conn, query, expName, sizeof(expName)) == NULL)
    {
    hPrintf("<TD>n/a</TD>");
    return;
    }
safef(query, sizeof(query), "select expScores from %s where name = '%s'",
	col->posTable, expName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    hPrintf("<TD>n/a???</TD>");
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
char query[256], nameBuf[64];

AllocArray(names, expCount);
for (i=0; i<expCount; ++i)
    {
    safef(query, sizeof(query), "select name from %s where id = %d", 
    	table, expIds[i]);
    if ((name = sqlQuickQuery(conn, query, nameBuf, sizeof(nameBuf))) == NULL)
        name = "unknown";
    names[i] = cloneString(name);
    }
sqlDisconnect(&conn);
return names;
}

void expRatioLabelPrint(struct column *col)
/* Print out labels of various experiments. */
{
int i, numExpts = col->representativeCount;
char **experiments = getExperimentNames("hgFixed", col->expTable, numExpts,
	col->representatives);
hPrintf("<TH><TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR>\n");
for (i=0; i<numExpts; ++i)
    {
    boolean doGreen = (i&1);
    char *label = experiments[i], c;
    hPrintf("<TD VALIGN=BOTTOM ALIGN=MIDDLE");
    if (doGreen)
	hPrintf(" BGCOLOR=\"#E0FFE0\"");
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
    }
hPrintf("</TR></TABLE></TH>\n");

/* CLean up */
for (i=0; i<numExpts; ++i)
   freeMem(experiments[i]);
freeMem(experiments);
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
        reps[i] = sqlUnsigned(repStrings[i]);
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

col->exists = expRatioExists;
col->cellVal = expRatioCellVal;
col->cellPrint = expRatioCellPrint;
col->labelPrint = expRatioLabelPrint;
}
