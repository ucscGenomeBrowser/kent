/* Put up pages for selecting and filtering on fields. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "web.h"
#include "trackDb.h"
#include "asParse.h"
#include "hgTables.h"
#include "joiner.h"

static char *fsVarName(char *type, char *db, char *table, char *field)
/* Get variable name that encodes type, table and field. */
{
static char buf[128];
safef(buf, sizeof(buf), "%s.%s.%s.%s.%s", 
	hgtaFieldSelectPrefix, type, db, table, field);
return buf;
}

static char *checkVarName(char *db, char *table, char *field)
/* Get variable name for check box on given table/field. */
{
return fsVarName("check", db, table, field);
}

static char *typePrefix(char *type)
/* Return prefix for type */
{
static char buf[128];
safef(buf, sizeof(buf), "%s.%s.", hgtaFieldSelectPrefix, type);
return buf;
}

#define linkedSym "linked"

static char *linkedPrefix()
/* Get prefix for openLinked check-boxes. */
{
static char buf[128];
safef(buf, sizeof(buf), "%s.%s.", hgtaFieldSelectPrefix, linkedSym);
return buf;
}

static char *linkedTableVar(char *db, char *table)
/* Get variable name that lets us know whether to include
 * linked tables or not. */
{
static char buf[128];
safef(buf, sizeof(buf), "%s.%s.%s.%s", 
	hgtaFieldSelectPrefix, linkedSym, db, table);
return buf;
}

static boolean varOn(char *var)
/* Return TRUE if variable exists and is set. */
{
return cartVarExists(cart, var) && cartBoolean(cart, var);
}

struct dbTable
/* database/table pair. */
    {
    struct dbTable *next;
    char *db;	/* Database name. */
    char *table; /* Table name. */
    };

struct dbTable *dbTableNew(char *db, char *table)
/* Return new dbTable struct. */
{
struct dbTable *dt;
AllocVar(dt);
dt->db = cloneString(db);
dt->table = cloneString(table);
return dt;
}

int dbTableCmp(const void *va, const void *vb)
/* Compare two dbTables. */
{
const struct dbTable *a = *((struct dbTable **)va);
const struct dbTable *b = *((struct dbTable **)vb);
int diff;
diff = strcmp(a->db, b->db);
if (diff == 0)
    diff = strcmp(a->table, b->table);
return diff;
}


void dbTableFree(struct dbTable **pDt)
/* Free up dbTable struct. */
{
struct dbTable *dt = *pDt;
if (dt != NULL)
    {
    freeMem(dt->db);
    freeMem(dt->table);
    freez(pDt);
    }
}

static void showTableFields(struct joiner *joiner, char *db, char *rootTable)
/* Put up a little html table with a check box, name, and hopefully
 * a description for each field in SQL rootTable. */
{
struct sqlConnection *conn = sqlConnect(db);
char *table = chromTable(conn, rootTable);
char query[256];
struct sqlResult *sr;
char **row;
struct asObject *asObj = asForTable(conn, rootTable);

safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);

hTableStart();
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *field = row[0];
    char *var = checkVarName(db, rootTable, field);
    struct asColumn *asCol;
    hPrintf("<TR>");
    hPrintf("<TD>");
    cgiMakeCheckBox(var, varOn(var));
    hPrintf("</TD>");
    hPrintf("<TD>");
    hPrintf(" %s<BR>\n", field);
    hPrintf("</TD>");
    if (asObj != NULL)
	{
	asCol = asColumnFind(asObj, field);
	if (asCol != NULL)
	    hPrintf("<TD>%s</TD>", asCol->comment);
	else
	    hPrintf("<TD>&nbsp;</TD>");
	}
    hPrintf("</TR>");
    }
hTableEnd();

freez(&table);
sqlDisconnect(&conn);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoPrintSelectedFields, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
}

