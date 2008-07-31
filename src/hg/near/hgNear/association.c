/* association - handle association type columns.  
 * An association is a two column table (or query)
 * with one column corresponding to a key, and the other
 * to a value.  It is very similar to a lookup type.
 * However where there is only one value per key in a lookup
 * there can be multiple values per keyin an association.
 *
 * Originally the pfam column was an association.  Unfortunately
 * mySQL was just too slow with the join that it required. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "hdb.h"
#include "hgNear.h"

boolean associationExists(struct column *col, struct sqlConnection *conn)
/* This returns true if all tables this depends on exists. */
{
char *dupe = cloneString(col->tablesUsed);
char *s = dupe;
char *table;
boolean ok = TRUE;

while ((table = nextWord(&s)) != NULL)
    {
    if (!sqlTableExists(conn, table))
        {
	ok = FALSE;
	break;
	}
    }
freez(&dupe);
return ok;
}

struct assocList
/* A gene and a list of strings associated with it. */
    {
    struct slRef *list;	/* References to strings. */
    };

struct assocGroup
/* Structure to help group an association table into
 * something a little easier to handle. */
    {
    struct hash *listHash; /* assocList valued hash keyed by keyField. */
    struct hash *valStringHash; /* String values to save some space. */
    struct lm *lm;	   /* Local memory pool for speed. Parasites off
                            * of valStringHash. */
    };

void assocGroupFree(struct assocGroup **pAg)
/* Free up resources of assocGroup. */
{
struct assocGroup *ag = *pAg;
if (ag != NULL)
    {
    hashFree(&ag->listHash);
    hashFree(&ag->valStringHash);
    freez(pAg);
    }
}

struct assocGroup *assocGroupNew(int hashSize)
/* Create new assocGroup. HashSize should be roughly
 * the log base 2 of the number of items. */
{
struct assocGroup *ag;
AllocVar(ag);
ag->listHash = hashNew(hashSize);
ag->valStringHash = hashNew(hashSize);
ag->lm = ag->valStringHash->lm;
return ag;
}

void assocGroupAdd(struct assocGroup *ag, char *key, char *val)
/* Add key/val pair to assocGroup. */
{
struct assocList *al = hashFindVal(ag->listHash, key);
struct slRef *ref;
if (al == NULL)
    {
    lmAllocVar(ag->lm, al);
    hashAdd(ag->listHash, key, al);
    }
val = hashStoreName(ag->valStringHash, val);
lmAllocVar(ag->lm, ref);
ref->val = val;
slAddHead(&al->list, ref);
}

static boolean wildAnyRefMatch(char *wild, struct slRef *refList)
/* Return true if any string-valued reference on list matches
 * wildcard. */
{
struct slRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    if (wildMatch(wild, ref->val))
        return TRUE;
    }
return FALSE;
}

static boolean wildMatchRefs(struct slName *wildList, struct slRef *refList, 
	boolean orLogic)
/* If using orLogic return true if any element of refList
 * matches any element of wildList.
 * If using andLogic return true all elements of wildList have
 * at least one match in refList. */
{
struct slName *wildEl;

if (orLogic)
    {
    for (wildEl = wildList; wildEl != NULL; wildEl = wildEl->next)
	{
	if (wildAnyRefMatch(wildEl->name, refList))
	    return TRUE;
	}
    return FALSE;
    }
else
    {
    for (wildEl = wildList; wildEl != NULL; wildEl = wildEl->next)
	{
	if (!wildAnyRefMatch(wildEl->name, refList))
	    return FALSE;
	}
    return TRUE;
    }
}

static struct genePos *wildAssociationFilter(
	struct slName *wildList, boolean orLogic, 
	struct column *col, struct sqlConnection *conn, struct genePos *list)
/* Handle relatively slow filtering when there is a wildcard present. */
{
struct assocGroup *ag = assocGroupNew(16);
struct genePos *gp;
struct hash *passHash = newHash(16); /* Hash of items passing filter. */
int assocCount = 0;
struct sqlResult *sr;
char **row;

/* Build up associations. */
sr = sqlGetResult(conn, col->queryFull);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++assocCount;
    assocGroupAdd(ag, row[0],row[1]);
    }
sqlFreeResult(&sr);

/* Look for matching associations and put them on newList. */
for (gp = list; gp != NULL; gp = gp->next)
    {
    char *key = (col->protKey 
	? (kgVersion == KG_III ? lookupProtein(conn, gp->name) : gp->protein)
	: gp->name);
    struct assocList *al = hashFindVal(ag->listHash, key);
    if (al != NULL)
	{
	if (wildList == NULL || wildMatchRefs(wildList, al->list, orLogic))
	    hashAdd(passHash, gp->name, gp);
	}
    }
list = weedUnlessInHash(list, passHash);
hashFree(&passHash);
assocGroupFree(&ag);
return list;
}

