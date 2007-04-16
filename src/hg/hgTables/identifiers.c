/* identifiers - handle identifier lists: uploading, pasting,
 * and restricting to just things on the list. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "portable.h"
#include "hgTables.h"
#include "trashDir.h"
#include "web.h"

static char const rcsid[] = "$Id: identifiers.c,v 1.12 2007/04/16 20:28:33 angie Exp $";


static void getXrefInfo(char **retXrefTable, char **retIdField,
			char **retAliasField)
/* See if curTrack specifies an xref/alias table for lookup of IDs. */
{
char *xrefSpec = curTrack ? trackDbSetting(curTrack, "idXref") : NULL;
char *xrefTable = NULL, *idField = NULL, *aliasField = NULL;
if (xrefSpec != NULL)
    {
    char *words[3];
    chopLine(cloneString(xrefSpec), words);
    if (isEmpty(words[2]))
	errAbort("trackDb error: track %s, setting idXref must be followed "
		 "by three words (xrefTable, idField, aliasField).",
		 curTrack->tableName);
    xrefTable = words[0];
    idField = words[1];
    aliasField = words[2];
    }
if (retXrefTable != NULL)
    *retXrefTable = xrefTable;
if (retIdField != NULL)
    *retIdField = idField;
if (retAliasField != NULL)
    *retAliasField = aliasField;
}

static struct slName *getExamples(char *table, char *field, int count)
/* Return a list of several example values of table.field. */
{
char fullTable[HDB_MAX_TABLE_STRING];
if (! hFindSplitTableDb(database, NULL, table, fullTable, NULL))
    safef(fullTable, sizeof(fullTable), table);
return sqlRandomSample(database, fullTable, field, count);
}

static void explainIdentifiers(struct sqlConnection *conn, char *idField)
/* Tell the user what field(s) they may paste/upload values for, and give 
 * some examples. */
{
char *xrefTable = NULL, *aliasField = NULL;
struct slName *exampleList = NULL, *ex;
getXrefInfo(&xrefTable, NULL, &aliasField);
hPrintf("The items must be values of the <B>%s</B> field of the currently "
	"selected table, <B>%s</B>",
	idField, curTable);
if (aliasField != NULL)
    hPrintf(", or the <B>%s</B> field of the alias table <B>%s</B>.\n",
	    aliasField, xrefTable);
else
    hPrintf(".\n");
hPrintf("(The \"describe table schema\" button shows more information about "
	"the table fields.)\n");
hPrintf("Some example values:<BR>\n");
exampleList = getExamples(curTable, idField, 3);
for (ex = exampleList;  ex != NULL;  ex = ex->next)
    hPrintf("<TT>%s</TT><BR>\n", ex->name);
if (aliasField != NULL)
    {
    exampleList = getExamples(xrefTable, aliasField, 3);
    for (ex = exampleList;  ex != NULL;  ex = ex->next)
	hPrintf("<TT>%s</TT><BR>\n", ex->name);
    }
hPrintf("\n");
}

void doPasteIdentifiers(struct sqlConnection *conn)
/* Respond to paste identifiers button. */
{
char *oldPasted = cartUsualString(cart, hgtaPastedIdentifiers, "");
struct hTableInfo *hti = hFindTableInfoDb(database, NULL, curTable);
char *idField = getIdField(database, curTrack, curTable, hti);
htmlOpen("Paste In Identifiers for %s", curTableLabel());
if (idField == NULL)
    errAbort("Sorry, I can't tell which field of table %s to treat as the "
	     "identifier field.", curTable);
hPrintf("<FORM ACTION=\"..%s\" METHOD=%s>\n", getScriptName(),
	cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
hPrintf("Please paste in the identifiers you want to include.\n");
explainIdentifiers(conn, idField);
hPrintf("<BR>\n");
cgiMakeTextArea(hgtaPastedIdentifiers, oldPasted, 10, 70);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoPastedIdentifiers, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoClearPasteIdentifierText, "clear");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>");
htmlClose();
}

