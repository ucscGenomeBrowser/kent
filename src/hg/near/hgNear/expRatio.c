/* Handle expression ratio (and expMulti) type microarray data. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "sqlList.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "hgNear.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: expRatio.c,v 1.26 2003/10/13 19:33:15 kent Exp $";


static boolean loadExpVals(struct sqlConnection *lookupConn,
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

static char *expCellVal(struct genePos *gp,
    struct sqlConnection *lookupConn, char *lookupTable,
    struct sqlConnection *dataConn, char *dataTable,
    int representativeCount, int *representatives, char *format)
/* Create a comma-separated string of expression values. */
{
int i;
struct dyString *dy = newDyString(1024);
int valCount;
float *vals = NULL;
char *result;

if (loadExpVals(lookupConn, dataConn, lookupTable, gp->name, dataTable, &valCount, &vals))
    {
    for (i=0; i<representativeCount; ++i)
	{
	int ix = representatives[i];
	if (i != -1)
	    {
	    float val = vals[ix];
	    if (val < -9999)
	        dyStringPrintf(dy, "n/a");
	    else
	        dyStringPrintf(dy, format, val);
	    dyStringAppendC(dy, ',');
	    }
	}
    freez(&vals);
    }
else
    {
    dyStringPrintf(dy, "n/a");
    }
result = cloneString(dy->string);
dyStringFree(&dy);
return result;
}

static char *expRatioCellVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Get comma separated list of values. */
{
return expCellVal(gp, conn, col->table, conn, col->posTable,
	col->representativeCount, col->representatives, "%4.3f");
}

static boolean expRatioExists(struct column *col, struct sqlConnection *conn)
/* This returns true if relevant tables exist. */
{
boolean tableOk = sqlTableExists(conn, col->table);
boolean posTableOk = sqlTableExists(conn, col->posTable);
boolean expTableOk = sqlTableExists(conn, col->experimentTable);
return tableOk && posTableOk && expTableOk;
}

static void hexOne(double val)
/* Convert val 0.0-1.0 to hex 00 to FF */
{
int hex = val * 0xFF;
if (hex > 0xFF) hex = 0xFF;
hPrintf("%02X", hex);
}

