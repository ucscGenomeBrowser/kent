/* ispyFeatures.c -- enables access of ispy clinical features for feature sorter */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "hdb.h"
#include "hgHeatmap.h"
#include "sqlList.h"
#include "ispyFeatures.h"
#include "userSettings.h"
#include "hPrint.h"

char *lookupItemUrlVal(struct column *col, char *sVal,
                       struct sqlConnection *conn)
{
char query[512];
safef(query, sizeof(query), col->itemUrlQuery, sVal);
return sqlQuickString(conn, query);
}

char *columnSetting(struct column *col, char *name, char *defaultVal)
/* Return value of named setting in column, or default if it doesn't exist. */
{
char *result = hashFindVal(col->settings, name);
if (result == NULL)
    result = defaultVal;
return result;
}

int columnSettingInt(struct column *col, char *name, int defaultVal)
/* Return value of named integer setting or default if it doesn't exist. */
{
char *result = hashFindVal(col->settings, name);
if (result == NULL)
    return defaultVal;
return atoi(result);
}

static void hPrintSpaces(int count)
/* Print count number of spaces. */
{
while (--count >= 0)
    hPrintf(" ");
}

char *colInfoUrl(struct column *col)
/* Return URL to get column info.  freeMem this when done. */
{
char *labelUrl;
if ((labelUrl = columnSetting(col, "labelUrl", NULL)) != NULL)
    return labelUrl;
else
    {
    char url[512];
    safef(url, sizeof(url), "../cgi-bin/hgHeatmap?%s&%s=%s",
	  cartSidUrlString(cart), colInfoVarName, col->name);
    return cloneString(url);
    }
}

void colInfoAnchor(struct column *col)
/* Print anchor tag that leads to column info page. */
{
char *url = colInfoUrl(col);
hPrintf("<A HREF=\"%s\">", url);
freeMem(url);
}

void colInfoLink(struct column *col)
/* Print link to column. */
{
colInfoAnchor(col);
hPrintf("%s</A>", col->shortLabel);
}


void labelSimplePrint(struct column *col)
/* This just prints cell->shortLabel.  If colWidth is
 * set it will add spaces, center justifying it.  */
{
int colWidth = columnSettingInt(col, "colWidth", 0);
  
hPrintf("<TH ALIGN=LEFT VALIGN=BOTTOM><B><PRE>");
/* The <PRE> above helps Internet Explorer avoid wrapping
 * in the label column, which helps us avoid wrapping in
 * the data columns below.  Wrapping in the data columns
 * makes the expression display less effective so we try                                          
 * to minimize it.  -jk */
if (colWidth == 0)
    {
    colInfoLink(col);
    }
else
    {
    int labelLen = strlen(col->shortLabel);
    int diff = colWidth - labelLen;
    if (diff < 0) diff = 0;
    colInfoLink(col);
    hPrintSpaces(diff);
    }
hPrintf("</PRE></B></TH>");
}

void cellSimplePrintExt(struct column *col, struct slName *id,
                        struct sqlConnection *conn, boolean lookupForUrl)
     /* This just prints one field from table. */
{
char *s = col->cellVal(col, id, conn);
hPrintf("<TD>");
if (s == NULL)
    {
    hPrintf("n/a");
    }
else
    {
    if (col->itemUrl != NULL)
        {
	hPrintf("<A HREF=\"");
	char *sVal = id->name;
	if (lookupForUrl)
            sVal = s;
	if (col->itemUrlQuery)
            sVal = lookupItemUrlVal(col, sVal, conn);
	hPrintf(col->itemUrl, sVal);
	if (col->itemUrlQuery)
            freez(&sVal);
	hPrintf("\"");
	hPrintf(">");
	hPrintNonBreak(s);
	hPrintf("</A>");
        }
    else
        {
	hPrintNonBreak(s);
        }
    freeMem(s);
    }
hPrintf("</TD>");
}

