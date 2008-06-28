/* microarray - Microarray data. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "hgExp.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: microarray.c,v 1.12 2008/06/28 16:37:57 galt Exp $";

struct expColumn
/* An expression column. */
    {
    struct expColumn *next;
    char *name;		/* Symbolic name. */
    float priority;	/* Sort order priority. */
    char *type;		/* What type this is. */
    struct hash *settings;	/* Rest of stuff. */
    char *probe;		/* Name of probe if any. */
    char *lookupTable;		/* Table to lookup gene ID to get expId. */
    };

static void expColumnFree(struct expColumn **pEl)
/* Free up expColumn. */
{
struct expColumn *el = *pEl;
if (el != NULL)
    {
    hashFree(&el->settings);
    freeMem(el->probe);
    freeMem(el->lookupTable);
    freez(pEl);
    }
}

static void expColumnFreeList(struct expColumn **pList)
/* Free a list of dynamically allocated expColumn's */
{
struct expColumn *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    expColumnFree(&el);
    }
*pList = NULL;
}


static int expColumnCmpPriority(const void *va, const void *vb)
/* Compare to sort expColumn based on priority. */
{
const struct expColumn *a = *((struct expColumn **)va);
const struct expColumn *b = *((struct expColumn **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
    return -1;
else if (dif > 0)
    return 1;
else
    return 0;
}

static char *expProbe(struct sqlConnection *conn, char *table, 
	char *geneId)
/* Lookup geneId in table */
{
char query[256];
safef(query, sizeof(query), "select value from %s where name='%s'",
	table, geneId);
return sqlQuickString(conn, query);
}

char *checkProbeData(struct sqlConnection *conn, char *table, char *probe)
/* Return probe if it exists in table, else NULL */
{
char query[256];
if (probe == NULL)
    return NULL;
safef(query, sizeof(query), "select count(*) from %s where name = '%s'",
	table, probe);
if (sqlQuickNum(conn, query) <= 0)
    probe = NULL;
return probe;
}

char *expRatioProbeCheck(struct sqlConnection *conn, char *geneId,
	char *lookup, char *parameters)
/* Check all necessary tables exist, and if so return 
 * probe name. */
{
char *data = nextWord(&parameters);
char *exp = nextWord(&parameters);
char *probe = NULL;
if (exp == NULL)
    errAbort("short expRatio type line");
if (!sqlTableExists(conn, lookup) 
	|| !sqlTableExists(conn, data) || !sqlTableExists(conn, exp))
    return NULL;
probe = expProbe(conn, lookup, geneId);
return checkProbeData(conn, data, probe);
}

static void expMultiSubtype(struct hash *ra, char *colName,
	char **retSubType, char **retSubName)
/* Return line with table, representatives etc
 * for our favorite subtype of an expMulti
 * column. */
{
char *subName = "median";
char *subType = hashFindVal(ra, subName);
if (subType == NULL)
    {
    subName = "all";
    subType = hashFindVal(ra, subName);
    }
if (subType == NULL)
    errAbort("Couldn't find all or median subtype in %s", colName);
*retSubType = subType;
*retSubName = subName;
}


char *expMultiProbeCheck(struct sqlConnection *conn, char *geneId, 
	char *lookup, char *parameters, char *colName, struct hash *ra)
/* Check all necessary tables exist, and if so return 
 * probe name. */
{
char *subName, *subType, *dupe, *s;
struct sqlConnection *fConn;
char *probe = NULL;
boolean ok;
char *ratio, *absolute, *exp;

/* Parse out and make sure that lookup table exists. */
if (!sqlTableExists(conn, lookup))
    return NULL;

/* Parse out other tables, usually from median line */
expMultiSubtype(ra, colName, &subType, &subName);
dupe = s = cloneString(subType);
exp = nextWord(&s);
ratio = nextWord(&s);
absolute = nextWord(&s);
if (absolute == NULL)
    errAbort("short %s line in %s", subName, colName);

/* Make sure other tables exist (in hgFixed by default */
fConn = sqlConnect("hgFixed");
ok = (sqlTableExists(fConn, exp) && sqlTableExists(fConn, ratio)
	&& sqlTableExists(fConn, absolute));
if (ok)
    probe = expProbe(conn, lookup, geneId);
probe = checkProbeData(fConn, absolute, probe);
sqlDisconnect(&fConn);
freez(&dupe);
return probe;
}

static struct expColumn *microarrayColumns(struct sqlConnection *conn, char *geneId)
/* Load up microarray columns that may exist from hgNearData/columnDb.ra */
{
struct hash *raNext, *ra, *raList = hgReadRa(genome, database, 
	"hgNearData", "columnDb.ra", NULL);
struct expColumn *colList = NULL;
for (ra = raList; ra != NULL; ra = raNext)
    {
    struct expColumn *col = NULL;
    char *fullType = hashMustFindVal(ra, "type");
    char *dupe = cloneString(fullType);
    char *s = dupe;
    char *type = nextWord(&s);
    raNext = ra->next;

    if ((sameString("expRatio", type) || sameString("expMulti", type)) && !hashLookup(ra, "hgGeneHide"))
        {
	char *name = hashMustFindVal(ra, "name");
	char *probe = NULL;
	char *lookup = nextWord(&s);
	if (sameString("expRatio", type))
	    probe = expRatioProbeCheck(conn, geneId, lookup, s);
	else if (sameString("expMulti", type))
	    probe = expMultiProbeCheck(conn, geneId, lookup, s, name, ra);
	if (probe != NULL)
	    {
	    AllocVar(col);
	    col->name = hashMustFindVal(ra, "name");
	    col->priority = atof(hashMustFindVal(ra, "priority"));
	    col->type = fullType;
	    col->settings = ra;
	    col->probe = probe;
	    col->lookupTable = cloneString(lookup);
	    slAddHead(&colList, col);
	    }
	}
    if (col == NULL)
	hashFree(&ra);
    freez(&dupe);
    }
slSort(&colList, expColumnCmpPriority);
return colList;
}

static boolean microarrayExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if microarray tables exist and have something
 * on this one. */
{
struct expColumn *col;

section->items = microarrayColumns(conn, geneId);
for (col = section->items; col != NULL; col = col->next)
    {
    if (startsWith("expRatio", col->type))
        {
	}
    else if (startsWith("expMulti", col->type))
        {
	}
    }
// return FALSE;
return section->items != NULL;
}

static void expLabelPrint(struct expColumn *col, char *subName,
	int representativeCount, int *representatives,
	char *expTable, int nameIxStart)
/* Print out labels. */
{
int skipName = 0;
char *skips;
if ((skips = hashFindVal(col->settings, "skipName")) != NULL)
    skipName = atoi(skips);
hPrintf("<TR>\n");
hgExpLabelPrint(col->name, subName, skipName, NULL,
	representativeCount, representatives, expTable, nameIxStart);
hPrintf("<TD> </TD>\n");	/* Dummy entry for labels. */
hPrintf("</TR>\n");
}

static int findEnd(int *reps, int repCount, int maxCount)
/* Find first -1 before maxCount. */
{
int i;
if (repCount <= maxCount)
    return repCount;
for (i=maxCount; i>0; --i)
    {
    if (reps[i] == -1)
        return i;
    }
return maxCount;
}

static void expRatioPrint(struct expColumn *col,
	struct sqlConnection *conn, struct sqlConnection *fConn,
	char *geneId, boolean useBlue)
/* Print out label and dots for expression ratio. */
{
float ratioMax = atof(hashMustFindVal(col->settings, "max"));
char *dupe = cloneString(col->type);
char *s = dupe;
char *repString = cloneString(hashMustFindVal(col->settings, "representatives"));
char *shortType, *lookupTable, *expTable, *ratioTable;
int representativeCount, *representatives = NULL;
int repSize, repStart, maxInRow=40;

shortType = nextWord(&s);
lookupTable = nextWord(&s);
ratioTable = nextWord(&s);
expTable = nextWord(&s);
if (expTable == NULL)
    errAbort("short type line in %s", col->name);
sqlSignedDynamicArray(repString, &representatives, &representativeCount);

for (repStart=0; repStart<representativeCount; repStart += repSize+1)
    {
    repSize = findEnd(representatives+repStart, representativeCount-repStart, 
	maxInRow);
    hPrintf("<TABLE>\n");
    expLabelPrint(col, "", repSize, representatives+repStart, expTable, repStart);
    hPrintf("<TR>");
    hgExpCellPrint(col->name, geneId, conn, col->lookupTable,
	    conn, ratioTable, repSize, representatives+repStart,
	    useBlue, FALSE, TRUE, 1.0/ratioMax);
    hPrintf("<TD>Ratios</TD>\n");
    hPrintf("</TR>\n");
    hPrintf("</TABLE>\n");
    }

freeMem(representatives);
freeMem(repString);
freeMem(dupe);
}

static void expMultiPrint(struct expColumn *col,
	struct sqlConnection *conn, struct sqlConnection *fConn,
	char *geneId, boolean useBlue)
/* Print out label and dots for expression multi. */
{
char *subType, *subName;
float ratioScale = 1.0/atof(hashMustFindVal(col->settings, "ratioMax"));
float absoluteScale = 1.0/log(atof(hashMustFindVal(col->settings, "absoluteMax")));
char *dupe = NULL, *s;
char *expTable, *ratioTable, *absTable, *repString;
int representativeCount, *representatives = NULL;
int repStart, repSize, maxInRow=40;

expMultiSubtype(col->settings, col->name, &subType, &subName);
dupe = s = cloneString(subType);
expTable = nextWord(&s);
ratioTable = nextWord(&s);
absTable = nextWord(&s);
repString = nextWord(&s);
if (repString == NULL)
    errAbort("short %s line in %s", subName, col->name);
sqlSignedDynamicArray(repString, &representatives, &representativeCount);

for (repStart = 0; repStart <representativeCount; repStart += repSize+1)
    {
    repSize = findEnd(representatives+repStart, representativeCount-repStart, 
    	maxInRow);
    hPrintf("<TABLE>\n");
    expLabelPrint(col, subName, repSize, representatives+repStart, expTable, repStart);
    hPrintf("<TR>");
    hgExpCellPrint(col->name, geneId, conn, col->lookupTable,
	    fConn, ratioTable, repSize, representatives+repStart,
	    useBlue, FALSE, TRUE, ratioScale);
    hPrintf("<TD>Ratios</TD>\n");
    hPrintf("</TR>\n");
    hPrintf("<TR>");
    hgExpCellPrint(col->name, geneId, conn, col->lookupTable,
	    fConn, absTable, repSize, representatives+repStart,
	    useBlue, TRUE, TRUE, absoluteScale);
    hPrintf("<TD>Absolute</TD>\n");
    hPrintf("</TR>");
    hPrintf("</TABLE>\n");
    }

freeMem(representatives);
freeMem(dupe);
}

static void expColumnPrint(struct expColumn *col,
	struct sqlConnection *conn, char *geneId, boolean useBlue)
/* Print out one expColumn. */
{
struct sqlConnection *fConn = sqlConnect("hgFixed");
hPrintf("<H3>%s</H3>\n", (char *)hashMustFindVal(col->settings, "longLabel"));
if (startsWith("expRatio", col->type))
    expRatioPrint(col, conn, fConn, geneId, useBlue);
else if (startsWith("expMulti", col->type))
    expMultiPrint(col, conn, fConn, geneId, useBlue);
sqlDisconnect(&fConn);
}

static void microarrayPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out microarray annotations. */
{
struct expColumn *colList = section->items, *col;
boolean useBlue = hgExpRatioUseBlue(cart, hggExpRatioColors);

hPrintf("<FORM ACTION=\"../cgi-bin/hgGene\" METHOD=GET>\n");
cartSaveSession(cart);
hPrintf("Expression ratio colors: ");
hgExpColorDropDown(cart, hggExpRatioColors);
cgiMakeButton("submit", "Submit");
hPrintf("<BR>");
for (col = colList; col != NULL; col = col->next)
    expColumnPrint(col, conn, geneId, useBlue);

/* Clean up. */
hPrintf("</FORM>\n");
expColumnFreeList(&colList);
section->items = NULL;
}

struct section *microarraySection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create microarray section. */
{
struct section *section = sectionNew(sectionRa, "microarray");
section->exists = microarrayExists;
section->print = microarrayPrint;
return section;
}

