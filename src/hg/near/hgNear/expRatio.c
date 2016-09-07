/* Handle expression ratio (and expMulti) type microarray data. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "sqlList.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "cheapcgi.h"
#include "hgExp.h"
#include "hgNear.h"



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

if (hgExpLoadVals(lookupConn, dataConn, lookupTable, gp->name, dataTable, &valCount, &vals))
    {
    for (i=0; i<representativeCount; ++i)
	{
	int ix = representatives[i];
	if (ix != -1)
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
boolean tableOk = sameWord(col->table, "null") || sqlTableExists(conn, col->table);
boolean posTableOk = sqlTableExists(conn, col->posTable);
boolean expTableOk = sqlTableExists(conn, col->experimentTable);
return tableOk && posTableOk && expTableOk;
}

void expRatioCellPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Print out html for expRatio cell. */
{
hgExpCellPrint(col->name, gp->name, conn, col->table, conn, col->posTable, 
	col->representativeCount, col->representatives,
	col->expRatioUseBlue, col->forceGrayscale, !col->forceGrayscale, col->brightness);
}


void expLabelPrint(struct column *col, char *subName, 
	int representativeCount, int *representatives,
	char *expTable)
/* Print out labels of various experiments. */
{
int skipName = atoi(columnSetting(col, "skipName", "0"));
char *url = colInfoUrl(col);
hgExpLabelPrint(database, col->name, subName, skipName, url,
	representativeCount, representatives, expTable, 0);
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
char *val = cartUsualString(cart, varName, columnSetting(col, "brightness", "1.0"));
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
char *experimentType = cloneString(columnSetting(col, "experimentType",
						 "tissue"));
char **experiments = hgExpGetNames(database, experimentTable, 
	representativeCount, representatives, skipName);

hPrintf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
toUpperN(experimentType, 1);
hPrintf("<TR><TH>%s</TH><TH>Minimum</TH><TH>Maximum</TH></TR>\n",
	experimentType);
for (i=0; i<representativeCount; ++i)
    {
    int ix = representatives[i];
    if (ix != -1)
        {
	hPrintf("<TR>");
	hPrintf("<TD>&nbsp;%s</TD>", experiments[i]);
	safef(lVarName, sizeof(lVarName), "%smin%d", subName, ix);
	hPrintf("<TD>");
	advFilterRemakeTextVar(col, lVarName, 8);
	hPrintf("</TD>");
	safef(lVarName, sizeof(lVarName), "%smax%d", subName, ix);
	hPrintf("<TD>");
	advFilterRemakeTextVar(col, lVarName, 8);
	hPrintf("</TD>");
	hPrintf("</TR>\n");
	}
    }
hPrintf("</TABLE>\n");
hPrintf("Include if ");
advFilterAnyAllMenu(col, "logic", FALSE);
toLowerN(experimentType, 1);
hPrintf(" %ss meet minimum/maximum criteria.", experimentType);
freeMem(experimentType);
}

static void explainLogTwoRatio(char *maxVal, char *experimentType)
/* Put up note on log base 2 expression values. */
{
hPrintf("Note: the values here range from about -%s to %s.<BR>\n",
	maxVal, maxVal);
hPrintf("These are calculated as logBase2(%s/reference).<BR>\n",
	experimentType);
}

void expRatioFilterControls(struct column *col, struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
char *maxVal = columnSetting(col, "max", "3.0");
char *experimentType = columnSetting(col, "experimentType", "tissue");
explainLogTwoRatio(maxVal, experimentType);
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

sqlSafef(query, sizeof(query), "select name,expScores from %s", table);
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

sqlSafef(query, sizeof(query), "select %s,%s from %s", keyField, valField,table);
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
    struct hash *nameExpHash = sameWord(lookupTable,"null") ? expHash :
	getNameExpHash(lookupConn, lookupTable, "name", "value", expHash);
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
    char *forceGrayscale = columnSetting(col, "forceGrayscale", "off");
    col->forceGrayscale = (sameWord(forceGrayscale,"on")) ? TRUE : FALSE;
    col->expRatioUseBlue = hgExpRatioUseBlue(cart, expRatioColorVarName);
    }

/* Figure out scale. */
    {
    char *varName = configVarName(col, "scale");
    char *val = cartUsualString(cart, varName, "1");
    char *raVal = columnSetting(col, "brightness", "1");
    double fVal = (cartVarExists(cart, varName)) ? atof(val) : atof(raVal);
    if (fVal == 0) fVal = 1.0;
    col->brightness = fVal/atof(expMax);
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
float scale;

if (col->expShowAbs)
    {
    dataTable = emd->absoluteTable;
    scale = col->expAbsScale;
    }
else
    {
    dataTable = emd->ratioTable;
    scale = col->expRatioScale;
    }
hgExpCellPrint(col->name, gp->name, conn, col->table, hgFixedConn(), dataTable, 
	emd->representativeCount, emd->representatives,
	col->expRatioUseBlue, col->expShowAbs, TRUE, scale);
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
char *experimentType = columnSetting(col, "experimentType", "tissue");
hPrintf("%ss: ", experimentType);
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
char emfPrefix[64];
struct expMultiData *emd = col->emd;
if (col->expShowAbs)
    {
    char *absoluteMax = columnSetting(col, "absoluteMax", "30000");
    explainAbsolute(absoluteMax);
    }
else
    {
    char *ratioMax = columnSetting(col, "ratioMax", "3.0");
    char *experimentType = columnSetting(col, "experimentType", "tissue");
    explainLogTwoRatio(ratioMax, experimentType);
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
char *ratioMax = columnSetting(col, "ratioMax", "3.0");
char *absoluteMax = columnSetting(col, "absoluteMax", "30000");

col->table = cloneString(nextWord(&parameters));
if (col->table == NULL)
    errAbort("missing parameters from type line of %s", col->name);    

/* Check that we have at least one set of experiments to display,
 * and set the current experiments. */
makeEmd(col, "all", "all replicates", &emdList);
// customize otherwise confusing menu option for GTEx.  Simply, this option selects all tissues.
makeEmd(col, "median", startsWith("gtex", col->name) ? "all" : "median of replicates", &emdList);
makeEmd(col, "selected", "selected", &emdList);
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
    col->expRatioUseBlue = hgExpRatioUseBlue(cart, expRatioColorVarName);
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
boolean noLookup = sameWord(col->table, "null");
if (!noLookup)
    sqlSafef(query, sizeof(query), "select value from %s where name = '%s'", 
	  col->table, gp->name);
if (noLookup || 
    (sqlQuickQuery(conn, query, expName, sizeof(expName)) != NULL))
    {
    char *commaString = NULL;
    sqlSafef(query, sizeof(query), "select expScores from %s where name = '%s'",
	  col->posTable, (noLookup) ? gp->name : expName);
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