void cellSimplePrint(struct column *col, struct slName *id,
                     struct sqlConnection *conn)
/* This just prints one field from table. */
{
cellSimplePrintExt(col, id, conn, TRUE);
}

static boolean alwaysExists(struct column *col, struct sqlConnection *conn)
/* We always exist. */
{
return TRUE;
}

boolean simpleTableExists(struct column *col, struct sqlConnection *conn)
/* This returns true if col->table exists. */
{
return sqlTableExists(conn, col->table);
}

static char *noVal(struct column *col, struct slName *id, struct sqlConnection *conn)
/* Return not-available value. */
{
return cloneString("n/a");
}

static char *noColVal(struct column *col, struct sqlConnection *conn)
/* Return not-available value. */
{
return cloneString("n/a");
}

static int oneColumn(struct column *col)
/* Return that we have single column. */
{
return 1;
}

void columnDefaultMethods(struct column *col)
/* Set up default methods. */
{
col->exists = alwaysExists;
col->cellVal = noVal;
col->cellMinVal = noColVal;
col->cellMaxVal = noColVal;
col->cellAvgVal = noColVal;
col->cellPrint = cellSimplePrint;
col->labelPrint = labelSimplePrint;
col->tableColumns = oneColumn;
}

void cellSelfLinkPrint(struct column *col, struct slName *id,
                       struct sqlConnection *conn)
/* Print self and hyperlink to make this the search term. */
{
char *s = col->cellVal(col, id, conn);
if (s == NULL)
    s = cloneString("n/a");
hPrintf("<TD>");

hPrintf("%s</A></TD>", s);
freeMem(s);
}

static char *numberVal(struct column *col, struct slName *id,
                       struct sqlConnection *conn)
/* Return incrementing number. */
{
static int ix = 0;
char buf[15];
++ix;
safef(buf, sizeof(buf), "%d", ix);
return cloneString(buf);
}

void setupColumnNum(struct column *col, char *parameters)
/* Set up column that displays index in displayed list. */
{
col->cellVal = numberVal;
col->cellPrint = cellSelfLinkPrint;
}


/* ---- Simple table lookup type columns ---- */

char *cellLookupVal(struct column *col, struct slName *id, 
	struct sqlConnection *conn)
/* Get a field in a table defined by col->table, col->keyField, 
 * col->valField.  If an xrefLookup is specified in col->settings,
 * use that to look up an alternate name for the result. */
{
char *xrefDb = hashFindVal(col->settings, "xrefDb");
char *xrefTable = hashFindVal(col->settings, "xrefTable");
char *xrefNameField = hashFindVal(col->settings, "xrefNameField");
char *xrefAliasField = hashFindVal(col->settings, "xrefAliasField");
char query[1024];
if (xrefDb)
    safef(query, sizeof(query),
	  "select %s.%s.%s "
	  "from %s.%s, %s "
	  "where %s.%s = '%s' "
	  "and %s.%s = %s.%s.%s;",
	  xrefDb, xrefTable, xrefAliasField,
	  xrefDb, xrefTable,   col->table,
	  col->table, col->keyField,   id->name,
	  col->table, col->valField,   xrefDb, xrefTable, xrefNameField);
else
    safef(query, sizeof(query), "select %s from %s where %s = '%s'",
	  col->valField, col->table, col->keyField, id->name);

return sqlQuickString(conn, query);
}

char *cellLookupMinVal(struct column *col, struct sqlConnection *conn)
/* Get minimum value of column in database */
{
char query[512];
safef(query, sizeof(query), "select min(%s) from %s", col->valField, col->table);
return sqlQuickString(conn, query);
}

char *cellLookupMaxVal(struct column *col, struct sqlConnection *conn)
/* Get maximum value of column in database */
{
char *max = cloneString(hashFindVal(col->settings, "max"));
if (max)
    return max;

char query[512];
safef(query, sizeof(query), "select max(%s) from %s", col->valField, col->table);
return sqlQuickString(conn, query);
}

