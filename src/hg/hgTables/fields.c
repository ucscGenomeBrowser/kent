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
#include "hgTables.h"

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

void fieldSelectorTable(char *db, char *rootTable)
/* Put up table that lets user select fields. */
{
struct sqlConnection *conn = sqlConnect(db);
char *table = chromTable(conn, rootTable);
char query[256];
struct sqlResult *sr;
char **row;

safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *field = row[0];
    char *var = checkVarName(rootTable, field);
    boolean isChecked = (cartVarExists(cart, var) && cartBoolean(cart, var));
    cgiMakeCheckBox(var, isChecked);
    hPrintf(" %s<BR>\n", field);
    }

freez(&table);
sqlDisconnect(&conn);
}

void doOutSelectedFields(struct trackDb *track, struct sqlConnection *conn)
/* Put up select fields (for tab-separated output) page. */
{
htmlOpen("Select Fields from %s (%s.%s)", 
	track->shortLabel, database, track->tableName);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=POST>\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hgtaDatabase, database);
cgiMakeHiddenVar(hgtaTable, track->tableName);
fieldSelectorTable(database, track->tableName);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoSelectedFields, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>");
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

