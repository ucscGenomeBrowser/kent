/* order - handle order of rows in big table. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "htmshell.h"
#include "subText.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "web.h"
#include "ra.h"
#include "hgNear.h"


#ifdef TEMPLATE
/* Here is a template for a new order.  Copy and replace
 * xyz with new order name to get started. */

boolean xyzExists(struct order *ord, struct sqlConnection *conn)
/* Return TRUE if this ordering can be computed from available data. */
{
}

void xyzCalcDistances(struct order *ord, struct sqlConnection *conn, 
    struct genePos **pGeneList, struct hash *geneHash, int maxCount)
/* Fill in distance fields in geneList. */
{
}

static void xyzMethods(struct order *ord, char *parameters)
/* Fill in xyz methods. */
{
ord->exists = xyzExists;
ord->calcDistances = xyzCalcDistances;
}

#endif /* TEMPLATE */


static boolean defaultExists(struct order *ord, struct sqlConnection *conn)
/* By default we always exist. */
{
return TRUE;
}

static void orderDefaultMethods(struct order *ord)
/* Fill in order defaults. */
{
ord->exists = defaultExists;
ord->calcDistances = NULL;
}

static boolean tableExists(struct order *ord, struct sqlConnection *conn)
/* True if table exists. */
{
return sqlTableExists(conn, ord->table);
}

static void distancesFromQuery(struct sqlConnection *conn, 
	char *query, struct hash *geneHash, int maxCount, float multiplier)
/* Fill in distance values based on query. */
{
struct sqlResult *sr;
char **row;
int count = 0;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hashEl *hel = hashLookup(geneHash, row[0]);
    while (hel != NULL)
        {
	struct genePos *gp = hel->val;
	double distance = atof(row[1]) * multiplier;
	if (distance < gp->distance)
	    gp->distance = distance;
	hel = hashLookupNext(hel);
	++count;
	}
    if (count >= maxCount)
	break;
    }
sqlFreeResult(&sr);
}

static void pairCalcDistances(struct order *ord, struct sqlConnection *conn, 
    struct genePos **pGeneList, struct hash *geneHash, int maxCount)
/* Fill in distance fields in geneList. */
{
struct dyString *query = dyStringNew(1024);
dyStringPrintf(query, "select %s,%s from %s where %s='%s'", 
	ord->keyField, ord->valField, ord->table, ord->curGeneField, 
	curGeneId->name);
distancesFromQuery(conn, query->string, geneHash, maxCount, 
	ord->distanceMultiplier);
dyStringFree(&query);
}

static void pairMethods(struct order *ord, char *parameters)
/* Fill in pair methods. */
{
char *s;
ord->table = cloneString(nextWord(&parameters));
ord->curGeneField = cloneString(nextWord(&parameters));
ord->keyField = cloneString(nextWord(&parameters));
ord->valField = cloneString(nextWord(&parameters));
s = nextWord(&parameters);
if (s == NULL)
    errAbort("Not enough parameters in type pair line for %s", ord->name);
ord->distanceMultiplier = atof(s);
ord->exists = tableExists;
ord->calcDistances = pairCalcDistances;
}

static int countStartSame(char *prefix, char *name)
/* Return how many letters of prefix match first characters of name. */
{
int i;
char c;
for (i=0; ;++i)
    {
    c = prefix[i];
    if (c == 0 || tolower(c) != tolower(name[i]))
        break;
    }
return i;
}

void nameSimilarityCalcDistances(struct order *ord, struct sqlConnection *conn, 
    struct genePos **pGeneList, struct hash *geneHash, int maxCount)
/* Fill in distance fields in geneList. */
{
struct sqlResult *sr;
char **row;
char query[512];
char curName[128];

safef(query, sizeof(query), 
	"select %s from %s where %s = '%s'", 
	ord->valField, ord->table, ord->keyField, 
	curGeneId->name);
if (!sqlQuickQuery(conn, query, curName, sizeof(curName)))
    {
    warn("Couldn't find name for %s", curGeneId->name);
    return;
    }

/* Scan table for names and sort by similarity. */
safef(query, sizeof(query), "select %s,%s from %s", 
	ord->keyField, ord->valField, ord->table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hashEl *hel = hashLookup(geneHash, row[0]);
    while (hel != NULL)
        {
	struct genePos *gp = hel->val;
	gp->distance = 1000 - countStartSame(curName,row[1]);	
	hel = hashLookupNext(hel);
	}
    }
sqlFreeResult(&sr);
}

static void nameSimilarityMethods(struct order *ord, char *parameters)
/* Fill in nameSimilarity methods. */
{
ord->table = cloneString(nextWord(&parameters));
ord->keyField = cloneString(nextWord(&parameters));
ord->valField = cloneString(nextWord(&parameters));
if( ord->valField == NULL )
    errAbort("Missing parameters in %s", ord->name );
ord->exists = tableExists;
ord->calcDistances = nameSimilarityCalcDistances;
}