void doUploadIdentifiers(struct sqlConnection *conn)
/* Respond to upload identifiers button. */
{
struct hTableInfo *hti = hFindTableInfoDb(database, NULL, curTable);
char *idField = getIdField(database, curTrack, curTable, hti);
htmlOpen("Upload Identifiers for %s", curTableLabel());
if (idField == NULL)
    errAbort("Sorry, I can't tell which field of table %s to treat as the "
	     "identifier field.", curTable);
hPrintf("<FORM ACTION=\"..%s\" METHOD=POST ENCTYPE=\"multipart/form-data\">\n",
	getScriptName());
cartSaveSession(cart);
hPrintf("Please enter the name of a file from your computer that contains a ");
hPrintf("space, tab, or ");
hPrintf("line separated list of the items you want to include.\n");
explainIdentifiers(conn, idField);
hPrintf("<BR>\n");
hPrintf("<INPUT TYPE=FILE NAME=\"%s\"> ", hgtaPastedIdentifiers);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoPastedIdentifiers, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>");
htmlClose();
}

static void addPrimaryIdsToHash(struct sqlConnection *conn, struct hash *hash,
				char *idField, struct slName *tableList)
/* For each table in tableList, query all idField values and add to hash,
 * id -> id self-mapped so we can use the value from lookup. */
{
struct slName *table;
struct sqlResult *sr;
char **row;
char query[1024];
for (table = tableList;  table != NULL;  table = table->next)
    {
    safef(query, sizeof(query), "select %s from %s", idField, table->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (isNotEmpty(row[0]))
	    {
	    struct hashEl *hel = hashStore(hash, row[0]);
	    hel->val = hel->name;
	    }
	}
    sqlFreeResult(&sr);
    }
}

static void addXrefIdsToHash(struct sqlConnection *conn, struct hash *hash,
			     char *table, char *idField, char *aliasField,
			     struct lm *lm)
/* Query all id-alias pairs from table and hash alias -> id. */
{
struct sqlResult *sr;
char **row;
char query[1024];
safef(query, sizeof(query), "select %s,%s from %s",
      aliasField, idField, table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[0], lmCloneString(lm, row[1]));
    }
sqlFreeResult(&sr);
}

static struct hash *getAllPossibleIds(struct sqlConnection *conn,
				      struct lm *lm, char **retIdField)
/* Make a hash of all identifiers in curTable (and alias tables if specified) 
 * so that we can check the validity of pasted/uploaded identifiers. */
{
struct hash *matchHash = hashNew(20);
struct slName *tableList = strchr(curTable, '.') ? slNameNew(curTable) : 
    hSplitTableNames(curTable);
struct hTableInfo *hti = hFindTableInfoDb(database, NULL, curTable);
char *idField = NULL;
char *xrefTable = NULL, *xrefIdField = NULL, *aliasField = NULL;
idField = getIdField(database, curTrack, curTable, hti);
if (idField != NULL)
    addPrimaryIdsToHash(conn, matchHash, idField, tableList);
if (retIdField != NULL)
    *retIdField = idField;
getXrefInfo(&xrefTable, &xrefIdField, &aliasField);
if (xrefTable != NULL)
    {
    addXrefIdsToHash(conn, matchHash, xrefTable, xrefIdField, aliasField, lm);
    }
return matchHash;
}

#define MAX_IDTEXT (64 * 1024)

