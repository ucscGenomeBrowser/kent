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

static char const rcsid[] = "$Id: expRatio.c,v 1.3 2003/06/22 04:33:30 kent Exp $";


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
        hexOne(val*0.7);    /* Green - much brighter than blue*/
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
        hexOne(val*0.8);    /* Green - brighter than red*/
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

static boolean expRatioUseBlue = TRUE;
static int expSubcellWidth = 16;

void expRatioCellPrint(struct column *col, char *geneId, 
	struct sqlConnection *conn)
/* Print out html for expRatio cell. */
{
int i;
hPrintf("<TD><TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR>");
for (i=0; i<9; ++i)
    {
    hPrintf("<TD WIDTH=%d BGCOLOR=\"#", expSubcellWidth);
    colorVal(i*0.1, FALSE);
    hPrintf("\">&nbsp;</TD>");
    }
hPrintf("</TR></TABLE></TD>");
}

void expRatioLabelPrint(struct column *col)
/* Print out labels of various experiments. */
{
static char *experiments[] = {"Soul", "Fortitude", "Anxiety", "Elbow", "Boots", "Wanderlust", "Reading", "Ok", "Bonkers", "Wild"};

int i, numExpts = 9;
hPrintf("<TH><TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR>\n");
for (i=0; i<numExpts; ++i)
    {
    char *label = experiments[i], c;
    hPrintf("<TD VALIGN=BOTTOM ALIGN=MIDDLE>");
    hPrintf("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>\n");
    while ((c = *label++) != 0)
        {
	hPrintf(" <TR ><TT><TD ALIGN=MIDDLE WIDTH=%d>%c</TD></TT></TR>\n", 
		expSubcellWidth, c);
	}
    hPrintf("</TABLE></TD>\n");
    }
hPrintf("</TR></TABLE></TH>\n");
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

if (expMax != NULL)
    col->expMax = atof(expMax);
else
    col->expMax = 3.0;

col->exists = expRatioExists;
col->cellVal = expRatioCellVal;
col->cellPrint = expRatioCellPrint;
col->labelPrint = expRatioLabelPrint;
}
