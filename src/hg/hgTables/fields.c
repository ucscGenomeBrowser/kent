/* Put up pages for selecting and filtering on fields. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "htmshell.h"
#include "cart.h"
#include "web.h"
#include "trackDb.h"
#include "asParse.h"
#include "hgTables.h"
#include "joiner.h"

/* ------- Stuff shared by Select Fields and Filters Pages ----------*/

static boolean varOn(char *var)
/* Return TRUE if variable exists and is set. */
{
return cartVarExists(cart, var) && cartBoolean(cart, var);
}

static char *dbTableVar(char *prefix, char *db, char *table)
/* Get variable name of form prefixDb.table */
{
static char buf[128];
safef(buf, sizeof(buf), "%s%s.%s", 
	prefix, db, table);
return buf;
}

static char *dbTableFieldVar(char *prefix, char *db, char *table, char *field)
/* Get variable name of form prefixDb.table.field */
{
static char buf[128];
safef(buf, sizeof(buf), "%s%s.%s.%s", 
	prefix, db, table, field);
return buf;
}

struct dbTable
/* database/table pair. */
    {
    struct dbTable *next;
    char *db;	/* Database name. */
    char *table; /* Table name. */
    };

static struct dbTable *dbTableNew(char *db, char *table)
/* Return new dbTable struct. */
{
struct dbTable *dt;
AllocVar(dt);
dt->db = cloneString(db);
dt->table = cloneString(table);
return dt;
}

static int dbTableCmp(const void *va, const void *vb)
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


static void dbTableFree(struct dbTable **pDt)
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

static struct dbTable *extraTableList(char *prefix)
/* Get list of tables (other than the primary table)
 * where we are displaying fields. */
{
struct hashEl *varList = NULL, *var;
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

static void showLinkedTables(struct joiner *joiner, struct dbTable *inList,
	char *varPrefix, char *buttonName, char *buttonText)
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
if (outList != NULL)
    {
    webNewSection("Linked Tables");
    hTableStart();
    for (out = outList; out != NULL; out = out->next)
	{
	struct sqlConnection *conn = sqlConnect(out->db);
	struct asObject *asObj = asForTable(conn, out->table);
	char *var = dbTableVar(varPrefix, out->db, out->table);
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

    cgiMakeButton(buttonName, buttonText);
    }
}

/* ------- Select Fields Stuff ----------*/

static char *checkVarPrefix()
/* Return prefix for checkBox */
{
static char buf[128];
safef(buf, sizeof(buf), "%scheck.", hgtaFieldSelectPrefix);
return buf;
}

static char *checkVarName(char *db, char *table, char *field)
/* Get variable name for check box on given table/field. */
{
return dbTableFieldVar(checkVarPrefix(), db, table, field);
}

static char *selFieldLinkedTablePrefix()
/* Get prefix for openLinked check-boxes. */
{
static char buf[128];
safef(buf, sizeof(buf), "%s%s.", hgtaFieldSelectPrefix, "linked");
return buf;
}

#ifdef OLD
static char *selFieldLinkedTableVar(char *db, char *table)
/* Get variable name that lets us know whether to include
 * linked tables or not. */
{
return dbTableVar(selFieldLinkedTablePrefix(), db, table);
}
#endif

static char *setClearAllVar(char *setOrClearPrefix, char *db, char *table)
/* Return concatenation of a and b. */
{
static char buf[128];
safef(buf, sizeof(buf), "%s%s.%s", setOrClearPrefix, db, table);
return buf;
}

static void showTableFields(char *db, char *rootTable)
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
cgiMakeButton(hgtaDoPrintSelectedFields, "Get Fields");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf(" ");
cgiMakeButton(setClearAllVar(hgtaDoSetAllFieldPrefix,db,rootTable), 
	"Check All");
hPrintf(" ");
cgiMakeButton(setClearAllVar(hgtaDoClearAllFieldPrefix,db,rootTable), 
	"Clear All");
}

static void showLinkedFields(struct dbTable *dtList)
/* Put up a section with fields for each linked table. */
{
struct dbTable *dt;
for (dt = dtList; dt != NULL; dt = dt->next)
    {
    /* Put it up in a new section. */
    webNewSection("%s.%s fields", dt->db, dt->table);
    showTableFields(dt->db, dt->table);
    }
}