static void colorVal(double val, double scale, boolean useBlue, boolean useGrays)
/* Val is -1.0 to 1.0.  Print color in form #FF0000, normally
 * using green for minus values, red for plus values, but
 * optionally using blue for minus values and yellow for plus values. */
{
if (useGrays)
    {
    if (val < 1)
        hPrintf("000000");
    else
	{
	val = log(val) * scale;
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
	    hPrintf("00");
	    hPrintf("00");
	    hexOne(val);
	    }
	else
	    {
	    val *= 0.7;
	    hexOne(val);    /* Red */
	    hexOne(val);     /* Green */
	    hPrintf("00");   /* Blue */
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
	int *reps, int valCount, float *vals, 
	boolean colorBlindColors,
	boolean useGrays, float scale)
/* Print out representatives in shades of color in table background. */
{
int i;
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
	    colorVal(val, scale, colorBlindColors, useGrays);
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

void expCellPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *lookupConn, char *lookupTable,
	struct sqlConnection *dataConn, char *dataTable,
	int representativeCount, int *representatives,
	boolean useBlue, boolean useGrays, float scale)
/* Print out html for expRatio cell. */
{
int i, numExpts = col->representativeCount;
int valCount;
float *vals = NULL;

if (loadExpVals(lookupConn, dataConn, lookupTable, gp->name, dataTable, 
	&valCount, &vals))
    {
    printRatioShades(col, representativeCount, representatives, 
    	valCount, vals, useBlue, useGrays, scale);
    freez(&vals);
    }
else
    {
    replicate("n/a", representativeCount, representatives);
    }
}

void expRatioCellPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Print out html for expRatio cell. */
{
expCellPrint(col, gp, conn, col->table, conn, col->posTable, 
	col->representativeCount, col->representatives,
	col->expRatioUseBlue, FALSE, col->expScale);
}


static char **getExperimentNames(char *database, char *table, 
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
	char *expTable)
/* Print out labels of various experiments. */
{
int i;
int groupSize, gifCount = 0;
char gifName[128];
char **experiments = getExperimentNames("hgFixed", 
	expTable, representativeCount, representatives, skipName);
int height = gifLabelMaxWidth(experiments, representativeCount);

for (i=0; i<representativeCount; i += groupSize+1)
    {
    hPrintf("<TD VALIGN=\"BOTTOM\">");
    groupSize = countNonNull(experiments+i, representativeCount-i);
    safef(gifName, sizeof(gifName), "../trash/near_%s_%s%d.gif", 
    	colName, subName, ++gifCount);
    gifLabelVerticalText(gifName, experiments+i, groupSize, height);
    if (url != NULL)
       hPrintf("<A HREF=\"%s\">", url); 
    hPrintf("<IMG BORDER=0 SRC=\"%s\">", gifName);
    if (url != NULL)
	hPrintf("</A>");
    hPrintf("</TD>");
    }

/* Clean up */
for (i=0; i<representativeCount; ++i)
   freeMem(experiments[i]);
freeMem(experiments);
}

void expLabelPrint(struct column *col, char *subName, 
	int representativeCount, int *representatives,
	char *expTable)
/* Print out labels of various experiments. */
{
int skipName = atoi(columnSetting(col, "skipName", "0"));
char *url = colInfoUrl(col);
    colInfoAnchor(col);
hgExpLabelPrint(col->name, subName, skipName, url,
	representativeCount, representatives, expTable);
freeMem(url);
}

void expRatioLabelPrint(struct column *col)
/* Print out labels of various experiments. */
{
expLabelPrint(col, "", col->representativeCount, 
	col->representatives, col->experimentTable);
}


static void expBrightnessControl(struct column *col)
/* Put up brightness text box. */
{
char *varName = configVarName(col, "scale");
char *val = cartUsualString(cart, varName, "1.0");
hPrintf("brightness: ");
cgiMakeTextVar(varName, val, 3);
}

static void expRatioConfigControls(struct column *col)
/* Print out configuration column */
{
hPrintf("<TD>");
expBrightnessControl(col);
hPrintf("</TD>");
}

void expFilterControls(struct column *col, char *subName,
	char *experimentTable, int representativeCount, int *representatives)
/* Print out controls for advanced filter. */
{
char lVarName[64];
int i;
int skipName = atoi(columnSetting(col, "skipName", "0"));
char **experiments = getExperimentNames("hgFixed", experimentTable, 
	representativeCount, representatives, skipName);

hPrintf("<TABLE BORDER=1 CELLSPACING=1 CELLPADDING=1>\n");
hPrintf("<TR><TH>Tissue</TH><TH>Minimum</TH><TH>Maximum</TH></TR>\n");
for (i=0; i<representativeCount; ++i)
    {
    int ix = representatives[i];
    hPrintf("<TR>");
    if (ix != -1)
        {
	hPrintf("<TD>%s</TD>", experiments[i]);
	safef(lVarName, sizeof(lVarName), "%smin%d", subName, ix);
	hPrintf("<TD>");
	advFilterRemakeTextVar(col, lVarName, 8);
	hPrintf("</TD>");
	safef(lVarName, sizeof(lVarName), "%smax%d", subName, ix);
	hPrintf("<TD>");
	advFilterRemakeTextVar(col, lVarName, 8);
	hPrintf("</TD>");
	}
    hPrintf("</TR>");
    }
hPrintf("</TABLE>\n");
hPrintf("Include if ");
advFilterAnyAllMenu(col, "logic", FALSE);
hPrintf(" tissues meet minimum/maximum criteria.");
}

static void explainLogTwoRatio(char *maxVal)
/* Put up note on log base 2 expression values. */
{
hPrintf("Note: the values here range from about -%s to %s.<BR>\n",
	maxVal, maxVal);
hPrintf("These are calculated as logBase2(tissue/reference).<BR>\n");
}

void expRatioFilterControls(struct column *col, struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
char *maxVal = columnSetting(col, "max", "3.0");
explainLogTwoRatio(maxVal);
expFilterControls(col, "", col->experimentTable, 
	col->representativeCount, col->representatives);
}

static struct hash *expValHash(struct sqlConnection *conn, char *table)
/* Load up all expresssion data into a hash keyed by table->name */
{
char query[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(16);

safef(query, sizeof(query), "select name,expScores from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if (!hashLookup(hash, name))
        {
	float *vals = NULL;
	int valCount;
	sqlFloatDynamicArray(row[1], &vals, &valCount);
	hashAdd(hash, name, vals);
	}
    }
sqlFreeResult(&sr);
return hash;
}

struct hash *getNameExpHash(struct sqlConnection *conn, 
	char *table, char *keyField, char *valField, struct hash *expHash)
/* For each key in table lookup val in expHash, and if there
 * put key/exp in table. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(16);

safef(query, sizeof(query), "select %s,%s from %s", keyField, valField,table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    float *val = hashFindVal(expHash, row[1]);
    if (val != NULL)
        hashAdd(hash, row[0], val);
    }
sqlFreeResult(&sr);
return hash;
}

static char *expLimitString(struct column *col, char *subName, 
	int *representatives, int repIx, boolean isMax)
/* Return string value of min/max variable for a given expression
 * experiment. */
{
char varName[64];
static char *varType[2] = {"min", "max"};
safef(varName, sizeof(varName), "%s%s%d", subName,
	varType[isMax], representatives[repIx]);
return advFilterVal(col, varName);
}

static boolean expAdvFilterUsed(struct column *col, char *subName)
/* Return TRUE if any of the advanced filter variables
 * for this col are set. */
{
char wild[128];
safef(wild, sizeof(wild), "%s%s.%s*", advFilterPrefix, col->name, subName);
return anyRealInCart(cart, wild);
}

struct genePos *expAdvFilter(struct column *col, char *subName,
	struct sqlConnection *lookupConn, char *lookupTable,
	struct sqlConnection *dataConn, char *dataTable,
	int representativeCount, int *representatives, struct genePos *list)
/* Do advanced filter on position. */
{
if (expAdvFilterUsed(col, subName))
    {
    struct hash *expHash = expValHash(dataConn, dataTable);
    struct hash *nameExpHash = getNameExpHash(lookupConn, lookupTable, 
    	"name", "value", expHash);
    int i;
    boolean orLogic = advFilterOrLogic(col, "logic", FALSE);
    int isMax;
    int repIx;
    char *varValString;
    char **mms[2];	/* Min/max strings. */
    float *mmv[2];	/* Min/max values. */
    struct genePos *newList = NULL, *gp, *next;

    /* Fetch all limit variables. */
    for (isMax=0; isMax<2; ++isMax)
	{
	AllocArray(mms[isMax], representativeCount);
	AllocArray(mmv[isMax], representativeCount);
	for (repIx=0; repIx < representativeCount; ++repIx)
	    {
	    varValString = mms[isMax][repIx] = 
	    		expLimitString(col, subName, representatives, repIx, isMax);
	    if (varValString != NULL)
		{
		mmv[isMax][repIx] = atof(varValString);
		}
	    }
	}

    /* Step through each item in list and figure out if it is in. */
    for (gp = list; gp != NULL; gp = next)
	{
	float *vals;
	next = gp->next;
	if ((vals = hashFindVal(nameExpHash, gp->name)) != NULL)
	    {
	    boolean passes = !orLogic;
	    for (repIx=0; repIx<representativeCount; ++repIx)
		{
		boolean anyLimit = FALSE;
		boolean passesOne = TRUE;
		float val = vals[representatives[repIx]];
		for (isMax=0; isMax<2; ++isMax)
		    {
		    if (mms[isMax][repIx] != NULL)
			{
			float varVal = mmv[isMax][repIx];
			anyLimit = TRUE;
			if (isMax)
			    {
			    if (val > varVal)
				passesOne = FALSE;
			    }
			else
			    {
			    if (val < varVal)
				passesOne = FALSE;
			    }
			}
		    }
		if (anyLimit)
		    {
		    if (orLogic)
			{
			passes |= passesOne;
			}
		    else
			{
			passes &= passesOne;
			}
		    }
		}
	    if (passes)
		{
		slAddHead(&newList, gp);
		}
	    }
	}
    slReverse(&newList);
    list = newList;

    /* cleanup . */
    for (isMax=0; isMax<2; ++isMax)
	{
	freez(&mms[isMax]);
	freez(&mmv[isMax]);
	}
    freeHash(&nameExpHash);
    freeHashAndVals(&expHash);
    }
return list;
}

struct genePos *expRatioAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on position. */
{
return expAdvFilter(col, "", conn, col->table, conn, col->posTable,
	col->representativeCount, col->representatives, list);
}

int expRatioTableColumns(struct column *col)
/* Return number of html columns this uses. */
{
int *reps = col->representatives;
int repCount = col->representativeCount;
int i, count = 1;

for (i=0; i<repCount; ++i)
    if (reps[i] == -1)
        ++count;
return count;
}


void setupColumnExpRatio(struct column *col, char *parameters)
/* Set up expression ratio type column. */
{
/* Hash up name value pairs and extract some we care about. */
char *repList = columnSetting(col, "representatives", NULL);
char *expMax = columnSetting(col, "max", "3.0");

col->table = cloneString(nextWord(&parameters));
col->posTable = cloneString(nextWord(&parameters));
col->experimentTable = cloneString(nextWord(&parameters));
if (col->posTable == NULL)
    errAbort("missing parameters from type line of %s", col->name);    


/* Convert list of ascii number in repList to an array of binary
 * numbers in col->representatives. */
if (repList != NULL)
    {
    char *dupe = cloneString(repList);
    sqlSignedDynamicArray(dupe, &col->representatives, &col->representativeCount);
    freez(&dupe);
    }
else
    {
    errAbort("Missing required representatives list in %s", col->name);
    }


/* Figure out color scheme. */
    {
    col->expRatioUseBlue = expRatioUseBlue();
    }

/* Figure out scale. */
    {
    char *varName = configVarName(col, "scale");
    char *val = cartUsualString(cart, varName, "1");
    double fVal = atof(val);
    if (fVal == 0) fVal = 1.0;
    col->expScale = fVal/atof(expMax);
    }

col->exists = expRatioExists;
col->cellVal = expRatioCellVal;
col->cellPrint = expRatioCellPrint;
col->labelPrint = expRatioLabelPrint;
col->configControls = expRatioConfigControls;
col->filterControls = expRatioFilterControls;
col->advFilter = expRatioAdvFilter;
}

/* --------- expMulti stuff for more complex expression ratio tracks -------- */

static struct expMultiData *emdNew(char *name, char *label, char *spec)
/* Create new expMultiData based on spec. */
{
char *dupe = cloneString(spec);
char *s = dupe;
char *repString;
struct expMultiData *emd;

AllocVar(emd);
emd->name = cloneString(name);
emd->shortLabel = cloneString(label);
emd->experimentTable = cloneString(nextWord(&s));
emd->ratioTable = cloneString(nextWord(&s));
emd->absoluteTable = cloneString(nextWord(&s));
repString = nextWord(&s);
if (repString == NULL)
    errAbort("Short spec in emdNew");
sqlSignedDynamicArray(repString, &emd->representatives, &emd->representativeCount);
freeMem(dupe);
return emd;
}

static struct expMultiData *makeEmd(struct column *col, char *name, char *label,
	struct expMultiData **pList)
/* Create new expMultiData based on specs in column settings and hang it on list. */
{
char *spec = hashFindVal(col->settings, name);
struct expMultiData *emd;
if (spec == NULL)
    return NULL;
emd = emdNew(name, label, spec);
slAddHead(pList, emd);
return emd;
}

struct expMultiData *emdFind(struct expMultiData *list, char *name)
/* Find named expMultiData, or NULL if not on list. */
{
struct expMultiData *emd;
for (emd = list; emd != NULL; emd = emd->next)
    if (sameString(name, emd->name))
        return emd;
return NULL;
}

struct expMultiData *getSelectedEmd(struct column *col, struct expMultiData *emdList)
/* Get user selected column or default. */
{
struct expMultiData *emd = NULL;
char *emdVal = configVarVal(col, "emd");
if (emdVal != NULL)
   emd = emdFind(emdList, emdVal);
if (emd == NULL)
   emd = emdList;
return emd;
}

static boolean expMultiExists(struct column *col, struct sqlConnection *conn)
/* Returns true if relevant tables exist. */
{
struct sqlConnection *fConn = hgFixedConn();
struct expMultiData *emd;
if (!sqlTableExists(conn, col->table))
    return FALSE;
for (emd = col->emdList; emd != NULL; emd = emd->next)
    {
    if (!sqlTableExists(fConn, emd->ratioTable))
        return FALSE;
    if (!sqlTableExists(fConn, emd->absoluteTable))
        return FALSE;
    }
return TRUE;
}

static char *expMultiCellVal(struct column *col, struct genePos *gp,
	struct sqlConnection *conn)
/* Return comma separated string. */
{
struct expMultiData *emd = col->emd;
char *dataTable;
char *format;
char *ret;

if (col->expShowAbs)
    {
    dataTable = emd->absoluteTable;
    format = "%2.1f";
    }
else
    {
    dataTable = emd->ratioTable;
    format = "%4.3f";
    }
ret = expCellVal(gp, conn, col->table, hgFixedConn(), dataTable,
	emd->representativeCount, emd->representatives, format);
return ret;
}

void expMultiCellPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Print out html for expMulti cell. */
{
struct expMultiData *emd = col->emd;
char *dataTable;
char *format;
float scale;

if (col->expShowAbs)
    {
    dataTable = emd->absoluteTable;
    format = "%2.1f";
    scale = col->expAbsScale;
    }
else
    {
    dataTable = emd->ratioTable;
    format = "%4.3f";
    scale = col->expRatioScale;
    }
expCellPrint(col, gp, conn, col->table, hgFixedConn(), dataTable, 
	emd->representativeCount, emd->representatives,
	col->expRatioUseBlue, col->expShowAbs, scale);
}

void expMultiLabelPrint(struct column *col)
/* Print out labels of various experiments. */
{
struct expMultiData *emd = col->emd;
expLabelPrint(col, emd->name, emd->representativeCount, 
	emd->representatives, emd->experimentTable);
}

static void expEmdControl(struct column *col)
/* Show selected/median/all control. */
{
struct expMultiData *emd;
struct expMultiData *curEmd = getSelectedEmd(col, col->emdList);
hPrintf("tissues: ");
hPrintf("<SELECT NAME=\"%s\">", configVarName(col, "emd"));
for (emd = col->emdList; emd != NULL; emd = emd->next)
    {
    hPrintf("<OPTION VALUE=\"%s\"", emd->name);
    if (emd == curEmd)
	hPrintf(" SELECTED");
    hPrintf(">%s", emd->shortLabel);
    }
hPrintf("</SELECT>\n");
}

static void expAbsRatioControl(struct column *col)
/* Show ratio/absolute control. */
{
char *name = configVarName(col, "ratioAbs");
char *curVal = cartUsualString(cart, name, "ratio");
static char *choices[2] = {"ratio", "absolute"};
hPrintf("values: ");
cgiMakeDropList(name, choices, ArraySize(choices), curVal);
}

static void expMultiConfigControls(struct column *col)
/* Print out configuration column */
{
hPrintf("<TD>");
hPrintf("<TABLE><TR><TD>");
expBrightnessControl(col);
hPrintf("</TD><TD>");
expEmdControl(col);
hPrintf("</TD><TD>");
expAbsRatioControl(col);
hPrintf("</TD></TR></TABLE>");
hPrintf("</TD>");
}

static void expMultiFilterPrefix(struct column *col, int maxPrefixSize, char *retPrefix )
/* Fill in prefix to use for filter variables in this multi configuration. */
{
char *absRel = (col->expShowAbs ? "abs" : "rel");
safef(retPrefix, maxPrefixSize, "%s.%s.", absRel, col->emd->name);
}

static void explainAbsolute(char *maxVal)
/* Explain a little about absolutes. */
{
hPrintf("Note: the values here range up to about %s.<BR>\n", maxVal);
hPrintf("Values of 20 or less indicate unmeasurable expression.<BR>\n");
}

static void expMultiFilterControls(struct column *col, 
	struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
char *dataTable;
char emfPrefix[64];
struct expMultiData *emd = col->emd;
if (col->expShowAbs)
    {
    char *absoluteMax = columnSetting(col, "absoluteMax", "30000");
    explainAbsolute(absoluteMax);
    dataTable = emd->absoluteTable;
    }
else
    {
    char *ratioMax = columnSetting(col, "ratioMax", "3.0");
    explainLogTwoRatio(ratioMax);
    dataTable = emd->ratioTable;
    }
expMultiFilterPrefix(col, sizeof(emfPrefix), emfPrefix);
expFilterControls(col, emfPrefix, emd->experimentTable, 
	emd->representativeCount, emd->representatives);
hPrintf("<BR>Other filter controls for this %s column will be<BR>", col->shortLabel);
hPrintf("displayed if the column is configured differently.  Only<BR>");
hPrintf("the displayed controls are active.");
}

struct genePos *expMultiAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on position. */
{
char *dataTable;
char emfPrefix[64];
struct expMultiData *emd = col->emd;
if (col->expShowAbs)
    dataTable = emd->absoluteTable;
else
    dataTable = emd->ratioTable;
expMultiFilterPrefix(col, sizeof(emfPrefix), emfPrefix);
return expAdvFilter(col, emfPrefix, conn, col->table, hgFixedConn(), dataTable,
	emd->representativeCount, emd->representatives, list);
}


void setupColumnExpMulti(struct column *col, char *parameters)
/* Set up expression type column that can be ratio or absolute,
 * short or long. */
{
struct expMultiData *emdList = NULL;
struct expMultiData *allEmd = makeEmd(col, "all", "all replicas", &emdList);
struct expMultiData *medianEmd = makeEmd(col, "median", "median of replicas", &emdList);
struct expMultiData *selectedEmd = makeEmd(col, "selected", "selected", &emdList);
char *ratioMax = columnSetting(col, "ratioMax", "3.0");
char *absoluteMax = columnSetting(col, "absoluteMax", "30000");

col->table = cloneString(nextWord(&parameters));
if (col->table == NULL)
    errAbort("missing parameters from type line of %s", col->name);    

/* Check that we have at least one set of experiments to display,
 * and set the current experiments. */
if (emdList == NULL)
   errAbort("Need at least one of all/median/selected for %s", col->name);
col->emdList = emdList;
col->emd = getSelectedEmd(col, emdList);


/* Figure out whether showing absolute or relative */
    {
    char *val = cartUsualString(cart, configVarName(col, "ratioAbs"), "ratio");
    col->expShowAbs = sameString(val, "absolute");
    }

/* Figure out color scheme. */
    {
    col->expRatioUseBlue = expRatioUseBlue();
    }

/* Figure out scale. */
    {
    char *varName = configVarName(col, "scale");
    char *val = cartUsualString(cart, varName, "1");
    double fVal = atof(val);
    if (fVal == 0) fVal = 1.0;
    col->expRatioScale = fVal/atof(ratioMax);
    col->expAbsScale = fVal/log(atof(absoluteMax));
    }

col->exists = expMultiExists;
col->configControls = expMultiConfigControls;
col->cellVal = expMultiCellVal;
col->cellPrint = expMultiCellPrint;
col->labelPrint = expMultiLabelPrint;
col->filterControls = expMultiFilterControls;
col->advFilter = expMultiAdvFilter;
}

/* --------- expMax stuff to show maximum expression value -------- */

boolean expMaxExists(struct column *col, struct sqlConnection *conn)
/* This returns true if col->table exists. */
{
return sqlTableExists(conn, col->table) 
	&& sqlTableExists(hgFixedConn(), col->posTable);
}

static float expMaxVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn, struct sqlConnection *fConn)
/* Return maximum expression value or -10000 if there's a problem. */
{
char query[256];
char expName[64];
float maxVal = -10000;

safef(query, sizeof(query), "select value from %s where name = '%s'", 
	col->table, gp->name);
if (sqlQuickQuery(conn, query, expName, sizeof(expName)) != NULL)
    {
    char *commaString = NULL;
    safef(query, sizeof(query), "select expScores from %s where name = '%s'",
    	col->posTable, expName);
    if ((commaString = sqlQuickString(fConn, query)) != NULL)
        {
	float *vals = NULL;
	int valCount, i;
	maxVal = atof(commaString);
	sqlFloatDynamicArray(commaString, &vals, &valCount);
	for (i=1; i<valCount; ++i)
	    if (maxVal < vals[i])
	        maxVal = vals[i];
	freeMem(commaString);
	freeMem(vals);
	}
    }
return maxVal;
}

static char *expMaxCellVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Return value as string for this cell. */
{
float maxVal = expMaxVal(col, gp, conn, hgFixedConn());
char buf[32];
if (maxVal <= -10000)
    return NULL;
safef(buf, sizeof(buf), "%1.1f", maxVal);
return cloneString(buf);
}

struct genePos *expMaxAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on max expression. */
{
char *minString = advFilterVal(col, "min");
char *maxString = advFilterVal(col, "max");
if (minString != NULL || maxString != NULL)
    {
    struct sqlConnection *fConn = hgFixedConn();
    struct genePos *passList = NULL, *gp, *next;
    float minVal = 0, maxVal = 0, val;
    if (minString != NULL)
        minVal = atof(minString);
    if (maxString != NULL)
        maxVal = atof(maxString);
    for (gp = list; gp != NULL; gp = next)
	{
	next = gp->next;
	val = expMaxVal(col, gp, conn, fConn);
	if (minString != NULL && val < minVal)
	    continue;
	if (maxString != NULL && val > maxVal)
	    continue;
	slAddHead(&passList, gp);
	}
    slReverse(&passList);
    return passList;
    }
else
    return list;
}

void setupColumnExpMax(struct column *col, char *parameters)
/* Set up maximum expression value column. */
{
col->table = cloneString(nextWord(&parameters));
col->posTable = cloneString(nextWord(&parameters));
if (col->posTable == NULL)
    errAbort("Not enough fields in type expMax for %s", col->name);
col->exists = expMaxExists;
col->cellVal = expMaxCellVal;
col->filterControls = minMaxAdvFilterControls;
col->advFilter = expMaxAdvFilter;
}


