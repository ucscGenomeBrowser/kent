/* Put up pages for selecting and filtering on fields. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "dlist.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "web.h"
#include "trackDb.h"
#include "asParse.h"
#include "hgTables.h"
#include "joiner.h"

static char *fsVarName(char *type, char *table, char *field)
/* Get variable name that encodes type, table and field. */
{
static char buf[128];
safef(buf, sizeof(buf), "%s%s%s_%s", 
	hgtaFieldSelectPrefix, type, table, field);
return buf;
}

static char *checkVarName(char *table, char *field)
/* Get variable name for check box on given table/field. */
{
return fsVarName("check_", table, field);
}

static char *openLinkedVarName(char *table, char *field)
/* Get variable name that lets us know whether to include
 * linked tables or not. */
{
return fsVarName("openLinked_", table, field);
}

static boolean varOn(char *var)
/* Return TRUE if variable exists and is set. */
{
return cartVarExists(cart, var) && cartBoolean(cart, var);
}

boolean anyJoinings(struct joinerPair *jpList, char *field)
/* Return TRUE if field looks like a joiner target. */
{
struct joinerPair *jp;
for (jp = jpList; jp != NULL; jp = jp->next)
    {
    if (sameString(jp->a->field, field))
        return TRUE;
    }
return FALSE;
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

void showTableSection(struct joiner *joiner, struct dbTable *dt, 
	struct dlList *tableList)
/* Put up a section of web page with check boxes and links
 * for each field of table.  If there are open links 
 * add them to the tail of the table list. */
{
struct sqlConnection *conn = sqlConnect(dt->db);
char *table = chromTable(conn, dt->table);
char query[256];
struct sqlResult *sr;
char **row;
struct asObject *asObj = asForTable(conn, dt->table);
struct joinerPair *jpList;

webNewSection("Fields in %s.%s", dt->db, dt->table);
jpList = joinerRelate(joiner, dt->db, dt->table);
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);

hTableStart();
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *field = row[0];
    char *var = checkVarName(dt->table, field);
    struct asColumn *asCol;
    hPrintf("<TR>");
    hPrintf("<TD>");
    cgiMakeCheckBox(var, varOn(var));
    hPrintf("</TD>");
    hPrintf("<TD>");
    if (anyJoinings(jpList, field ))
        {
	boolean opened;
	var = openLinkedVarName(table, field);
	opened = varOn(var);
	hPrintf("<A HREF=\"../cgi-bin/hgTables");
	hPrintf("?%s", cartSidUrlString(cart));
	hPrintf("&%s=on", hgtaDoTopSubmit);
	hPrintf("&%s=%d", var, !opened);
	hPrintf("\">&nbsp;");
	if (opened)
	    {
	    struct joinerPair *jp;
	    hPrintf("-");
	    /* Go add stuff to list. */
	    for (jp = jpList; jp != NULL; jp = jp->next)
	        {
		if (sameString(jp->a->field, field))
		    {
		    dlAddValTail(tableList, 
		    	dbTableNew(jp->b->database, jp->b->table));
		    }
		}
	    }
	else
	    hPrintf("+");
	hPrintf("&nbsp;</A>");
	}
    else
        {
	hPrintf("&nbsp;");
	}
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

joinerPairFreeList(&jpList);
freez(&table);
sqlDisconnect(&conn);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoSelectedFields, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
}


void fieldSelectorTable(struct joiner *joiner, struct dlList *tableList)
/* Put up table that lets user select fields.  As a side effect this
 * will empty out tableList. */
{
struct dbTable *dt;
struct dlNode *node;
struct hash *seenHash = hashNew(0);
char tableName[256];

/* Keep processing as long as there are tables on list,
 * filtering out tables we've seen already. */
while ((node = dlPopHead(tableList)) != NULL)
    {
    dt = node->val;
    safef(tableName, sizeof(tableName), "%s.%s", dt->db, dt->table);
    if (!hashLookup(seenHash, tableName))
        {
	hashAdd(seenHash, tableName, NULL);
	showTableSection(joiner, dt, tableList);
	}

    /* Free up this node, we are done. */
    dbTableFree(&dt);
    freez(&node);
    }
hashFree(&seenHash);
}

void doBigSelectPage(char *db, char *table)
/* Put up big field selection page. Assumes html page open already*/
{
struct joiner *joiner = joinerRead("all.joiner");
struct dlList *tableList = newDlList();

hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=POST>\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hgtaDatabase, database);
cgiMakeHiddenVar(hgtaTable, table);

dlAddValTail(tableList, dbTableNew(database, table));
fieldSelectorTable(joiner, tableList);

/* clean up. */
hPrintf("</FORM>");
joinerFree(&joiner);
freeDlList(&tableList);
}


void doOutSelectedFields(struct trackDb *track, struct sqlConnection *conn)
/* Put up select fields (for tab-separated output) page. */
{
char *table = connectingTableForTrack(track);
htmlOpen("Select Fields from %s And Related Tables", track->shortLabel);
hPrintf(
   "Use the check boxes to select the fields you want to include. "
   "In some cases a field links to other tables.  Click on the '+' "
   "by the field name to view and select from these tables as well. "
   "(They will appear under the primary table.)" );
doBigSelectPage(database, table);
htmlClose();
}

void doSelectedFields()
/* Actually produce selected field output as text stream. */
{
char *db = cartString(cart, hgtaDatabase);
char *table = cartString(cart, hgtaTable);
struct sqlConnection *conn = sqlConnect(db);
char *varPrefix = checkVarName(table, "");
int varPrefixSize = strlen(varPrefix);
struct hashEl *varList, *var;
struct dyString *fields = dyStringNew(0);
int fieldCount = 0;

textOpen();
varList = cartFindPrefix(cart, varPrefix);
for (var = varList; var != NULL; var = var->next)
    {
    if (!sameString(var->val, "0"))
	{
	if (fieldCount > 0)
	    dyStringAppendC(fields, ',');
	dyStringAppend(fields, var->name + varPrefixSize);
	++fieldCount;
	}
    }
if (fieldCount < 1)
    errAbort("Please go back and select at least one field");
doTabOutTable(table, conn, fields->string);

/* Clean up. */
dyStringFree(&fields);
sqlDisconnect(&conn);
}