static void geneDistanceCalcDistances(struct order *ord, 
	struct sqlConnection *conn, struct genePos **pGeneList, 
	struct hash *geneHash, int maxCount)
/* Fill in distance fields in geneList. */
{
struct genePos *gp, *curGp = curGeneId;
for (gp = *pGeneList; gp != NULL; gp = gp->next)
    {
    if (sameString(gp->chrom, curGp->chrom))
	gp->distance = abs(gp->start - curGp->start);
    }
}

static void geneDistanceMethods(struct order *ord, char *parameters)
/* Fill in geneDistance methods. */
{
ord->calcDistances = geneDistanceCalcDistances;
}

void abcCalcDistances(struct order *ord, struct sqlConnection *conn, 
    struct genePos **pGeneList, struct hash *geneHash, int maxCount)
/* Fill in distance fields in geneList. */
{
struct hashEl *sortList = NULL, *sortEl;
struct genePos *gp;
int count = 0;
struct sqlResult *sr;
char **row;
char query[512];

/* Make up a list keyed by name with genePos vals, and sort it */
safef(query, sizeof(query), "select %s,%s from %s", 
	ord->keyField, ord->valField, ord->table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hashEl *hel = hashLookup(geneHash, row[0]);
    while (hel != NULL)
        {
	gp = hel->val;
	AllocVar(sortEl);
	sortEl->name = cloneString(row[1]);
	sortEl->val = gp;
	slAddHead(&sortList, sortEl);
	hel = hashLookupNext(hel);
	}
    }
sqlFreeResult(&sr);
slSort(&sortList, hashElCmp);

/* Assign distance according to order in list. */
for (sortEl = sortList; sortEl != NULL; sortEl = sortEl->next)
    {
    gp = sortEl->val;
    gp->distance = ++count;
    }

/* Clean up. */
for (sortEl = sortList; sortEl != NULL; sortEl = sortEl->next)
    freeMem(sortEl->name);
slFreeList(&sortList);
}

static void abcMethods(struct order *ord, char *parameters)
/* Fill in abc methods. */
{
ord->table = cloneString(nextWord(&parameters));
ord->keyField = cloneString(nextWord(&parameters));
ord->valField = cloneString(nextWord(&parameters));
ord->exists = tableExists;
ord->calcDistances = abcCalcDistances;
}

void genomePosCalcDistances(struct order *ord, struct sqlConnection *conn, 
    struct genePos **pGeneList, struct hash *geneHash, int maxCount)
/* Fill in distance fields in geneList. */
{
int count = 0;
struct genePos *gp;
slSort(pGeneList, genePosCmpPos);
for (gp = *pGeneList; gp != NULL; gp = gp->next)
    gp->distance = ++count;
}

static void genomePosMethods(struct order *ord, char *parameters)
/* Fill in genomePos methods. */
{
ord->calcDistances = genomePosCalcDistances;
}


static void orderSetMethods(struct order *ord)
/* Set up methods for this ordering. */
{
char *dupe = cloneString(ord->type);	
char *s = dupe;
char *type = nextWord(&s);

orderDefaultMethods(ord);
if (sameString(type, "pair"))
     pairMethods(ord, s);
else if (sameString(type, "nameSimilarity"))
     nameSimilarityMethods(ord, s);
else if (sameString(type, "geneDistance"))
     geneDistanceMethods(ord, s);
else if (sameString(type, "association"))
     associationSimilarityMethods(ord, s);
else if (sameString(type, "abc"))
     abcMethods(ord, s);
else if (sameString(type, "genomePos"))
     genomePosMethods(ord, s);
/* Fill in abc methods. */
else
     errAbort("Unrecognized type %s in ordering %s", ord->type, ord->name);
freez(&dupe);
}

static int orderCmpPriority(const void *va, const void *vb)
/* Compare to sort orders based on priority. */
{
const struct order *a = *((struct order **)va);
const struct order *b = *((struct order **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
    return -1;
else if (dif > 0)
    return 1;
else
    return 0;
}


struct order *orderGetAll(struct sqlConnection *conn)
/* Return list of row orders available. */
{
char *raName = "orderDb.ra";
struct hash *ra, *raList = readRa(raName);
struct order *ord, *ordList = NULL;

for (ra = raList; ra != NULL; ra = ra->next)
    {
    AllocVar(ord);
    ord->name = mustFindInRaHash(raName, ra, "name");
    ord->shortLabel = mustFindInRaHash(raName, ra, "shortLabel");
    ord->longLabel = mustFindInRaHash(raName, ra, "longLabel");
    ord->type = mustFindInRaHash(raName, ra, "type");
    ord->priority = atof(mustFindInRaHash(raName, ra, "priority"));
    ord->settings = ra;
    orderSetMethods(ord);
    if (ord->exists(ord, conn))
	slAddHead(&ordList, ord);
    }
slSort(&ordList, orderCmpPriority);
return ordList;
}