char *cellLookupAvgVal(struct column *col, struct sqlConnection *conn)
/* Get average value of column in database */
{
char query[512];
safef(query, sizeof(query), "select avg(%s) from %s", col->valField, col->table);
return sqlQuickString(conn, query);
}

static void cellLookupConfigControls(struct column *col)
{
hPrintf("<TD>");
char *name = configVarName(col, "sortType");
char *curVal = cartUsualString(cart, name, "unsorted");
static char *choices[3] = {"ascending", "unsorted", "descending"};
hPrintf("sort: ");
cgiMakeDropList(name, choices, ArraySize(choices), curVal);    
hPrintf("</TD>");
}

void setupColumnLookup(struct column *col, char *parameters)
/* Set up column that just looks up one field in a table
 * keyed by the geneId. */
{
char *xrefLookup = cloneString(hashFindVal(col->settings, "xrefLookup"));
col->table = cloneString(nextWord(&parameters));
col->keyField = cloneString(nextWord(&parameters));
col->valField = cloneString(nextWord(&parameters));
if (col->valField == NULL)
    errAbort("Not enough fields in type lookup for %s", col->name);
col->exists = simpleTableExists;
col->cellVal = cellLookupVal;
col->cellAvgVal = cellLookupAvgVal;
col->cellMinVal = cellLookupMinVal;
col->cellMaxVal = cellLookupMaxVal;

char *val = cartUsualString(cart, configVarName(col, "sortType"), "unsorted");                      
if (sameString(val, "ascending"))
    col->cellSortDirection = 1;
else if (sameString(val, "descending"))
    col->cellSortDirection = -1;
else
    col->cellSortDirection = 0;

col->configControls = cellLookupConfigControls;


if (isNotEmpty(xrefLookup))
    {
    char *xrefTable = nextWord(&xrefLookup);
    char *xrefNameField = nextWord(&xrefLookup);
    char *xrefAliasField = nextWord(&xrefLookup);
    if (isNotEmpty(xrefAliasField))
	{
	char *xrefOrg = hashFindVal(col->settings, "xrefOrg");
	char *xrefDb;
	if (xrefOrg)
	    xrefDb = hDefaultDbForGenome(xrefOrg);
	else
	    xrefDb = cloneString("ispy");
	struct sqlConnection *xrefConn = hAllocOrConnect(xrefDb);
	if (sqlTableExists(xrefConn, xrefTable))
	    {
	    /* These are the column settings that will be used by 
	     * cellLookupVal, so it doesn't have to parse xrefLookup and 
	     * query for table existence for each cell. */
	    hashAdd(col->settings, "xrefDb", xrefDb);
	    hashAdd(col->settings, "xrefTable", xrefTable);
	    hashAdd(col->settings, "xrefNameField", xrefNameField);
	    hashAdd(col->settings, "xrefAliasField", xrefAliasField);
	    }
	hFreeOrDisconnect(&xrefConn);
	}
    }
}


void setupColumnType(struct column *col)
/* Set up methods and column-specific variables based on
 * track type. */
{
char *dupe = cloneString(col->type);
char *s = dupe;
char *type = nextWord(&s);

columnDefaultMethods(col);

if (type == NULL)
    warn("Missing type value for column %s", col->name);
if (sameString(type, "num"))
    setupColumnNum(col, s);
else if (sameString(type, "lookup"))
    setupColumnLookup(col, s);
else
    errAbort("Unrecognized type %s for %s", col->type, col->name);
freez(&dupe);
}


static char *rootDir = "hgHeatmapData";

struct hash *readRa(char *rootName)
     /* Read in ra in root, root/org, and root/org/database. */
{
  return hgReadRa(genome, database, rootDir, rootName, NULL);
}

boolean columnSettingExists(struct column *col, char *name)
/* Return TRUE if setting exists in column. */
{
return hashFindVal(col->settings, name) != NULL;
}