void doPastedIdentifiers(struct sqlConnection *conn)
/* Process submit in past identifiers page. */
{
struct hashEl *hel = NULL;
char *idText = trimSpaces(cartString(cart, hgtaPastedIdentifiers));
htmlOpen("Table Browser (Input Identifiers)");
if (isNotEmpty(idText))
    {
    /* Write terms to temp file, checking whether they have matches, and 
     * save temp file name. */
    boolean saveIdText = (strlen(idText) < MAX_IDTEXT);
    char *idTextForLf = saveIdText ? cloneString(idText) : idText;
    struct lineFile *lf = lineFileOnString("idText", TRUE, idTextForLf);
    char *line, *word;
    struct tempName tn;
    FILE *f;
    int totalTerms = 0, foundTerms = 0;
    char *exampleMiss = NULL;
    char *idField = NULL;
    struct lm *lm = lmInit(0);
    struct hash *matchHash = getAllPossibleIds(conn, lm, &idField);
    if (idField == NULL)
	{
	warn("Sorry, I can't tell which field of table %s to treat as the "
	     "identifier field.", curTable);
	webNewSection("Table Browser");
	cartRemove(cart, hgtaIdentifierTable);
	cartRemove(cart, hgtaIdentifierFile);
	mainPageAfterOpen(conn);
	htmlClose();
	return;
	}
    trashDirFile(&tn, "hgtData", "identifiers", ".key");
    f = mustOpen(tn.forCgi, "w");
    while (lineFileNext(lf, &line, NULL))
	{
	while ((word = nextWord(&line)) != NULL)
	    {
	    totalTerms++;
	    if ((hel = hashLookup(matchHash, word)) != NULL)
		{
		foundTerms++;
		mustWrite(f, (char *)hel->val, strlen((char *)hel->val));
		mustWrite(f, "\n", 1);
		/* Support multiple alias->id mappings: */
		while ((hel = hashLookupNext(hel)) != NULL)
		    {
		    mustWrite(f, (char *)hel->val, strlen((char *)hel->val));
		    mustWrite(f, "\n", 1);
		    }
		}
	    else if (exampleMiss == NULL)
		{
		exampleMiss = cloneString(word);
		}
	    }
	}
    carefulClose(&f);
    lineFileClose(&lf);
    cartSetString(cart, hgtaIdentifierTable, curTable);
    cartSetString(cart, hgtaIdentifierFile, tn.forCgi);
    if (saveIdText)
	freez(&idTextForLf);
    else
	cartRemove(cart, hgtaPastedIdentifiers);
    if (foundTerms < totalTerms)
	{
	char *xrefTable, *aliasField;
	getXrefInfo(&xrefTable, NULL, &aliasField);
	warn("Note: some of the identifiers (e.g. %s) have no match in "
	     "table %s, field %s%s%s%s%s.  "
	     "Try the \"describe table schema\" button for more "
	     "information about the table and field.",
	     exampleMiss, curTable, idField,
	     (xrefTable ? " or in alias table " : ""),
	     (xrefTable ? xrefTable : ""),
	     (xrefTable ? ", field " : ""),
	     (xrefTable ? aliasField : ""));
	webNewSection("Table Browser");
	}
    lmCleanup(&lm);
    hashFree(&matchHash);
    }
else
    {
    cartRemove(cart, hgtaIdentifierFile);
    }
mainPageAfterOpen(conn);
htmlClose();
}


char *identifierFileName()
/* File name identifiers are in, or NULL if no such file. */
{
char *fileName = cartOptionalString(cart, hgtaIdentifierFile);
char *identifierTable = cartOptionalString(cart, hgtaIdentifierTable);
if (fileName == NULL)
    return NULL;
if (identifierTable != NULL && !sameString(identifierTable, curTable) &&
    !sameString(connectingTableForTrack(identifierTable), curTable))
    return NULL;
if (fileExists(fileName))
    return fileName;
else
    {
    cartRemove(cart, hgtaIdentifierFile);
    return NULL;
    }
}

struct hash *identifierHash()
/* Return hash full of identifiers. */
{
char *fileName = identifierFileName();
if (fileName == NULL)
    return NULL;
else
    {
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct hash *hash = hashNew(18);
    char *line, *word;
    while (lineFileNext(lf, &line, NULL))
        {
	while ((word = nextWord(&line)) != NULL)
	    hashAdd(hash, word, NULL);
	}
    lineFileClose(&lf);
    return hash;
    }
}

void doClearPasteIdentifierText(struct sqlConnection *conn)
/* Respond to clear within paste identifier page. */
{
cartRemove(cart, hgtaPastedIdentifiers);
doPasteIdentifiers(conn);
}

void doClearIdentifiers(struct sqlConnection *conn)
/* Respond to clear identifiers button. */
{
char *fileName;

htmlOpen("Table Browser (Cleared Identifiers)");
fileName = identifierFileName();
if (fileName != NULL)
    remove(fileName);
cartRemove(cart, hgtaIdentifierFile);
mainPageAfterOpen(conn);
htmlClose();
}