static void showLinkedTables(struct joiner *joiner, struct dbTable *inList)
/* Print section with list of linked tables and check boxes to turn them
 * on. */
{
struct slName *table;
struct dbTable *outList = NULL, *out, *in;
char dtName[256];
struct hash *uniqHash = newHash(0);
struct hash *inHash = newHash(8);

/* Build up list of tables we link to in outList. */
for (in = inList; in != NULL; in = in->next)
    {
    struct sqlConnection *conn = sqlConnect(in->db);
    struct joinerPair *jpList, *jp;
    
    /* Keep track of tables in inList. */
    safef(dtName, sizeof(dtName), "%s.%s", inList->db, inList->table);
    hashAdd(inHash, dtName, NULL);

    /* First table in input is not allowed in output. */
    if (in == inList)	
        hashAdd(uniqHash, dtName, NULL);

    /* Scan through joining information and add tables,
     * avoiding duplicate additions. */
    jpList = joinerRelate(joiner, in->db, in->table);
    for (jp = jpList; jp != NULL; jp = jp->next)
        {
	safef(dtName, sizeof(dtName), "%s.%s", 
		jp->b->database, jp->b->table);
	if (!hashLookup(uniqHash, dtName))
	    {
	    hashAdd(uniqHash, dtName, NULL);
	    out = dbTableNew(jp->b->database, jp->b->table);
	    slAddHead(&outList, out);
	    }
	}
    joinerPairFreeList(&jpList);
    sqlDisconnect(&conn);
    }
slSort(&outList, dbTableCmp);

/* Print html. */
webNewSection("Linked Tables");
hTableStart();
for (out = outList; out != NULL; out = out->next)
    {
    struct sqlConnection *conn = sqlConnect(out->db);
    struct asObject *asObj = asForTable(conn, out->table);
    char *var = linkedTableVar(out->db, out->table);
    hPrintf("<TR>");
    hPrintf("<TD>");
    cgiMakeCheckBox(var, varOn(var));
    hPrintf("</TD>");
    hPrintf("<TD>%s</TD>", out->db);
    hPrintf("<TD>%s</TD>", out->table);
    hPrintf("<TD>");
    if (asObj != NULL)
        hPrintf("%s", asObj->comment);
    else
        hPrintf("&nbsp;");
    hPrintf("</TD>");
    hPrintf("</TR>");
    sqlDisconnect(&conn);
    }
hTableEnd();
hPrintf("<BR>");

cgiMakeButton(hgtaDoSelectFieldsMore, "Allow Selection From Checked Tables");
}

struct dbTable *extraTableList()
/* Get list of tables (other than the primary table)
 * where we are displaying fields. */
{
struct hashEl *varList = NULL, *var;
char *prefix = linkedPrefix();
int prefixSize = strlen(prefix);
struct dbTable *dtList = NULL, *dt;

/* Build up list of tables to show by looking at
 * variables with right prefix in cart. */
varList = cartFindPrefix(cart, prefix);
for (var = varList; var != NULL; var = var->next)
    {
    if (cartBoolean(cart, var->name))
	{
	/* From variable name parse out database and table. */
	char *dbTab = cloneString(var->name + prefixSize);
	char *db = dbTab;
	char *table = strchr(db, '.');
	if (table == NULL)
	    internalErr();
	*table++ = 0;

	dt = dbTableNew(db, table);
	slAddHead(&dtList, dt);
	freez(&dbTab);
	}
    }
slSort(&dtList, dbTableCmp);
return dtList;
}

void showLinkedFields(struct joiner *joiner, struct dbTable *dtList)
/* Put up a section with fields for each linked table. */
{
struct dbTable *dt;
for (dt = dtList; dt != NULL; dt = dt->next)
    {
    /* Put it up in a new section. */
    webNewSection("%s.%s fields", dt->db, dt->table);
    showTableFields(joiner, dt->db, dt->table);
    }
}

void doBigSelectPage(char *db, char *table)
/* Put up big field selection page. Assumes html page open already*/
{
struct joiner *joiner = joinerRead("all.joiner");
struct dbTable *dtList, *dt;

htmlOpen("Select Fields from %s.%s", db, table);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=POST>\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hgtaDatabase, database);
cgiMakeHiddenVar(hgtaTable, table);

showTableFields(joiner, database, table);
dtList = extraTableList();
showLinkedFields(joiner, dtList);
dt = dbTableNew(database, table);
slAddHead(&dtList, dt);
showLinkedTables(joiner, dtList);

/* clean up. */
hPrintf("</FORM>");
htmlClose();
joinerFree(&joiner);
}

void doSelectFieldsMore()
/* Do select fields page (generally as a continuation. */
{
char *db = cartString(cart, hgtaDatabase);
char *table = cartString(cart, hgtaTable);
doBigSelectPage(db, table);
}

void doOutSelectedFields(struct trackDb *track, struct sqlConnection *conn)
/* Put up select fields (for tab-separated output) page. */
{
char *table = connectingTableForTrack(track);
cartRemovePrefix(cart, hgtaFieldSelectPrefix);
doBigSelectPage(database, table);
}

void doPrintSelectedFields()
/* Actually produce selected field output as text stream. */
{
char *db = cartString(cart, hgtaDatabase);
char *table = cartString(cart, hgtaTable);
char *varPrefix = typePrefix("check");
int varPrefixSize = strlen(varPrefix);
struct hashEl *varList = NULL, *var;
struct slName *fieldList = NULL, *field;

textOpen();

/* Gather together field list from cart. */
varList = cartFindPrefix(cart, varPrefix);
for (var = varList; var != NULL; var = var->next)
    {
    if (!sameString(var->val, "0"))
	{
	field = slNameNew(var->name + varPrefixSize);
	slAddHead(&fieldList, field);
	}
    }
if (fieldList == NULL)
    errAbort("Please go back and select at least one field");
slReverse(&fieldList);

/* Do output. */
tabOutSelectedFields(db, table, fieldList);

/* Clean up. */
slFreeList(&fieldList);
hashElFreeList(&varList);
}

