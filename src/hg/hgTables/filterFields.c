/* Put up pages for selecting and filtering on fields. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "sqlSanity.h"
#include "htmshell.h"
#include "cart.h"
#include "cartTrackDb.h"
#include "jsHelper.h"
#include "web.h"
#include "trackDb.h"
#include "asParse.h"
#include "kxTok.h"
#include "customTrack.h"
#include "joiner.h"
#include "hgTables.h"
#include "bedCart.h"
#include "wiggle.h"
#include "wikiTrack.h"
#include "makeItemsItem.h"
#include "bedDetail.h"
#include "pgSnp.h"
#include "samAlignment.h"
#include "trackHub.h"


/* ------- Stuff shared by Select Fields and Filters Pages ----------*/

boolean varOn(char *var)
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

#if defined(NOT_USED)
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
#endif

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
if (varList == NULL && curTrack != NULL)
    {
    char *defaultLinkedTables = trackDbSetting(curTrack, "defaultLinkedTables");
    if (defaultLinkedTables != NULL)
	{
	struct slName *t, *tables = slNameListFromString(defaultLinkedTables, ',');
	for (t = tables;  t != NULL;  t = t->next)
	    {
	    char varName[1024];
	    safef(varName, sizeof(varName), "%s%s.%s", prefix, database, t->name);
	    cartSetBoolean(cart, varName, TRUE);
	    dt = dbTableNew(database, t->name);
	    slAddHead(&dtList, dt);
	    }
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
struct dbTable *outList = NULL, *out, *in;
char dtName[256];
struct hash *uniqHash = newHash(0);
struct hash *inHash = newHash(8);

/* Build up list of tables we link to in outList. */
for (in = inList; in != NULL; in = in->next)
    {
    struct sqlConnection *conn = NULL;
    if (!trackHubDatabase(database))
	conn = hAllocConn(in->db);
    struct joinerPair *jpList, *jp;

    /* Keep track of tables in inList. */
    safef(dtName, sizeof(dtName), "%s.%s", inList->db, inList->table);
    hashAdd(inHash, dtName, NULL);

    /* First table in input is not allowed in output. */
    if (in == inList)
        hashAdd(uniqHash, dtName, NULL);

    /* Scan through joining information and add tables,
     * avoiding duplicate additions. */
    jpList = joinerRelate(joiner, in->db, in->table, database);
    for (jp = jpList; jp != NULL; jp = jp->next)
        {
	safef(dtName, sizeof(dtName), "%s.%s",
		jp->b->database, jp->b->table);
	if (!hashLookup(uniqHash, dtName) &&
	   !cartTrackDbIsAccessDenied(jp->b->database, jp->b->table))
	    {
	    hashAdd(uniqHash, dtName, NULL);
	    out = dbTableNew(jp->b->database, jp->b->table);
	    slAddHead(&outList, out);
	    }
	}
    joinerPairFreeList(&jpList);
    hFreeConn(&conn);
    }
slSort(&outList, dbTableCmp);

/* Print html. */
if (outList != NULL)
    {
    webNewSection("Linked Tables");
    hTableStart();
    for (out = outList; out != NULL; out = out->next)
	{
	struct sqlConnection *conn = hAllocConn(out->db);
	struct asObject *asObj = asForTable(conn, out->table);
	char *var = dbTableVar(varPrefix, out->db, out->table);
        boolean disabled = isNoGenomeDisabled(out->db, out->table);
        char *classString = disabled ? NO_GENOME_CLASS : "";
	hPrintf("<TR>");
	hPrintf("<TD>");
        cgiMakeCheckBoxEnabled(var, varOn(var) && !disabled, !disabled);
	hPrintf("</TD>");
	hPrintf("<TD><span%s>%s</span></TD>", classString, out->db);
	hPrintf("<TD><span%s>%s</span></TD>", classString, out->table);
	hPrintf("<TD>");
	if (asObj != NULL)
	    hPrintf("<span%s>%s</span>", classString, asObj->comment);
	else
	    hPrintf("&nbsp;");
	hPrintf("</TD>");
	hPrintf("</TR>");
	hFreeConn(&conn);
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

static char *setClearAllVar(char *setOrClearPrefix, char *db, char *table)
/* Return concatenation of a and b. */
{
static char buf[128];
safef(buf, sizeof(buf), "%s%s.%s", setOrClearPrefix, db, table);
return buf;
}

static void showTableButtons(char *db, char *table, boolean withGetButton)
/* Put up the last buttons in a showTable section. */
{
hPrintf("<BR>\n");
if (withGetButton)
    {
    if (doGalaxy()) /* need form fields here and Galaxy so add step to Galaxy */
        cgiMakeButton(hgtaDoGalaxySelectedFields, "done with selections");
    else
        cgiMakeButton(hgtaDoPrintSelectedFields, "get output");
    hPrintf(" ");
    cgiMakeButton(hgtaDoMainPage, "cancel");
    hPrintf(" ");
    }
jsInit();
cgiMakeOnClickSubmitButton(jsSetVerticalPosition("mainForm"),
			   setClearAllVar(hgtaDoSetAllFieldPrefix,db,table),
			   "check all");
hPrintf(" ");
cgiMakeOnClickSubmitButton(jsSetVerticalPosition("mainForm"),
			   setClearAllVar(hgtaDoClearAllFieldPrefix,db,table),
			   "clear all");
cgiDown(0.7); // Extra spacing below the buttons
}

static void showTableFieldsOnList(char *db, char *rootTable,
                                  struct asObject *asObj, struct slName *fieldList,
                                  boolean showItemRgb, boolean withGetButton)
/* Put up html table with check box, name, description, etc for each field. */
{
hTableStart();
struct slName *fieldName;
for (fieldName = fieldList; fieldName != NULL; fieldName = fieldName->next)
    {
    char *field = fieldName->name;
    char *var = checkVarName(db, rootTable, field);
    struct asColumn *asCol;
    hPrintf("<TR>");
    hPrintf("<TD>");
    cgiMakeCheckBox(var, varOn(var));
    hPrintf("</TD>");
    hPrintf("<TD>");
    if (showItemRgb && sameWord(field,"reserved"))
	hPrintf("itemRgb");
    else
	hPrintf("%s", field);
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
showTableButtons(db, rootTable, withGetButton);
}

static void showTableFieldsDb(char *db, char *rootTable, boolean withGetButton)
/* Put up a little html table with a check box, name, and hopefully
 * a description for each field in SQL rootTable. */
{
struct sqlConnection *conn = NULL;
if (!trackHubDatabase(database))
    conn = hAllocConn(db);
struct trackDb *tdb = findTdbForTable(db, curTrack, rootTable, ctLookupName);
struct asObject *asObj = asForTable(conn, rootTable);
boolean showItemRgb = FALSE;

showItemRgb=bedItemRgb(tdb);	/* should we expect itemRgb instead of "reserved" */

struct slName *fieldList;
if (isBigBed(database, rootTable, curTrack, ctLookupName))
    fieldList = bigBedGetFields(rootTable, conn);
else if (isLongTabixTable(rootTable))
    fieldList = getLongTabixFields();
else if (isBamTable(rootTable))
    fieldList = bamGetFields();
else if (isVcfTable(rootTable, NULL))
    fieldList = vcfGetFields();
else if (isHicTable(rootTable))
    fieldList = hicGetFields();
else
    {
    char *table = chromTable(conn, rootTable);
    fieldList = sqlListFields(conn, table);
    freez(&table);
    }

showTableFieldsOnList(db, rootTable, asObj, fieldList, showItemRgb, withGetButton);

hFreeConn(&conn);
}

static void showBedTableFields(char *db, char *table, int fieldCount, boolean withGetButton)
/* Put up html table with a check box for each field of custom
 * track. */
{
struct slName *field, *fieldList = getBedFields(fieldCount);

hTableStart();
for (field = fieldList; field != NULL; field = field->next)
    {
    char *var = checkVarName(db, table, field->name);
    hPrintf("<TR><TD>");
    cgiMakeCheckBox(var, varOn(var));
    hPrintf("</TD><TD>");
    hPrintf(" %s<BR>\n", field->name);
    hPrintf("</TD></TR>");
    }
hTableEnd();
showTableButtons(db, table, withGetButton);
}

static void showTableFieldsCt(char *db, char *table, boolean withGetButton)
/* Put up html table with a check box for each field of custom
 * track. */
{
struct customTrack *ct = ctLookupName(table);
char *type = ct->dbTrackType;
if (type == NULL)
    type = ct->tdb->type;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
struct asObject *asObj = asForTdb(conn, ct->tdb);
if (asObj)
    {
    struct slName *fieldList = NULL;
    if (ct->dbTableName != NULL)
	{
	if (isVcfTable(table, NULL))
	    fieldList = vcfGetFields();
	else
	    fieldList = sqlListFields(conn, ct->dbTableName);
	}
    if (fieldList == NULL)
        fieldList = asColNames(asObj);
    showTableFieldsOnList(db, table, asObj, fieldList, FALSE, withGetButton);
    asObjectFree(&asObj);
    slNameFreeList(&fieldList);
    }
else
    showBedTableFields(db, table, ct->fieldCount, withGetButton);
hFreeConn(&conn);
}

static void showTableFields(char *db, char *rootTable, boolean withGetButton)
/* Put up a little html table with a check box, name, and hopefully
 * a description for each field in SQL rootTable. */
{
if (isCustomTrack(rootTable))
    showTableFieldsCt(db, rootTable, withGetButton);
else if (sameWord(rootTable, WIKI_TRACK_TABLE))
    showTableFieldsDb(wikiDbName(), rootTable, withGetButton);
else
    showTableFieldsDb(db, rootTable, withGetButton);
}

static void showLinkedFields(struct dbTable *dtList)
/* Put up a section with fields for each linked table. */
{
struct dbTable *dt;
for (dt = dtList; dt != NULL; dt = dt->next)
    {
    if (! isNoGenomeDisabled(dt->db, dt->table))
        {
        /* Put it up in a new section. */
        webNewSection("%s.%s fields", dt->db, dt->table);
        showTableFields(dt->db, dt->table, FALSE);
        }
    }
}

static void doBigSelectPage(char *db, char *table)
/* Put up big field selection page. Assumes html page open already*/
{
struct joiner *joiner = allJoiner;
struct dbTable *dtList, *dt;
char dbTableBuf[256];

cartSetString(cart, hgtaFieldSelectTable, getDbTable(db, table));
if (strchr(table, '.'))
    htmlOpen("Select Fields from %s", table);
else
    htmlOpen("Select Fields from %s.%s", db, table);
hPrintf("<FORM NAME=\"mainForm\" ACTION=\"%s\" METHOD=%s>\n", cgiScriptName(),
	cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
cgiMakeHiddenVar(hgtaDatabase, db);
cgiMakeHiddenVar(hgtaTable, table);
dbOverrideFromTable(dbTableBuf, &db, &table);

showTableFields(db, table, TRUE);
dtList = extraTableList(selFieldLinkedTablePrefix());
showLinkedFields(dtList);
dt = dbTableNew(db, table);
slAddHead(&dtList, dt);
showLinkedTables(joiner, dtList, selFieldLinkedTablePrefix(),
	hgtaDoSelectFieldsMore, "allow selection from checked tables");

/* clean up. */
hPrintf("</FORM>");
cgiDown(0.9);
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

void doOutSelectedFields(char *table, struct sqlConnection *conn)
/* Put up select fields (for tab-separated output) page. */
{
if (anySubtrackMerge(database, curTable))
    errAbort("Can't do selected fields output when subtrack merge is on. "
    "Please go back and select another output type, or clear the subtrack merge.");
else if (anyIntersection())
    errAbort("Can't do selected fields output when intersection is on. "
    "Please go back and select another output type, or clear the intersection.");
else
    {
    char *fsTable = cartOptionalString(cart, hgtaFieldSelectTable);
    char *dbTable = NULL;
    table = connectingTableForTrack(table);
    dbTable = getDbTable(database, table);
    /* Remove cart state if table has been changed: */
    if (fsTable && ! sameString(fsTable, dbTable))
	{
	cartRemovePrefix(cart, hgtaFieldSelectPrefix);
	cartRemove(cart, hgtaFieldSelectTable);
	}
    doBigSelectPage(database, table);
    }
}

boolean primaryOrLinked(char *dbTableField)
/* Return TRUE if this is the primary table for field selection, or if the field's checkbox
 * is checked, and table is not disabled due to range==genome and 'tableBrowser noGenome'. */
{
char dbTable[PATH_LEN], db[PATH_LEN], table[PATH_LEN];
char *ptr = NULL;

/* Extract just the db.table part of db.table.field as well as db and table separately */
safef(dbTable, sizeof(dbTable), "%s", dbTableField);
ptr = strstr(dbTable, ".hub_");
if (ptr == NULL)
    ptr = strchr(dbTable, '.');
if (ptr == NULL)
    errAbort("Expected 3 .-separated words in %s but can't find first .",
	     dbTableField);
safencpy(db, sizeof(db), dbTable, ptr-dbTable);
char *p2 = strrchr(ptr+1, '.');
if (p2 == NULL)
    errAbort("Expected 3 .-separated words in %s but can't find second .",
	     dbTableField);
*p2 = '\0';
safecpy(table, sizeof(table), ptr+1);
if (isNoGenomeDisabled(db, table))
    return FALSE;
if (sameString(dbTable, cartString(cart, hgtaFieldSelectTable)))
    return TRUE;
else
    {
    char varName[256];
    safef(varName, sizeof(varName),
	  "%s%s", selFieldLinkedTablePrefix(), dbTable);
    return cartUsualBoolean(cart, varName, FALSE);
    }
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

/* Gather together field list for primary and linked tables from cart. */
varList = cartFindPrefix(cart, varPrefix);
for (var = varList; var != NULL; var = var->next)
    {
    if (!sameString(var->val, "0"))
	{
	field = slNameNew(var->name + varPrefixSize);
	if (primaryOrLinked(field->name))
	    slAddHead(&fieldList, field);
	}
    }
if (fieldList == NULL)
    errAbort("Please go back and select at least one field");
slReverse(&fieldList);

/* Do output. */
tabOutSelectedFields(db, table, NULL, fieldList);

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

char *filterFieldVarName(char *db, char *table, char *field, char *type)
/* Return variable name for filter page. */
{
static char buf[256];
safef(buf, sizeof(buf), "%s%s.%s.%s.%s",
	hgtaFilterVarPrefix, db, table, field, type);
return buf;
}

static char *filterPatternVarName(char *db, char *table, char *field)
/* Return variable name for a filter page text box. */
{
return filterFieldVarName(db, table, field, filterPatternVar);
}

static void removeFilterVars()
/* Remove filter variables from cart. */
{
cartRemovePrefix(cart, hgtaFilterPrefix);
cartRemove(cart, hgtaFilterTable);
}

boolean anyFilter()
/* Return TRUE if any filter set.  If there is filter state from a filter
 * defined on a different table, clear it. */
{
char *filterTable = cartOptionalString(cart, hgtaFilterTable);
if (filterTable == NULL)
    {
    removeFilterVars();  // sometimes these get left around due to the back button being used
    return FALSE;
    }
else
    {
    char *dbTable = getDbTable(database, curTable);
    boolean curTableHasFilter = sameString(filterTable, dbTable);
    freez(&dbTable);
    if (curTableHasFilter)
	return TRUE;
    else
	{
	removeFilterVars();
	return FALSE;
	}
    }
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

char *maxOutMenu[] =
{
    "100,000",
    "1,000,000",
    "10,000,000",
};
int maxOutMenuSize = ArraySize(maxOutMenu);

void stringFilterOption(char *db, char *table, char *field, char *logOp)
/* Print out a table row with filter constraint options for a string/char. */
{
char *name;

hPrintf("<TR VALIGN=BOTTOM align='left'><TD colspan=2> %s </TD><TD>\n", field);
name = filterFieldVarName(db, table, field, filterDdVar);
cgiMakeDropListClassWithStyle(name, ddOpMenu, ddOpMenuSize,
                              cartUsualString(cart, name, ddOpMenu[0]),"normalText","width: 76px");
hPrintf("</TD><TD>match </TD><TD>\n");
name = filterPatternVarName(db, table, field);
cgiMakeTextVarWithJs(name, cartUsualString(cart, name, "*"),140,NULL,NULL);
if (logOp == NULL)
    logOp = "";
hPrintf("&nbsp;%s </TD></TR>\n", logOp);
}

static void makeEnumValMenu(char *type, char ***pMenu, int *pMenuSize)
/* Given a SQL type description of an enum or set, parse out the list of
 * values and turn them into a char array for menu display, with "*" as
 * the first item (no constraint).
 * This assumes that the values do not contain the ' character.
 * This will leak a little mem unless you free *pMenu[1] and *pMenu
 * when done. */
{
static char *noop = "*";
char *dup = NULL;
char *words[256];
int wordCount = 0;
int len = 0, i = 0;
if (startsWith("enum(", type))
    dup = cloneString(type + strlen("enum("));
else if (startsWith("set(", type))
    dup = cloneString(type + strlen("set("));
else
    errAbort("makeEnumValMenu: expecting a SQL type description that begins "
	     "with \"enum(\" or \"set(\", but got \"%s\".", type);
if (dup[0] == '"')
    {
    // bigBed uses asColumnToSqlType, which gives double-quoted, comma-and-space-separated string.
    strSwapStrs(dup, strlen(dup)+1, "\", \"", ",");
    stripChar(dup, '"');
    }
else if (dup[0] == '\'')
    // "desc table" in mysql gives single-quoted, no-space comma-sep string.
    stripChar(dup, '\'');
wordCount = chopCommas(dup, words);
len = strlen(words[wordCount-1]);
if (words[wordCount-1][len-1] == ')')
    words[wordCount-1][len-1] = 0;
else
    errAbort("makeEnumValMenu: expecting a ')' at the end of the last word "
	     "of SQL type, but got \"%s\"", type);
*pMenuSize = wordCount + 1;
AllocArray(*pMenu, wordCount+1);
*pMenu[0] = noop;
for (i = 1;  i < wordCount + 1;  i++)
    {
    (*pMenu)[i] = words[i-1];
    }
}

void enumFilterOption(char *db, char *table, char *field, char *type,
		      char *logOp)
/* Print out a table row with filter constraint options for an enum/set.  */
{
char *name = NULL;
char **valMenu = NULL;
int valMenuSize = 0;

hPrintf("<TR VALIGN=BOTTOM align='left'><TD valign=top align='left'colspan=2> %s </TD>"
        "<TD valign=top>\n", field);
name = filterFieldVarName(db, table, field, filterDdVar);
cgiMakeDropListClassWithStyle(name, ddOpMenu, ddOpMenuSize,
                              cartUsualString(cart, name, ddOpMenu[0]),"normalText","width: 76px");
hPrintf("<TD valign=top>%s</TD><TD colspan=4 nowrap>\n", isSqlSetType(type) ? "include" : "match");
name = filterPatternVarName(db, table, field);
makeEnumValMenu(type, &valMenu, &valMenuSize);
if (logOp == NULL)
    logOp = "";
if (valMenuSize-1 > 2)
    {
    struct slName *defaults = cartOptionalSlNameList(cart, name);
    if (defaults == NULL)
	defaults = slNameNew("*");
    cgiMakeCheckboxGroup(name, valMenu, valMenuSize, defaults, 5);
    hPrintf("</TD><TD>%s </TD></TR>\n", logOp);
    }
else
    {
    cgiMakeDropList(name, valMenu, valMenuSize,cartUsualString(cart, name, valMenu[0]));
    hPrintf("&nbsp;%s </TD></TR>\n", logOp);
    }
}


static void numericFilter(char *db, char *table, char *field, char *label,char *logOp)
/* Print out a table row with filter constraint options for a number. */
{
char *name;

hPrintf("<TR VALIGN=BOTTOM align='left'><TD> %s</TD><TD>is</TD><TD colspan=2>\n", label);
name = filterFieldVarName(db, table, field, filterCmpVar);
cgiMakeDropListClassWithStyle(name, cmpOpMenu, cmpOpMenuSize,
                              cartUsualString(cart, name, cmpOpMenu[0]),"normalText","width: 76px");
puts("</TD><TD>\n");
name = filterPatternVarName(db, table, field);
cgiMakeTextVar(name, cartUsualString(cart, name, "0"), 20);
if (logOp == NULL)
    logOp = "";
hPrintf("&nbsp;%s</TD></TR>\n", logOp);
}

static void numericFilterWithLimits(char *db, char *table, char *field, char *label,
                                    double min,double max,char *logOp)
/* Print out a filter constraint for an integer within a range. */
{
char *name;

hPrintf("<TR VALIGN=BOTTOM align='left'><TD> %s</TD><TD>is</TD><TD colspan=2>\n", label);
name = filterFieldVarName(db, table, field, filterCmpVar);
cgiMakeDropListClassWithStyle(name, cmpOpMenu, cmpOpMenuSize,
                              cartUsualString(cart, name, cmpOpMenu[0]),"normalText","width: 76px");
puts("</TD><TD>\n");
name = filterPatternVarName(db, table, field);
cgiMakeTextVar(name, cartUsualString(cart, name, "0"), 20);
if (logOp == NULL)
    logOp = "";
hPrintf("&nbsp;%s</TD></TR>\n", logOp);
}

void integerFilter(char *db, char *table, char *field, char *label,char *logOp)
/* Print out a filter constraint for an integer within a range. */
{
char *name;

hPrintf("<TR VALIGN=BOTTOM align='left'><TD> %s</TD><TD>is</TD><TD colspan=2>\n", label);
name = filterFieldVarName(db, table, field, filterCmpVar);
cgiMakeDropListClassWithStyle(name, cmpOpMenu, cmpOpMenuSize,
                              cartUsualString(cart, name, cmpOpMenu[0]),"normalText","width: 76px");
puts("</TD><TD>\n");
name = filterPatternVarName(db, table, field);
cgiMakeTextVar(name, cartUsualString(cart, name, "0"), 20);
if (logOp == NULL)
    logOp = "";
hPrintf("&nbsp;%s</TD></TR>\n", logOp);
}

void integerFilterWithLimits(char *db, char *table, char *field, char *label,
                             int min,int max,char *logOp)
/* Print out a filter constraint for an integer within a range. */
{
char *name;

hPrintf("<TR VALIGN=BOTTOM align='left'><TD> %s is</TD><TD colspan=2>\n", label);
name = filterFieldVarName(db, table, field, filterCmpVar);
cgiMakeDropListClassWithStyle(name, cmpOpMenu, cmpOpMenuSize,
                              cartUsualString(cart, name, cmpOpMenu[0]),"normalText","width: 76px");
puts("</TD><TD>\n");
name = filterPatternVarName(db, table, field);
int val = cartUsualInt(cart, name, 0);
cgiMakeIntVarWithLimits(name,val,label,140,min,max);
if (logOp == NULL)
    logOp = "";
hPrintf("&nbsp;%s</TD></TR>\n", logOp);
}

void eqFilterOption(char *db, char *table, char *field,
	char *fieldLabel1, char *fieldLabel2, char *logOp)
/* Print out a table row with filter constraint options for an equality
 * comparison. */
{
char *name;

hPrintf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", fieldLabel1);
puts(" is ");
name = filterFieldVarName(db, table, field, filterCmpVar);
cgiMakeDropList(name, eqOpMenu, eqOpMenuSize,
		cartUsualString(cart, name, eqOpMenu[0]));
/* make a dummy pat_ CGI var for consistency with other filter options */
name = filterPatternVarName(db, table, field);
cgiMakeHiddenVar(name, "0");
hPrintf("</TD><TD>\n");
hPrintf("%s\n", fieldLabel2);
if (logOp == NULL)
    logOp = "";
hPrintf("<TD>%s</TD></TR>\n", logOp);
}

static void printSqlFieldListAsControlTable(struct sqlFieldType *ftList, char *db,
	char *rootTable, struct trackDb *tdb, boolean isBedGr)
/* Print out table of controls for fields. */
{
int fieldNum = 0;
int bedGraphColumn = 5;		/*	default score column	*/
int noBinBedGraphColumn = bedGraphColumn;
boolean gotFirst = FALSE;
struct sqlFieldType *ft;
hPrintf("<TABLE BORDER=0>\n");
for (ft = ftList; ft != NULL; ft = ft->next)
    {
    char *field = ft->name;
    char *type = ft->type;
    char *logic = "";

    if ((0 == fieldNum) && (!sameWord(field,"bin")))
	noBinBedGraphColumn -= 1;
    if (!sameWord(type, "longblob"))
	{
	if (!gotFirst)
	    gotFirst = TRUE;
	else if (!isBedGr)
	    logic = " AND ";
	}
    if (!isBedGr || (noBinBedGraphColumn == fieldNum))
	{
	if (isSqlEnumType(type) || isSqlSetType(type))
	    {
	    enumFilterOption(db, rootTable, field, type, logic);
	    }
	else if(isSqlIntType(type))
	    {
	    integerFilter(db, rootTable, field, field, logic);
	    }
	else if(isSqlNumType(type))
	    {
	    if(isBedGr)
		{
		double min, max;
		double tDbMin, tDbMax;

		wigFetchMinMaxLimits(tdb, &min, &max, &tDbMin, &tDbMax);
		if (tDbMin < min)
		    min = tDbMin;
		if (tDbMax > max)
		    max = tDbMax;
		numericFilterWithLimits(db, rootTable, field, field, min, max, logic);
		hPrintf("<TR><TD COLSPAN=3 ALIGN=RIGHT> (%s range: [%g:%g]) "
		    "</TD></TR>\n", field, min, max);
		}
	    else
		{
		numericFilter(db, rootTable, field, field, logic);
		}
	    }
	else //if (isSqlStringType(type))
	    {
	    stringFilterOption(db, rootTable, field, logic);
	    }
	}
    ++fieldNum;
    }
hPrintf("</TABLE>\n");
}

static void filterControlsForTableDb(char *db, char *rootTable)
/* Put up filter controls for a single database table. */
{
struct sqlConnection *conn =  NULL;
if (!trackHubDatabase(db))
    conn = hAllocConn(db);
struct trackDb *tdb = findTdbForTable(db, curTrack, rootTable, ctLookupName);
char *table = rootTable;
if (! (tdb && trackDbSetting(tdb, "bigDataUrl")))
    table = chromTable(conn, rootTable);
boolean isSmallWig = isWiggle(db, table);
boolean isBigWig = tdb ? tdbIsBigWig(tdb) : isBigWigTable(table);
boolean isWig = isSmallWig || isBigWig;
boolean isBedGr = tdb ? tdbIsBedGraph(tdb) : isBedGraph(rootTable);
boolean isBb = tdb ? tdbIsBigBed(tdb) : isBigBed(database, table, curTrack, ctLookupName);
boolean isBam = tdb ? tdbIsBam(tdb) : isBamTable(rootTable);
boolean isLongTabix = tdb ? tdbIsLongTabix(tdb) : isLongTabixTable(rootTable);
boolean isVcf = tdb ? tdbIsVcf(tdb) : isVcfTable(rootTable, NULL);
boolean isHic = tdb ? tdbIsHic(tdb) : isHicTable(rootTable);

if (isWig)
    {
    hPrintf("<TABLE BORDER=0>\n");
    if ((tdb != NULL) && (tdb->type != NULL))
        {
        double min, max;
        wiggleMinMax(tdb,&min,&max);
        numericFilterWithLimits(db, rootTable, filterDataValueVar,filterDataValueVar,min,max,"");

        hPrintf("<TR><TD COLSPAN=3 ALIGN=RIGHT> (dataValue range: [%g:%g]) "
                "</TD></TR></TABLE>\n", min, max);
        }
    else
        {
        numericFilter(db, rootTable, filterDataValueVar,filterDataValueVar, "");
        hPrintf("</TABLE>\n");
        }
    }
else
    {
    struct sqlFieldType *ftList;
    if (isBb)
        ftList = bigBedListFieldsAndTypes(tdb, conn);
    else if (isLongTabix)
	ftList = longTabixListFieldsAndTypes();
    else if (isBam)
	ftList = bamListFieldsAndTypes();
    else if (isVcf)
	ftList = vcfListFieldsAndTypes();
    else if (isHic)
	ftList = hicListFieldsAndTypes();
    else
        ftList = sqlListFieldsAndTypes(conn, table);
    printSqlFieldListAsControlTable(ftList, db, rootTable, tdb, isBedGr);
    }

/* Printf free-form query row. */
if (!(isWig||isBedGr||isBam||isVcf||isLongTabix||isHic))
    {
    char *name;
    hPrintf("<TABLE BORDER=0><TR><TD>\n");
    name = filterFieldVarName(db, rootTable, "", filterRawLogicVar);
    cgiMakeDropList(name, logOpMenu, logOpMenuSize,
		cartUsualString(cart, name, logOpMenu[0]));
    hPrintf(" Free-form query: ");
    name = filterFieldVarName(db, rootTable, "", filterRawQueryVar);
    char *val = cartUsualString(cart, name, "");
    // escape double quotes to avoid HTML parse trouble in the text input.
    val = htmlEncode(val);
    cgiMakeTextVar(name, val, 50);
    hPrintf("</TD></TR></TABLE>\n");
    }

if (isWig||isBedGr||isBam||isVcf||isLongTabix||isHic)
    {
    char *name;
    hPrintf("<TABLE BORDER=0><TR><TD> Limit data output to:&nbsp\n");
    name = filterFieldVarName(db, rootTable, "_", filterMaxOutputVar);
    cgiMakeDropList(name, maxOutMenu, maxOutMenuSize,
		cartUsualString(cart, name, maxOutMenu[0]));
    hPrintf("&nbsp;lines</TD></TR></TABLE>\n");
    }

freez(&table);
hFreeConn(&conn);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoFilterSubmit, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
}

static void filterControlsForTableCt(char *db, char *table)
/* Put up filter controls for a custom track. */
{
struct customTrack *ct = ctLookupName(table);
char *type = ct->dbTrackType;
puts("<TABLE BORDER=0>");

if (type != NULL && startsWithWord("maf", type))
    {
    stringFilterOption(db, table, "chrom", " AND ");
    integerFilter(db, table, "chromStart", "chromStart", " AND ");
    integerFilter(db, table, "chromEnd", "chromEnd", " AND ");
    }
else if (type != NULL && 
        (startsWithWord("makeItems", type) || 
        sameWord("bedDetail", type) || 
        sameWord("barChart", type) || 
        sameWord("interact", type) || 
        sameWord("pgSnp", type)))
    {
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct sqlFieldType *ftList = sqlListFieldsAndTypes(conn, ct->dbTableName);
    printSqlFieldListAsControlTable(ftList, db, table, ct->tdb, FALSE);
    hFreeConn(&conn);
    }
else if (ct->wiggle || isBigWigTable(table))
    {
    if ((ct->tdb != NULL) && (ct->tdb != NULL))
        {
        double min, max;
        wiggleMinMax(ct->tdb,&min,&max);

        numericFilterWithLimits("ct", table, filterDataValueVar, filterDataValueVar,min,max,"");
        hPrintf("<TR><TD COLSPAN=3 ALIGN=RIGHT> (dataValue range: [%g,%g]) "
                "</TD></TR>\n", min, max);
        }
    else
        {
        numericFilter("ct", table, filterDataValueVar, filterDataValueVar,"");
        }
    }
else if (isBigBed(db, table, curTrack, ctLookupName))
    {
    struct sqlFieldType *ftList = bigBedListFieldsAndTypes(ct->tdb, NULL);
    printSqlFieldListAsControlTable(ftList, db, table, ct->tdb, FALSE);
    }
else if (isLongTabixTable(table))
    {
    struct sqlFieldType *ftList = longTabixListFieldsAndTypes();
    printSqlFieldListAsControlTable(ftList, db, table, ct->tdb, FALSE);
    }
else if (isBamTable(table))
    {
    struct sqlFieldType *ftList = bamListFieldsAndTypes();
    printSqlFieldListAsControlTable(ftList, db, table, ct->tdb, FALSE);
    }
else if (isVcfTable(table, NULL))
    {
    struct sqlFieldType *ftList = vcfListFieldsAndTypes();
    printSqlFieldListAsControlTable(ftList, db, table, ct->tdb, FALSE);
    }
else if (isHicTable(table))
    {
    struct sqlFieldType *ftList = hicListFieldsAndTypes();
    printSqlFieldListAsControlTable(ftList, db, table, ct->tdb, FALSE);
    }
else
    {
    if (ct->fieldCount >= 3)
        {
        stringFilterOption(db, table, "chrom", " AND ");
        integerFilter(db, table, "chromStart", "chromStart", " AND ");
        integerFilter(db, table, "chromEnd", "chromEnd", " AND ");
        }
    if (ct->fieldCount >= 4)
        {
        stringFilterOption(db, table, "name", " AND ");
        }
    if (ct->fieldCount >= 5)
        {
        numericFilter(db, table, "score", "score", " AND ");
        }
    if (ct->fieldCount >= 6)
        {
        stringFilterOption(db, table, "strand", " AND ");
        }
    if (ct->fieldCount >= 8)
        {
        integerFilter(db, table, "thickStart", "thickStart", " AND ");
        integerFilter(db, table, "thickEnd", "thickEnd", " AND ");
        }
    if (ct->fieldCount >= 12)
        {
        integerFilter(db, table, "blockCount", "blockCount", " AND ");
        }
    /* These are not bed fields, just extra constraints that we offer: */
    if (ct->fieldCount >= 3)
        {
        integerFilter(db, table, "chromLength", "(chromEnd - chromStart)",
                      (ct->fieldCount >= 8) ? " AND " : "");
        }
    if (ct->fieldCount >= 8)
        {
        integerFilter( db, table, "thickLength",  "(thickEnd - thickStart)", " AND ");
        eqFilterOption(db, table, "compareStarts","chromStart","thickStart", " AND ");
        eqFilterOption(db, table, "compareEnds",  "chromEnd",  "thickEnd",   "");
        }
    }

puts("</TABLE>");

if (ct->wiggle || isBigWigTable(table) || isBamTable(table) || isVcfTable(table, NULL) || isLongTabixTable(table) || isHicTable(table))
    {
    char *name;
    hPrintf("<TABLE BORDER=0><TR><TD> Limit data output to:&nbsp\n");
    name = filterFieldVarName("ct", table, "_", filterMaxOutputVar);
    cgiMakeDropList(name, maxOutMenu, maxOutMenuSize,
		cartUsualString(cart, name, maxOutMenu[0]));
    hPrintf("&nbsp;lines</TD></TR></TABLE>\n");
    }

hPrintf("<BR>\n");
cgiMakeButton(hgtaDoFilterSubmit, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
}


static void filterControlsForTable(char *db, char *rootTable)
/* Put up filter controls for a single table. */
{
if (isCustomTrack(rootTable))
    filterControlsForTableCt(db, rootTable);
else
    filterControlsForTableDb(db, rootTable);
cgiDown(0.7); // Extra spacing below the buttons
}

static void showLinkedFilters(struct dbTable *dtList)
/* Put up a section with filters for each linked table. */
{
struct dbTable *dt;
for (dt = dtList; dt != NULL; dt = dt->next)
    {
    if (! isNoGenomeDisabled(dt->db, dt->table))
        {
        /* Put it up in a new section. */
        webNewSection("%s.%s based filters", dt->db, dt->table);
        filterControlsForTable(dt->db, dt->table);
        }
    }
}


#define filterLinkedTablePrefix hgtaFilterPrefix "linked."

static void doBigFilterPage(struct sqlConnection *conn, char *db, char *table)
/* Put up filter page on given db.table. */
{
struct joiner *joiner = allJoiner;
struct dbTable *dtList, *dt;
char dbTableBuf[256];

if (strchr(table, '.'))
    htmlOpen("Filter on Fields from %s", table);
else
    htmlOpen("Filter on Fields from %s.%s", db, table);

jsIncludeFile("jquery.js", NULL);
jsIncludeFile("utils.js", NULL);
commonCssStyles();

hPrintf("<FORM ACTION=\"%s\" METHOD=%s>\n", cgiScriptName(),
	cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
cgiMakeHiddenVar(hgtaDatabase, db);
cgiMakeHiddenVar(hgtaTable, table);
dbOverrideFromTable(dbTableBuf, &db, &table);

filterControlsForTable(db, table);
dtList = extraTableList(filterLinkedTablePrefix);
showLinkedFilters(dtList);
dt = dbTableNew(db, table);
slAddHead(&dtList, dt);
showLinkedTables(joiner, dtList, filterLinkedTablePrefix,
	hgtaDoFilterMore, "allow filtering using fields in checked tables");

hPrintf("</FORM>\n");
cgiDown(0.9);
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
char *table = connectingTableForTrack(curTable);
char *db = database;
if (sameWord(table, WIKI_TRACK_TABLE))
    db = wikiDbName();
doBigFilterPage(conn, db, table);
}

void doFilterSubmit(struct sqlConnection *conn)
/* Respond to submit on filters page. */
{
cartSetString(cart, hgtaFilterTable, getDbTable(database, curTable));
doMainPage(conn);
}

void doClearFilter(struct sqlConnection *conn)
/* Respond to click on clear filter. */
{
removeFilterVars();
doMainPage(conn);
}


static boolean wildReal(char *pat)
/* Return TRUE if pat is something we really might want to search on. */
{
return pat != NULL && pat[0] != 0 && !sameString(pat, "*");
}

static boolean cmpReal(char *pat, char *cmpOp)
/* Return TRUE if we have a real cmpOp. */
{
return isNotEmpty(pat) && stringArrayIx(cmpOp, cmpOpMenu, cmpOpMenuSize) > 0;
}

static boolean filteredOrLinked(char *db, char *table)
/* Return TRUE if this table is the table to be filtered or if it is to be
 * linked with that table, and is not disabled due to region==genome and 'tableBrowser noGenome'. */
{
if (isNoGenomeDisabled(db, table))
    return FALSE;
char *dbTable = getDbTable(db, table);
char *filterTable = cartUsualString(cart, hgtaFilterTable, "");
boolean isFilterTable = sameString(dbTable, filterTable);
freez(&dbTable);
if (isFilterTable)
    return TRUE;
else
    {
    char varName[256];
    safef(varName, sizeof(varName),
	  "%slinked.%s.%s", hgtaFilterPrefix, db, table);
    return cartUsualBoolean(cart, varName, FALSE);
    }
}

struct joinerDtf *filteringTables()
/* Get list of tables we're filtering on as joinerDtf list (with
 * the field entry NULL). */
{
if (!anyFilter())
    return NULL;
else
    {
    struct joinerDtf *dtfList = NULL, *dtf;
    struct hashEl *varList, *var;
    struct hash *uniqHash = hashNew(0);
    int prefixSize = strlen(hgtaFilterVarPrefix);
    varList = cartFindPrefix(cart, hgtaFilterVarPrefix);
    for (var = varList; var != NULL; var = var->next)
        {
	char *dupe = cloneString(var->name + prefixSize);
	char *parts[5];
	int partCount;
	char dbTable[256];
	char *db, *table, *field, *type;
	partCount = chopByChar(dupe, '.', parts, ArraySize(parts));
	if (partCount != 4)
	    {
	    warn("Part count != expected 4 line %d of %s", __LINE__, __FILE__);
	    continue;
	    }
	db = parts[0];
	table = parts[1];
	field = parts[2];
	type = parts[3];
	safef(dbTable, sizeof(dbTable), "%s.%s", db, table);
	if (! filteredOrLinked(db, table))
	    continue;
	if (!hashLookup(uniqHash, dbTable))
	    {
	    boolean gotFilter = FALSE;

	    if (sameString(type, filterPatternVar))
	        {
		char *pat = trimSpaces(var->val);
		gotFilter = wildReal(pat);
		}
	    else if (sameString(type, filterCmpVar))
	        {
		char *patVar = filterPatternVarName(db, table, field);
		char *pat = trimSpaces(cartOptionalString(cart, patVar));
		gotFilter = cmpReal(pat, var->val);
		}
	    else if (sameString(type, filterRawQueryVar))
	        {
		char *pat = trimSpaces(var->val);
		gotFilter = (pat != NULL && pat[0] != 0);
		}
	    if (gotFilter)
		{
		hashAdd(uniqHash, dbTable, NULL);
		AllocVar(dtf);
		dtf->database = cloneString(db);
		dtf->table = cloneString(table);
		slAddHead(&dtfList, dtf);
		}
	    }
	freeMem(dupe);
	}
    hashFree(&uniqHash);
    return dtfList;
    }
}

static char *getSqlType(struct sqlConnection *conn, char *table, char *field)
/* Return the type of the given field. */
{
struct sqlResult *sr = NULL;
char **row = NULL;
char query[512];
char *type = NULL;
sqlSafef(query, sizeof(query), "describe %s %s", table, field);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    type = cloneString(row[1]);
else
    errAbort("getSqlType: no results for query \"%s\".", query);
sqlFreeResult(&sr);
return type;
}

static void normalizePatList(struct slName **pPatList)
/* patList might be a plain old list of terms, in which case we keep the
 * terms only if they are not no-ops.  patList might also be one element
 * that is a space-separated list of terms, in which case we make a new
 * list item for each non-no-op term.  (Trim spaces while we're at it.) */
{
struct slName *pat, *nextPat, *patListOut = NULL;
if (pPatList == NULL) return;
for (pat = *pPatList;  pat != NULL;  pat = nextPat)
    {
    nextPat = pat->next;
    strcpy(pat->name, trimSpaces(pat->name));
    if (hasWhiteSpace(pat->name))
	{
	char *line = pat->name, *word;
	while ((word = nextQuotedWord(&line)) != NULL)
	    if (wildReal(word))
		{
		struct slName *newPat = slNameNew(word);
		slAddHead(&patListOut, newPat);
		}
	slNameFree(&pat);
	}
    else if (wildReal(pat->name))
	slAddHead(&patListOut, pat);
    }
*pPatList = patListOut;
}

char *filterClause(char *db, char *table, char *chrom, char *extraClause)
/* Get filter clause (something to put after 'where')
 * for table */
{
struct sqlConnection *conn = NULL;
char varPrefix[128];
int varPrefixSize, fieldNameSize;
struct hashEl *varList, *var;
struct dyString *dy = NULL;
boolean needAnd = FALSE;
char oldDb[128];
char dbTableBuf[256];
char explicitDb[128];
char splitTable[256];
char explicitDbTable[512];

/* Return just extraClause (which may be NULL) if no filter on us. */
if (! (anyFilter() && filteredOrLinked(db, table)))
    return cloneString(extraClause);
safef(oldDb, sizeof(oldDb), "%s", db);
dbOverrideFromTable(dbTableBuf, &db, &table);
if (!sameString(oldDb, db))
     safef(explicitDb, sizeof(explicitDb), "%s.", db);
else
     explicitDb[0] = 0;

/* Cope with split table and/or custom tracks. */
if (isCustomTrack(table))
    {
    conn = hAllocConn(CUSTOM_TRASH);
    struct customTrack *ct = ctLookupName(table);
    safef(explicitDbTable, sizeof(explicitDbTable), "%s", ct->dbTableName);
    }
else
    {
    conn = hAllocConn(db);
    safef(splitTable, sizeof(splitTable), "%s_%s", chrom, table);
    if (!sqlTableExists(conn, splitTable))
	safef(splitTable, sizeof(splitTable), "%s", table);
    safef(explicitDbTable, sizeof(explicitDbTable), "%s%s",
	  explicitDb, splitTable);
    }

/* Get list of filter variables for this table. */
safef(varPrefix, sizeof(varPrefix), "%s%s.%s.", hgtaFilterVarPrefix, db, table);
varPrefixSize = strlen(varPrefix);
varList = cartFindPrefix(cart, varPrefix);
if (varList == NULL)
    {
    hFreeConn(&conn);
    return cloneString(extraClause);
    }

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
    sqlCkId(field);
    type += 1;
    /* rawLogic and rawQuery are handled below;
     * filterMaxOutputVar is not really a filter variable and is handled
     * in wiggle.c. */
    if (startsWith("raw", type) || sameString(filterMaxOutputVar, type))
	continue;
    /*	Any other variables that are missing a name:
     *		<varPrefix>..<type>
     *	are illegal
     */
    if (fieldNameSize < 1)
	{
	warn("Missing name in cart variable: %s\n", var->name);
	continue;
	}
    if (sameString(type, filterDdVar))
        {
	char *patVar = filterPatternVarName(db, table, field);
	struct slName *patList = cartOptionalSlNameList(cart, patVar);
	normalizePatList(&patList);
	if (slCount(patList) > 0)
	    {
	    char *ddVal = cartString(cart, var->name);
	    boolean neg = sameString(ddVal, ddOpMenu[1]);
	    char *fieldType = getSqlType(conn, explicitDbTable, field);
	    boolean needOr = FALSE;
	    if (needAnd) dyStringAppend(dy, " and ");
	    needAnd = TRUE;
	    if (neg) dyStringAppend(dy, "not ");
	    boolean composite = (slCount(patList) > 1);
	    if (composite || neg) dyStringAppendC(dy, '(');
	    struct slName *pat;
	    for (pat = patList;  pat != NULL;  pat = pat->next)
		{
		char *sqlPat = sqlLikeFromWild(pat->name);
		if (needOr)
		    dyStringAppend(dy, " OR ");
		needOr = TRUE;
		if (isSqlSetType(fieldType))
		    {
		    sqlDyStringPrintfFrag(dy, "FIND_IN_SET('%s', %s.%s)>0 ",
				   sqlPat, explicitDbTable , field);
		    }
		else
		    {
		    sqlDyStringPrintfFrag(dy, "%s.%s ", explicitDbTable, field);
		    if (sqlWildcardIn(sqlPat))
			dyStringAppend(dy, "like ");
		    else
			dyStringAppend(dy, "= ");
		    sqlDyStringPrintf(dy, "'%s'", sqlPat);
		    }
		freez(&sqlPat);
		}
	    if (composite || neg) dyStringAppendC(dy, ')');
	    }
	}
    else if (sameString(type, filterCmpVar))
        {
	char *patVar = filterPatternVarName(db, table, field);
	char *pat = trimSpaces(cartOptionalString(cart, patVar));
	char *cmpVal = cartString(cart, var->name);
	if (cmpReal(pat, cmpVal))
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
		    sqlDyStringPrintfFrag(dy, "%s.%s >= %f && %s.%s <= %f",
		    	explicitDbTable, field, a, explicitDbTable, field, b);
		    }
		else
		    {
		    int a = atoi(words[0]), b = atoi(words[1]);
		    sqlDyStringPrintfFrag(dy, "%s.%s >= %d && %s.%s <= %d",
		    	explicitDbTable, field, a, explicitDbTable, field, b);
		    }
		freez(&dupe);
		}
	    else
	        {
		// cmpVal has been checked already above in cmpReal for legal values.
		sqlDyStringPrintfFrag(dy, "%s.%s %-s ", explicitDbTable, field, cmpVal);
		if (strchr(pat, '.'))	/* Assume floating point. */
		    dyStringPrintf(dy, "%f", atof(pat));
		else
		    dyStringPrintf(dy, "%d", atoi(pat));
		}
	    }
	}
    }
/* Handle rawQuery if any */
    {
    char *varName;
    char *logic, *query;
    varName = filterFieldVarName(db, table, "", filterRawLogicVar);
    logic = cartUsualString(cart, varName, logOpMenu[0]);
    varName = filterFieldVarName(db, table, "", filterRawQueryVar);
    query = trimSpaces(cartOptionalString(cart, varName));
    if (query != NULL && query[0] != 0)
        {
	if (needAnd) dyStringPrintf(dy, " %s ", logic);
	sqlSanityCheckWhere(query, dy);
	}
    }

/* Clean up and return */
hFreeConn(&conn);
hashElFreeList(&varList);
if (dy->stringSize == 0)
    {
    dyStringFree(&dy);
    return cloneString(extraClause);
    }
else
    {
    if (isNotEmpty(extraClause))
	dyStringPrintf(dy, " and %s", extraClause);
    return dyStringCannibalize(&dy);
    }
}



void doTest(struct sqlConnection *conn)
/* Put up a page to see what happens. */
{
char *s = NULL;
textOpen();
hPrintf("Doing test!\n");
s = filterClause("hg18", "knownGene", "chrX", NULL);
if (s != NULL)
    hPrintf("%s\n", s);
else
    hPrintf("%p\n", s);
}

void cgiToCharFilter(char *dd, char *pat, enum charFilterType *retCft,
		     char **retVals, boolean *retInv)
/* Given a "does/doesn't" and a (list of) literal chars from CGI, fill in
 * retCft, retVals and retInv to make a filter. */
{
char *vals, *ptrs[32];
int numWords;
int i;

assert(retCft != NULL);
assert(retVals != NULL);
assert(retInv != NULL);
assert(sameString(dd, "does") || sameString(dd, "doesn't"));

/* catch null-constraint cases.  ? will be treated as a literal match,
 * which would make sense for bed strand and maybe other single-char things: */
if (pat == NULL)
    pat = "";
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*"))
    {
    *retCft = cftIgnore;
    return;
    }

*retCft = cftMultiLiteral;
numWords = chopByWhite(pat, ptrs, ArraySize(ptrs));
vals = needMem((numWords+1) * sizeof(char));
for (i=0;  i < numWords;  i++)
    vals[i] = ptrs[i][0];
vals[i] = 0;
*retVals = vals;
*retInv = sameString("doesn't", dd);
}

void cgiToStringFilter(char *dd, char *pat, enum stringFilterType *retSft,
		       char ***retVals, boolean *retInv)
/* Given a "does/doesn't" and a (list of) regexps from CGI, fill in
 * retCft, retVals and retInv to make a filter. */
{
char **vals, *ptrs[32];
int numWords;
int i;

assert(retSft != NULL);
assert(retVals != NULL);
assert(retInv != NULL);
assert(sameString(dd, "does") || sameString(dd, "doesn't"));

/* catch null-constraint cases: */
if (pat == NULL)
    pat = "";
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*"))
    {
    *retSft = sftIgnore;
    return;
    }

*retSft = sftMultiRegexp;
numWords = chopByWhite(pat, ptrs, ArraySize(ptrs));
vals = needMem((numWords+1) * sizeof(char *));
for (i=0;  i < numWords;  i++)
    vals[i] = cloneString(ptrs[i]);
vals[i] = NULL;
*retVals = vals;
*retInv = sameString("doesn't", dd);
}

void cgiToDoubleFilter(char *cmp, char *pat, enum numericFilterType *retNft,
		    double **retVals)
/* Given a comparison operator and a (pair of) integers from CGI, fill in
 * retNft and retVals to make a filter. */
{
char *ptrs[3];
double *vals;
int numWords;

assert(retNft != NULL);
assert(retVals != NULL);

/* catch null-constraint cases: */
if (pat == NULL)
    pat = "";
if (cmp == NULL)
    errAbort("Null cmp passed to cgiToDoubleFilter (pattern=%s)", pat);
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*") || sameString(cmp, "ignored"))
    {
    *retNft = nftIgnore;
    *retVals = NULL;
    return;
    }
else if (sameString(cmp, "in range"))
    {
    *retNft = nftInRange;
    numWords = chopString(pat, " \t,", ptrs, ArraySize(ptrs));
    if (numWords != 2)
	errAbort("For \"in range\" constraint, you must give two numbers separated by whitespace or comma.");
    vals = needMem(2 * sizeof(double));
    vals[0] = atof(ptrs[0]);
    vals[1] = atof(ptrs[1]);
    if (vals[0] > vals[1])
	{
	int tmp = vals[0];
	vals[0] = vals[1];
	vals[1] = tmp;
	}
    *retVals = vals;
   }
else
    {
    if (sameString(cmp, "<"))
	*retNft = nftLessThan;
    else if (sameString(cmp, "<="))
	*retNft = nftLTE;
    else if (sameString(cmp, "="))
	*retNft = nftEqual;
    else if (sameString(cmp, "!="))
	*retNft = nftNotEqual;
    else if (sameString(cmp, ">="))
	*retNft = nftGTE;
    else if (sameString(cmp, ">"))
	*retNft = nftGreaterThan;
    else
	errAbort("Unrecognized comparison operator %s", cmp);
    vals = needMem(2*sizeof(double));	// 2* simplifies conversion elsewhere.
    vals[0] = atof(pat);
    *retVals = vals;
    }
}

void cgiToIntFilter(char *cmp, char *pat, enum numericFilterType *retNft,
		    int **retVals)
/* Given a comparison operator and a (pair of) integers from CGI, fill in
 * retNft and retVals to make a filter. */
{
double *doubleVals;
cgiToDoubleFilter(cmp, pat, retNft, &doubleVals);
if (doubleVals == NULL)
    {
    *retVals = NULL;
    return;
    }
int *intVals = needMem(2*sizeof(int));
intVals[0] = doubleVals[0];
intVals[1] = doubleVals[1];
freeMem(doubleVals);
*retVals = intVals;
}

void cgiToLongFilter(char *cmp, char *pat, enum numericFilterType *retNft,
		    long long **retVals)
/* Given a comparison operator and a (pair of) integers from CGI, fill in
 * retNft and retVals to make a filter. */
{
double *doubleVals;
cgiToDoubleFilter(cmp, pat, retNft, &doubleVals);
if (doubleVals == NULL)
    {
    *retVals = NULL;
    return;
    }
long long *longVals = needMem(2*sizeof(long long));
longVals[0] = doubleVals[0];
longVals[1] = doubleVals[1];
freeMem(doubleVals);
*retVals = longVals;
}