static void doBigSelectPage(char *db, char *table)
/* Put up big field selection page. Assumes html page open already*/
{
struct joiner *joiner = joinerRead("all.joiner");
struct dbTable *dtList, *dt;

htmlOpen("Select Fields from %s.%s", db, table);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=POST>\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hgtaDatabase, db);
cgiMakeHiddenVar(hgtaTable, table);

showTableFields(db, table);
dtList = extraTableList(selFieldLinkedTablePrefix());
showLinkedFields(dtList);
dt = dbTableNew(db, table);
slAddHead(&dtList, dt);
showLinkedTables(joiner, dtList, selFieldLinkedTablePrefix(),
	hgtaDoSelectFieldsMore, "Allow Selection From Checked Tables");

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
char *varPrefix = checkVarPrefix();
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

static void setCheckVarsForTable(char *dbTable, char *val)
/* Return list of check variables for this table. */
{
char prefix[128];
struct hashEl *varList, *var;
safef(prefix, sizeof(prefix), "%s%s.", checkVarPrefix(), dbTable);
varList = cartFindPrefix(cart, prefix);
for (var = varList; var != NULL; var = var->next)
    cartSetString(cart, var->name, val);
hashElFreeList(&varList);
}

void doClearAllField(char *dbTable)
/* Clear all checks by fields in db.table. */
{
setCheckVarsForTable(dbTable, "0");
doSelectFieldsMore();
}

void doSetAllField(char *dbTable)
/* Set all checks by fields in db.table. */
{
setCheckVarsForTable(dbTable, "1");
doSelectFieldsMore();
}

/* ------- Filter Page Stuff ----------*/

#define filterLinkedTablePrefix hgtaFilterPrefix "linked."
#define filterVarPrefix hgtaFilterPrefix "v."
#define filterDdVar "dd"
#define filterCmpVar "cmp"
#define filterPatternVar "pat"

static char *filterFieldVarName(char *db, char *table, char *field, char *type)
/* Return variable name for filter page. */
{
static char buf[256];
safef(buf, sizeof(buf), "%s%s.%s.%s.%s",
	filterVarPrefix, db, table, field, type);
return buf;
}

static char *filterPatternVarName(char *db, char *table, char *field)
/* Return variable name for a filter page text box. */
{
return filterFieldVarName(db, table, field, filterPatternVar);
}

boolean anyFilter()
/* Return TRUE if any filter set. */
{
return varOn(hgtaFilterOn);
}

/* Droplist menus for filtering on fields: */
char *ddOpMenu[] =
{
    "does",
    "doesn't"
};
int ddOpMenuSize = 2;

char *logOpMenu[] =
{
    "AND",
    "OR"
};
int logOpMenuSize = 2;

char *cmpOpMenu[] =
{
    "ignored",
    "in range",
    "<",
    "<=",
    "=",
    "!=",
    ">=",
    ">"
};
int cmpOpMenuSize = ArraySize(cmpOpMenu);

char *eqOpMenu[] =
{
    "ignored",
    "=",
    "!=",
};
int eqOpMenuSize = ArraySize(eqOpMenu);

void stringFilterOption(char *db, char *table, char *field, char *logOp)
/* Print out a table row with filter constraint options for a string/char. */
{
char *name;

printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", field);
name = filterFieldVarName(db, table, field, filterDdVar);
cgiMakeDropList(name, ddOpMenu, ddOpMenuSize,
		cartUsualString(cart, name, ddOpMenu[0]));
puts(" match </TD><TD>");
name = filterPatternVarName(db, table, field);
cgiMakeTextVar(name, cartUsualString(cart, name, "*"), 20);
if (logOp == NULL)
    logOp = "";
printf("</TD><TD> %s </TD></TR>\n", logOp);
}

void numericFilterOption(char *db, char *table, char *field, char *label,
	char *logOp)
/* Print out a table row with filter constraint options for a number. */
{
char *name;

printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", label);
puts(" is ");
name = filterFieldVarName(db, table, field, filterCmpVar);
cgiMakeDropList(name, cmpOpMenu, cmpOpMenuSize,
		cartUsualString(cart, name, cmpOpMenu[0]));
puts("</TD><TD>\n");
name = filterPatternVarName(db, table, field);
cgiMakeTextVar(name, cartUsualString(cart, name, ""), 20);
if (logOp == NULL)
    logOp = "";
printf("</TD><TD>%s</TD></TR>\n", logOp);
}

