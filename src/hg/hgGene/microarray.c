/* microarray - Gene Ontology annotations. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hdb.h"
#include "hgExp.h"
#include "hgGene.h"


struct expColumn
/* An expression column. */
    {
    struct expColumn *next;
    char *name;		/* Symbolic name. */
    float priority;	/* Sort order priority. */
    char *type;		/* What type this is. */
    struct hash *settings;	/* Rest of stuff. */
    char *probe;		/* Name of probe if any. */
    };

static void expColumnFree(struct expColumn **pEl)
/* Free up expColumn. */
{
struct expColumn *el = *pEl;
if (el != NULL)
    {
    hashFree(&el->settings);
    freeMem(el->probe);
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

    if (sameString("expRatio", type) || sameString("expMulti", type))
        {
	char *lookup = nextWord(&s);
	char *probe;
	if (lookup == NULL)
	    errAbort("short type line");
	probe = expProbe(conn, lookup, geneId);
	if (probe != NULL)
	    {
	    AllocVar(col);
	    col->name = hashMustFindVal(ra, "name");
	    col->priority = atof(hashMustFindVal(ra, "priority"));
	    col->type = fullType;
	    col->settings = ra;
	    col->probe = probe;
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
section->items = microarrayColumns(conn, geneId);
return section->items != NULL;
}

static void expRatioPrint(struct expColumn *col,
	struct sqlConnection *conn, struct sqlConnection *fConn,
	char *geneId)
/* Print out label and dots for expression ratio. */
{
char *dupe = cloneString(col->type);

freeMem(dupe);
}

static void expLabelPrint(struct expColumn *col, char *subName,
	int representativeCount, int *representatives,
	char *expTable)
/* Print out labels. */
{
int skipName = 0;
char *skips;
if ((skips = hashFindVal(col->settings, "skipName")) != NULL)
    skipName = atoi(skips);
hPrintf("<TR>\n");
hgExpLabelPrint(col->name, subName, skipName, NULL,
	representativeCount, representatives, expTable);
hPrintf("</TR>\n");
}

static void expMultiPrint(struct expColumn *col,
	struct sqlConnection *conn, struct sqlConnection *fConn,
	char *geneId)
/* Print out label and dots for expression multi. */
{
char *subName = "median";
char *subType = hashFindVal(col->settings, subName);
char *dupe = NULL, *s;
char *expTable, *ratioTable, *absTable, *repString;
int representativeCount, *representatives = NULL;

if (subType == NULL)
    {
    subName = "all";
    subType = hashFindVal(col->settings, subName);
    }
if (subType == NULL)
    {
    errAbort("Couldn't find all or median subtype in %s", col->name);
    }
dupe = s = cloneString(subType);
expTable = nextWord(&s);
ratioTable = nextWord(&s);
absTable = nextWord(&s);
repString = nextWord(&s);
if (repString == NULL)
    errAbort("short %s line in %s", subName, col->name);
sqlSignedDynamicArray(repString, &representatives, &representativeCount);

expLabelPrint(col, subName, representativeCount, representatives, expTable);

freeMem(representatives);
freeMem(dupe);
}


static void expColumnPrint(struct expColumn *col,
	struct sqlConnection *conn, char *geneId)
/* Print out one expColumn. */
{
struct sqlConnection *fConn = sqlConnect("hgFixed");
hPrintf("<H3>%s</H3>\n", hashMustFindVal(col->settings, "longLabel"));
hPrintf("<TABLE>\n");
if (startsWith("expRatio", col->type))
    expRatioPrint(col, conn, fConn, geneId);
else if (startsWith("expMulti", col->type))
    expMultiPrint(col, conn, fConn, geneId);
hPrintf("</TABLE>\n");
sqlDisconnect(&fConn);
}

static void microarrayPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out microarray annotations. */
{
struct expColumn *colList = section->items, *col;

uglyf("%d columns<BR>\n", slCount(section->items));
for (col = colList; col != NULL; col = col->next)
    expColumnPrint(col, conn, geneId);

/* Clean up. */
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