static struct genePos *tameAssociationFilter(
	struct slName *termList, boolean orLogic, 
	struct column *col, struct sqlConnection *conn, struct genePos *list)
/* Handle filtering when there are no wildcards present. */
{
struct sqlResult *sr;
char **row;
struct slName *term;
struct hash *passHash = newHash(17);
struct hash *protHash = NULL;
struct hash *prevHash = NULL;
struct genePos *gp;
int protCount = 0, termCount = 0, matchRow = 0, keyRow = 0;

/* Make up protein-keyed hash if need be. */
if (col->protKey)
    {
    protHash = newHash(17);
    for (gp = list; gp != NULL; gp = gp->next)
	{
        hashAdd(protHash, gp->protein, gp->name);
	++protCount;
	}
    }
for (term = termList; term != NULL; term = term->next)
    {
    char query[1024];
    safef(query, sizeof(query), col->invQueryOne, term->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *key = row[0];
	++matchRow;
	if (protHash != NULL)
	    key = hashFindVal(protHash, key);
	if (key != NULL)
	    {
	    ++keyRow;
	    if (prevHash == NULL || hashLookup(prevHash, key) != NULL)
		{
		hashStore(passHash, key);
		}
	    }
	}
    if (!orLogic)
	{
	hashFree(&prevHash);
	if (term->next != NULL)
	    {
	    prevHash = passHash;
	    passHash = newHash(17);
	    }
	}
    sqlFreeResult(&sr);
    ++termCount;
    }
list = weedUnlessInHash(list, passHash);
hashFree(&prevHash);
freeHash(&protHash);
freeHash(&passHash);
return list;
}

static struct genePos *associationAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on position. */
{
char *terms = advFilterVal(col, "terms");
if (terms != NULL)
    {
    boolean orLogic = advFilterOrLogic(col, "logic", TRUE);
    struct slName *termList = stringToSlNames(terms);

    if (anyWild(terms))
	list = wildAssociationFilter(termList, orLogic, col, conn, list);
    else
	list = tameAssociationFilter(termList, orLogic, col, conn, list);

    }
return list;
}

char *associationCellVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Make comma separated list of matches to association table. */
{
char query[1024];
struct sqlResult *sr;
char **row;
boolean gotOne = FALSE;
struct dyString *dy = newDyString(512);
char *result = NULL;
char *key = (col->protKey 
    ? (kgVersion == KG_III ? lookupProtein(conn, gp->name) : gp->protein)
    : gp->name);
struct hash *uniqHash = NULL;

if (col->weedDupes) uniqHash = newHash(8);
safef(query, sizeof(query), col->queryOne, key);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *s = row[0];
    boolean needQuote;
    if (uniqHash != NULL)
        {
	if (hashLookup(uniqHash, s))
	    continue;
	else
	    hashAdd(uniqHash, s, NULL);
	}
    needQuote = hasWhiteSpace(s);
    if (needQuote)
    	dyStringAppendC(dy, '\'');
    dyStringAppend(dy, s);
    if (needQuote)
    	dyStringAppendC(dy, '\'');
    dyStringAppend(dy, ",");
    gotOne = TRUE;
    }
sqlFreeResult(&sr);
if (gotOne)
    result = cloneString(dy->string);
dyStringFree(&dy);
freeHash(&uniqHash);
return result;
}

static void associationCellPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Print cell in association table. */
{
char query[1024];
struct sqlResult *sr;
char **row;
boolean gotOne = FALSE;
char *key = (col->protKey 
    ? (kgVersion == KG_III ? lookupProtein(conn, gp->name) : gp->protein)
    : gp->name);
struct hash *uniqHash = NULL;

if (col->weedDupes) uniqHash = newHash(8);
hPrintf("<TD>");
safef(query, sizeof(query), col->queryOne, key);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *s = row[0];
    boolean needQuote;
    if (uniqHash != NULL)
        {
	if (hashLookup(uniqHash, s))
	    continue;
	else
	    hashAdd(uniqHash, s, NULL);
	}
    needQuote = hasWhiteSpace(s);
    if (!gotOne)
        gotOne = TRUE;
    else
	hPrintf("&nbsp;");
    if (needQuote)
        hPrintf("'");
    if (col->itemUrl)
	{
	hPrintf("<A HREF=\"");
	hPrintf(col->itemUrl, row[1]);
	hPrintf("\" TARGET=_blank>");
	}
    hPrintNonBreak(s);
    if (col->itemUrl)
        {
	hPrintf("</A>");
	}
    if (needQuote)
        hPrintf("'");
    }
sqlFreeResult(&sr);
if (!gotOne)
    {
    hPrintf("n/a");
    }
hPrintf("</TD>");
freeHash(&uniqHash);
}

static void associationFilterControls(struct column *col, 
	struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
hPrintf("Enclose term in single quotes if it "
        "contains multiple words.  You may include "
	"* and ? wildcards.<BR>\n");
hPrintf("Term(s): ");
advFilterRemakeTextVar(col, "terms", 35);
hPrintf(" Include if ");
advFilterAnyAllMenu(col, "logic", FALSE);
hPrintf("terms match");
}