void eqFilterOption(char *db, char *table, char *field,
	char *fieldLabel1, char *fieldLabel2, char *logOp)
/* Print out a table row with filter constraint options for an equality 
 * comparison. */
{
char *name;

printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", fieldLabel1);
puts(" is ");
name = filterFieldVarName(db, table, field, filterCmpVar);
cgiMakeDropList(name, eqOpMenu, eqOpMenuSize,
		cgiUsualString(name, eqOpMenu[0]));
/* make a dummy pat_ CGI var for consistency with other filter options */
name = filterPatternVarName(db, table, field);
cgiMakeHiddenVar(name, "0");
puts("</TD><TD>\n");
printf("%s\n", fieldLabel2);
if (logOp == NULL)
    logOp = "";
printf("<TD>%s</TD></TR>\n", logOp);
}

static void filterControlsForTable(char *db, char *rootTable)
/* Put up filter controls for a single table. */
{
struct sqlConnection *conn = sqlConnect(db);
char *table = chromTable(conn, rootTable);
char query[256];
struct sqlResult *sr;
char **row;
boolean gotFirst = FALSE;

safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
hPrintf("<TABLE BORDER=0>\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *field = row[0];
    char *type = row[1];
    char *logic = "";

    if (!sameWord(type, "longblob"))
        {
	if (!gotFirst)
	    gotFirst = TRUE;
	else
	    logic = " AND ";
	}
    if (isSqlStringType(type))
        {
	stringFilterOption(db, rootTable, field, logic);
	}
    else
        {
	numericFilterOption(db, rootTable, field, field, logic);
	}
    }
hPrintf("</TABLE>");

freez(&table);
sqlDisconnect(&conn);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoFilterSubmit, "(Submit)");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
}

static void showLinkedFilters(struct dbTable *dtList)
/* Put up a section with filters for each linked table. */
{
struct dbTable *dt;
for (dt = dtList; dt != NULL; dt = dt->next)
    {
    /* Put it up in a new section. */
    webNewSection("%s.%s based filters", dt->db, dt->table);
    filterControlsForTable(dt->db, dt->table);
    }
}


static void doBigFilterPage(struct sqlConnection *conn, char *db, char *table)
/* Put up filter page on given db.table. */
{
struct joiner *joiner = joinerRead("all.joiner");
struct dbTable *dtList, *dt;

htmlOpen("Filter on Fields from %s.%s", db, table);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=POST>\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hgtaDatabase, db);
cgiMakeHiddenVar(hgtaTable, table);

filterControlsForTable(db, table);
dtList = extraTableList(filterLinkedTablePrefix);
showLinkedFilters(dtList);
dt = dbTableNew(db, table);
slAddHead(&dtList, dt);
showLinkedTables(joiner, dtList, filterLinkedTablePrefix,
	hgtaDoFilterMore, "Allow Filtering Using Fields in Checked Tables");

hPrintf("</FORM>\n");
htmlClose();
}


void doFilterMore(struct sqlConnection *conn)
/* Continue with Filter Page. */
{
char *db = cartString(cart, hgtaDatabase);
char *table = cartString(cart, hgtaTable);
doBigFilterPage(conn, db, table);
}

void doFilterPage(struct sqlConnection *conn)
/* Respond to filter create/edit button */
{
char *trackName = cartString(cart, hgtaTrack);
struct trackDb *track = findTrack(trackName, fullTrackList);
char *table = connectingTableForTrack(track);
doBigFilterPage(conn, database, table);
}

void doFilterSubmit(struct sqlConnection *conn)
/* Respond to submit on filters page. */
{
cartSetBoolean(cart, hgtaFilterOn, TRUE);
doMainPage(conn);
}

void doClearFilter(struct sqlConnection *conn)
/* Respond to click on clear filter. */
{
cartRemovePrefix(cart, hgtaFilterPrefix);
cartSetBoolean(cart, hgtaFilterOn, FALSE);
doMainPage(conn);
}

