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

void fieldSelectorTable(struct joiner *joiner, char *db, char *rootTable)
/* Put up table that lets user select fields. */
{
struct sqlConnection *conn = sqlConnect(db);
char *table = chromTable(conn, rootTable);
char query[256];
struct sqlResult *sr;
char **row;
struct asObject *asObj = asForTable(conn, rootTable);
struct joinerPair *jpList;


jpList = joinerRelate(joiner, db, rootTable);
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);

hTableStart();
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *field = row[0];
    char *var = checkVarName(rootTable, field);
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
	    hPrintf("-");
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
}

void doBigSelectPage(char *db, char *table)
/* Put up big field selection page. Assumes html page open already*/
{
struct joiner *joiner = joinerRead("all.joiner");
struct hash *tableHash = newHash(0);

hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=POST>\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hgtaDatabase, database);
cgiMakeHiddenVar(hgtaTable, table);
fieldSelectorTable(joiner, database, table);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoSelectedFields, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>");
joinerFree(&joiner);
}


void doOutSelectedFields(struct trackDb *track, struct sqlConnection *conn)
/* Put up select fields (for tab-separated output) page. */
{
char *table = connectingTableForTrack(track);
htmlOpen("Select Fields from %s (%s.%s)", 
	track->shortLabel, database, table);
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