int columnCmpPriority(const void *va, const void *vb)
/* Compare to sort columns based on priority. */
{
const struct column *a = *((struct column **)va);
const struct column *b = *((struct column **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
    return -1;
else if (dif > 0)
    return 1;
else
    return 0;
}

void refinePriorities(struct hash *colHash)
/* Consult colOrderVar if it exists to reorder priorities. */
{
char *orig = cartOptionalString(cart, colOrderVar);
if (orig != NULL)
    {
    char *dupe = cloneString(orig);
    char *s = dupe;
    char *name, *val;
    struct column *col;
    while ((name = nextWord(&s)) != NULL)
        {
	if ((val = nextWord(&s)) == NULL || !isdigit(val[0]))
            {
	    warn("Bad priority list: %s", orig);
	    cartRemove(cart, colOrderVar);
	    break;
            }
	col = hashFindVal(colHash, name);
	if (col != NULL)
            col->priority = atof(val);
        }
    freez(&dupe);
    }
}

void refineVisibility(struct column *colList)
/* Consult cart to set on/off visibility. */
{
char varName[128], *val;
struct column *col;

for (col = colList; col != NULL; col = col->next)
    {
    safef(varName, sizeof(varName), "%s%s.vis", colConfigPrefix, col->name);
    val = cartOptionalString(cart, varName);
    if (val != NULL)
        col->on = sameString(val, "1");
    }
}

char *mustFindInRaHash(char *fileName, struct hash *raHash, char *name)
/* Look up in ra hash or die trying. */
{
char *val = hashFindVal(raHash, name);
if (val == NULL)
    errAbort("Missing required %s field in %s", name, fileName);
return val;
}

void columnVarsFromSettings(struct column *col, char *fileName)
/* Grab a bunch of variables from col->settings and                                             
 * move them into col proper. */
{
struct hash *settings = col->settings;
col->name = mustFindInRaHash(fileName, settings, "name");

col->shortLabel = mustFindInRaHash(fileName, settings, "shortLabel");
col->longLabel = mustFindInRaHash(fileName, settings, "longLabel");
col->priority = atof(mustFindInRaHash(fileName, settings, "priority"));
col->on = col->defaultOn =
    sameString(mustFindInRaHash(fileName, settings, "visibility"), "on");
col->type = mustFindInRaHash(fileName, settings, "type");
col->itemUrl = hashFindVal(settings, "itemUrl");
col->itemUrlQuery = hashFindVal(settings, "itemUrlQuery");
}


static struct hash *hashColumns(struct column *colList)
/* Return a hash of columns keyed by name. */
{
struct column *col;
struct hash *hash = hashNew(8);
for (col = colList; col != NULL; col = col->next)
    {
    if (hashLookup(hash, col->name))
        warn("duplicate %s in column list", col->name);
    hashAdd(hash, col->name, col);
    }
return hash;
}

struct column *getColumns(struct sqlConnection *conn)
/* Return list of columns for big table. */
{
char *raName = "columnDb.ra";
struct column *col, *colList = NULL;
struct hash *raList = readRa(raName), *raHash = NULL;

/* Create built-in columns. */
if (raList == NULL)
    errAbort("Couldn't find anything from %s", raName);
for (raHash = raList; raHash != NULL; raHash = raHash->next)
    {
    AllocVar(col);
    col->settings = raHash;
    columnVarsFromSettings(col, raName);
    
    if (!hashFindVal(raHash, "hide"))
        {
	setupColumnType(col);
	if (col->exists(col, conn))
            {
	    slAddHead(&colList, col);
            }
	else
            {
	    hPrintf("\n%s doesn't exist", col->name);
            }
        }
    }

/* Put columns in hash */
columnHash = hashColumns(colList);

/* Tweak ordering and visibilities as per user settings. */
refinePriorities(columnHash);
refineVisibility(colList);
slSort(&colList, columnCmpPriority);

return colList;
}