char *filterClause(char *db, char *table)
/* Get filter clause (something to put after 'where')
 * for table */
{
char varPrefix[128];
int varPrefixSize, fieldNameSize;
struct hashEl *varList, *var;
struct dyString *dy = NULL;
boolean needAnd = FALSE;

/* Get list of filter variables for this table.  Return
 * NULL if no filter on us. */
uglyf("filterClause %s %s\n", db, table);
if (!anyFilter())
    return NULL;
safef(varPrefix, sizeof(varPrefix), "%s%s.%s.", filterVarPrefix, db, table);
uglyf("varPrefix %s\n", varPrefix);
varPrefixSize = strlen(varPrefix);
varList = cartFindPrefix(cart, varPrefix);
if (varList == NULL)
    return NULL;

/* Create filter clause string, stepping through vars. */
dy = dyStringNew(0);
for (var = varList; var != NULL; var = var->next)
    {
    /* Parse variable name into field and type. */
    char field[64], *s, *type;
    s = var->name + varPrefixSize;
    type = strchr(s, '.');
    if (type == NULL)
        internalErr();
    fieldNameSize = type - s;
    if (fieldNameSize >= sizeof(field))
        internalErr();
    memcpy(field, s, fieldNameSize);
    field[fieldNameSize] = 0;
    type += 1;
    if (sameString(type, filterDdVar))
        {
	char *patVar = filterPatternVarName(db, table, field);
	char *pat = trimSpaces(cartOptionalString(cart, patVar));
	if (pat != NULL && pat[0] != 0 && !sameString(pat, "*"))
	    {
	    char *sqlPat = sqlLikeFromWild(pat);
	    char *ddVal = cartString(cart, var->name);
	    boolean neg = sameString(ddVal, ddOpMenu[1]);
	    if (needAnd) dyStringAppend(dy, " and ");
	    needAnd = TRUE;
	    dyStringPrintf(dy, "%s.%s.%s ", db, table, field);
	    if (sqlWildcardIn(sqlPat))
	        {
		if (neg)
		    dyStringAppend(dy, "not ");
		dyStringAppend(dy, "like ");
		}
	    else
	        {
		if (neg)
		    dyStringAppend(dy, "!= ");
		else
		    dyStringAppend(dy, "= ");
		}
	    dyStringAppendC(dy, '\'');
	    dyStringAppend(dy, sqlPat);
	    dyStringAppendC(dy, '\'');
	    freez(&sqlPat);
	    }
	}
    else if (sameString(type, filterCmpVar))
        {
	char *patVar = filterPatternVarName(db, table, field);
	char *pat = trimSpaces(cartOptionalString(cart, patVar));
	char *cmpVal = cartString(cart, var->name);
	if (pat != NULL && pat[0] != 0 && !sameString(cmpVal, cmpOpMenu[0]))
	    {
	    if (needAnd) dyStringAppend(dy, " and ");
	    needAnd = TRUE;
	    if (sameString(cmpVal, "in range"))
	        {
		char *words[2];
		int wordCount;
		char *dupe = cloneString(pat);

		wordCount = chopString(dupe, ", \t\n", words, ArraySize(words));
		if (wordCount < 2)	/* Fake short input */
		    words[1] = "2000000000";
		if (strchr(pat, '.')) /* Assume floating point */
		    {
		    double a = atof(words[0]), b = atof(words[1]);
		    dyStringPrintf(dy, "%s.%s.%s >= %f && %s.%s.%s < %f",
		    	db, table, field, a, db, table, field, b);
		    }
		else
		    {
		    int a = atoi(words[0]), b = atoi(words[1]);
		    dyStringPrintf(dy, "%s.%s.%s >= %d && %s.%s.%s < %d",
		    	db, table, field, a, db, table, field, b);
		    }
		freez(&dupe);
		}
	    else
	        {
		dyStringPrintf(dy, "%s.%s.%s %s ", db, table, field, cmpVal);
		if (strchr(pat, '.'))	/* Assume floating point. */
		    dyStringPrintf(dy, "%f", atof(pat));
		else
		    dyStringPrintf(dy, "%d", atoi(pat));
		}
	    }
	}
    uglyf("%s %s\n", field, type);
    }

/* Clean up and return */
hashElFreeList(&varList);
return dyStringCannibalize(&dy);
}

void doTest(struct sqlConnection *conn)
/* Put up a page to see what happens. */
{
char *s = NULL;
textOpen();
uglyf("Doing test!\n");
s = filterClause("hg16", "knownGene");
if (s != NULL)
    uglyf("%s\n", s);
else
    uglyf("%p\n", s);
}