struct searchResult *associationSimpleSearch(struct column *col, 
    struct sqlConnection *conn, char *search)
/* Search lookup type column. */
{
struct dyString *query = dyStringNew(512);
struct sqlResult *sr;
char **row;
struct searchResult *resList = NULL, *res;
struct sqlConnection *conn2 = hAllocConn(database);

dyStringPrintf(query, col->invQueryOne, search);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = NULL;
    if (col->protKey)
	name = protToGeneId(conn2, row[0]);
    else
	name = cloneString(row[0]);
    if (name != NULL)
	{
	AllocVar(res);
	res->gp.name = name;
	slAddHead(&resList, res);
	}
    }

/* Clean up and go home. */
sqlFreeResult(&sr);
hFreeConn(&conn2);
dyStringFree(&query);
slReverse(&resList);
return resList;
}

void setupColumnAssociation(struct column *col, char *parameters)
/* Set up a column that looks for an association table 
 * keyed by the geneId. */
{
if ((col->queryFull = columnSetting(col, "queryFull", NULL)) == NULL)
    errAbort("Missing required queryFull field in column %s", col->name);
if ((col->queryOne = columnSetting(col, "queryOne", NULL)) == NULL)
    errAbort("Missing required queryOne field in column %s", col->name);
if ((col->invQueryOne = columnSetting(col, "invQueryOne", NULL)) == NULL)
    errAbort("Missing required invQueryOne field in column %s", col->name);
col->protKey = (columnSetting(col, "protKey", NULL) != NULL);
col->weedDupes = (columnSetting(col, "weedDupes", NULL) != NULL);
col->tablesUsed = cloneString(parameters);
col->exists = associationExists;
col->filterControls = associationFilterControls;
col->advFilter = associationAdvFilter;
col->cellVal = associationCellVal;
col->cellPrint = associationCellPrint;
if (columnSetting(col, "search", NULL))
    col->simpleSearch = associationSimpleSearch;
}

/* ----- Ordering by association. ----- */

static boolean associationOrderExists(struct order *ord, 
	struct sqlConnection *conn)
/* This returns true if needed tables exist. */
{
return sqlTablesExist(conn, ord->tables);
}


static void associationCalcDistances(struct order *ord, 
	struct sqlConnection *conn, /* connection to main database. */
	struct genePos **pGeneList, struct hash *geneHash, int maxCount)
/* Fill in distance fields in geneList. */
{
struct sqlResult *sr;
char **row;
struct hash *curTerms = newHash(8);
struct hash *protHash = NULL;
struct hash *lookupHash = geneHash;
char query[512];
struct genePos *gp;
char *geneId = curGeneId->name;

if (ord->protKey)
    {
    /* Build up hash of genes keyed by protein names. (The geneHash
     * passed in is keyed by the mrna name. */
    protHash = newHash(17);
    for (gp = *pGeneList; gp != NULL; gp = gp->next)
	{
	char *id = (ord->protKey 
	    ? (kgVersion == KG_III ? lookupProtein(conn, gp->name) : gp->protein)
	    : gp->name);
	hashAdd(protHash, id, gp);
	}

    /* Also switch current gene id and lookup hash to protein. */
    geneId = curGeneId->protein;
    lookupHash = protHash;
    }


/* Build up hash full of all go IDs associated with gene. */
if (geneId != NULL)
    {
    safef(query, sizeof(query), ord->queryOne, geneId);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	hashAdd(curTerms, row[0], NULL);
	}
    sqlFreeResult(&sr);
    }

/* Stream through association table counting matches. */
sr = sqlGetResult(conn, ord->queryAll);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (hashLookup(curTerms, row[1]))
	{
	struct hashEl *hel = hashLookup(lookupHash, row[0]);
	while (hel != NULL)
	    {
	    gp = hel->val;
	    gp->count += 1;
	    hel = hashLookupNext(hel);
	    }
	}
    }
sqlFreeResult(&sr);

/* Go through list translating non-zero counts to distances. */
for (gp = *pGeneList; gp != NULL; gp = gp->next)
    {
    if (gp->count > 0)
        {
	gp->distance = 1.0/gp->count;
	gp->count = 0;
	}
    if (sameString(gp->name, curGeneId->name))	/* Force self to top of list. */
        gp->distance = 0;
    }

hashFree(&protHash);
hashFree(&curTerms);
}

void associationSimilarityMethods(struct order *ord, char *parameters)
/* Fill in associationSimilarity methods. */
{
ord->exists = associationOrderExists;
ord->calcDistances = associationCalcDistances;
ord->tables = cloneString(parameters);
ord->protKey = orderSettingExists(ord, "protKey");
ord->queryOne = orderRequiredSetting(ord, "queryOne");
ord->queryAll = orderRequiredSetting(ord, "queryAll");
}

