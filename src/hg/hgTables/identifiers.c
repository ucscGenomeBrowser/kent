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
#include "wikiTrack.h"
#include "htmshell.h"

static char const rcsid[] = "$Id: identifiers.c,v 1.23.10.2 2008/08/02 04:06:24 markd Exp $";


static boolean forCurTable()
/* Return TRUE if cart Identifier stuff is for curTable. */
{
char *identifierDb = cartOptionalString(cart, hgtaIdentifierDb);
char *identifierTable = cartOptionalString(cart, hgtaIdentifierTable);

return (identifierDb && identifierTable &&
	sameString(identifierDb, database) &&
	(sameString(identifierTable, curTable) ||
	 sameString(connectingTableForTrack(identifierTable), curTable)));
}

static void getXrefInfo(struct sqlConnection *conn,
			char **retXrefTable, char **retIdField,
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
    if (!sqlTableExists(conn, xrefTable) ||
	sqlFieldIndex(conn, xrefTable, idField) < 0 ||
	sqlFieldIndex(conn, xrefTable, aliasField) < 0)
	xrefTable = idField = aliasField = NULL;
    }
if (retXrefTable != NULL)
    *retXrefTable = xrefTable;
if (retIdField != NULL)
    *retIdField = idField;
if (retAliasField != NULL)
    *retAliasField = aliasField;
}

static struct slName *getExamples(struct sqlConnection *conn,
				  char *table, char *field, int count)
/* Return a list of several example values of table.field. */
{
char fullTable[HDB_MAX_TABLE_STRING];
if (! hFindSplitTable(database, NULL, table, fullTable, NULL))
    safef(fullTable, sizeof(fullTable), table);
return sqlRandomSampleConn(conn, fullTable, field, count);
}

static void explainIdentifiers(struct sqlConnection *conn, char *idField)
/* Tell the user what field(s) they may paste/upload values for, and give 
 * some examples. */
{
char *xrefTable = NULL, *xrefIdField = NULL, *aliasField = NULL;
getXrefInfo(conn, &xrefTable, &xrefIdField, &aliasField);
hPrintf("The items must be values of the <B>%s</B> field of the currently "
	"selected table, <B>%s</B>",
	idField, curTable);
if (aliasField != NULL)
    {
    if (sameString(curTable, xrefTable))
	hPrintf(", or the <B>%s</B> field.\n", aliasField);
    else
	hPrintf(", or the <B>%s</B> field of the alias table <B>%s</B>.\n",
		aliasField, xrefTable);
    }
else
    hPrintf(".\n");
hPrintf("(The \"describe table schema\" button shows more information about "
	"the table fields.)\n");
if (!isCustomTrack(curTable))
    {
    struct slName *exampleList = NULL, *ex;
    hPrintf("Some example values:<BR>\n");
    exampleList = getExamples(conn, curTable, idField, 3);
    for (ex = exampleList;  ex != NULL;  ex = ex->next)
	{
	char *tmp = htmlEncode(ex->name);
	hPrintf("<TT>%s</TT><BR>\n", tmp);
	freeMem(tmp);
	}
    if (aliasField != NULL)
	{
	char tmpTable[512];
	char query[2048];
	safef(tmpTable, sizeof(tmpTable), "tmp%s%s", curTable, xrefTable);
	safef(query, sizeof(query),
	      "create temporary table %s "
	      "select %s.%s as %s from %s,%s "
	      "where %s.%s = %s.%s and %s.%s != %s.%s limit 100000",
	      tmpTable, xrefTable, aliasField, aliasField, xrefTable, curTable,
	      xrefTable, xrefIdField, curTable, idField,
	      xrefTable, xrefIdField, xrefTable, aliasField);
	sqlUpdate(conn, query);
	exampleList = getExamples(conn, tmpTable, aliasField, 3);
	for (ex = exampleList;  ex != NULL;  ex = ex->next)
	    hPrintf("<TT>%s</TT><BR>\n", ex->name);
	}
    hPrintf("\n");
    }
}

void doPasteIdentifiers(struct sqlConnection *conn)
/* Respond to paste identifiers button. */
{
struct sqlConnection *alternateConn = conn;
char *actualDb = database;
if (sameWord(curTable, WIKI_TRACK_TABLE))
    {
    alternateConn = wikiConnect();
    actualDb = wikiDbName();
    }

char *oldPasted = forCurTable() ?
    cartUsualString(cart, hgtaPastedIdentifiers, "") : "";
struct hTableInfo *hti = maybeGetHti(actualDb, curTable);
char *idField = getIdField(actualDb, curTrack, curTable, hti);
htmlOpen("Paste In Identifiers for %s", curTableLabel());
if (idField == NULL)
    errAbort("Sorry, I can't tell which field of table %s to treat as the "
	     "identifier field.", curTable);
hPrintf("<FORM ACTION=\"%s\" METHOD=%s>\n", getScriptName(),
	cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
hPrintf("Please paste in the identifiers you want to include.\n");
explainIdentifiers(alternateConn, idField);
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
if (sameWord(curTable, WIKI_TRACK_TABLE))
    wikiDisconnect(&alternateConn);
}

void doUploadIdentifiers(struct sqlConnection *conn)
/* Respond to upload identifiers button. */
{
struct hTableInfo *hti = maybeGetHti(database, curTable);
char *idField = getIdField(database, curTrack, curTable, hti);
htmlOpen("Upload Identifiers for %s", curTableLabel());
if (idField == NULL)
    errAbort("Sorry, I can't tell which field of table %s to treat as the "
	     "identifier field.", curTable);
hPrintf("<FORM ACTION=\"%s\" METHOD=POST ENCTYPE=\"multipart/form-data\">\n",
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
				char *idField, struct slName *tableList,
				struct lm *lm)
/* For each table in tableList, query all idField values and add to hash,
 * id -> uppercased id for case-insensitive matching. */
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
	    char *origCase = lmCloneString(lm, row[0]);
	    touppers(row[0]);
	    hashAdd(hash, row[0], origCase);
	    }
	}
    sqlFreeResult(&sr);
    }
}

static void addXrefIdsToHash(struct sqlConnection *conn, struct hash *hash,
			     char *idField, char *xrefTable, char *xrefIdField,
			     char *aliasField, struct lm *lm)
/* Query all id-alias pairs from xrefTable (where id actually appears
 * in curTable) and hash alias -> id.  Convert alias to upper case for
 * case-insensitive matching.
 * Ignore self (alias = id) mappings -- we already got those above. */
{
struct sqlResult *sr;
char **row;
char query[1024];
if (sameString(xrefTable, curTable))
    safef(query, sizeof(query), "select %s,%s from %s",
      aliasField, xrefIdField, xrefTable);
else
    /* Get only the aliases for items actually in curTable.idField: */
    safef(query, sizeof(query),
	  "select %s.%s,%s.%s from %s,%s where %s.%s = %s.%s",
	  xrefTable, aliasField, xrefTable, xrefIdField,
	  xrefTable, curTable,
	  xrefTable, xrefIdField, curTable, idField);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], row[1]))
	continue;
    touppers(row[0]);
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
struct slName *tableList;
struct hTableInfo *hti = maybeGetHti(database, curTable);
char *idField = NULL;
char *xrefTable = NULL, *xrefIdField = NULL, *aliasField = NULL;
char *actualDb = database;
struct sqlConnection *alternateConn = conn;

if (sameWord(curTable, WIKI_TRACK_TABLE))
    {
    actualDb = wikiDbName();
    alternateConn = wikiConnect();
    }

if (isCustomTrack(curTable))
    /* Currently we don't check whether these are valid CT item
     * names or not.  matchHash is empty for CTs. */
    tableList = NULL;
else if (sameWord(curTable, WIKI_TRACK_TABLE))
    tableList = slNameNew(WIKI_TRACK_TABLE);
else if (strchr(curTable, '.'))
    tableList = slNameNew(curTable);
else
    tableList = hSplitTableNames(database, curTable);
idField = getIdField(actualDb, curTrack, curTable, hti);
if (idField != NULL)
    addPrimaryIdsToHash(alternateConn, matchHash, idField, tableList, lm);
if (retIdField != NULL)
    *retIdField = idField;
getXrefInfo(alternateConn, &xrefTable, &xrefIdField, &aliasField);
if (xrefTable != NULL)
    {
    addXrefIdsToHash(alternateConn, matchHash, idField,
		     xrefTable, xrefIdField, aliasField, lm);
    }
if (sameWord(curTable, WIKI_TRACK_TABLE))
    wikiDisconnect(&alternateConn);
return matchHash;
}

#define MAX_IDTEXT (64 * 1024)

void doPastedIdentifiers(struct sqlConnection *conn)
/* Process submit in paste identifiers page. */
{
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
	cartRemove(cart, hgtaIdentifierDb);
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
	    struct slName *matchList = NULL, *match;
	    totalTerms++;
	    if (isCustomTrack(curTable))
		{
		/* Currently we don't check whether these are valid CT item
		 * names or not.  matchHash is empty for CTs. */
		matchList = slNameNew(word);
		}
	    else
		{
		/* Support multiple alias->id mappings: */
		char upcased[1024];
		safecpy(upcased, sizeof(upcased), word);
		touppers(upcased);
		struct hashEl *hel = hashLookup(matchHash, upcased);
		if (hel != NULL)
		    {
		    matchList = slNameNew((char *)hel->val);
		    while ((hel = hashLookupNext(hel)) != NULL)
			{
			match = slNameNew((char *)hel->val);
			slAddHead(&matchList, match);
			}
		    }
		}
	    if (matchList != NULL)
		{
		foundTerms++;
		for (match = matchList;  match != NULL;  match = match->next)
		    {
		    mustWrite(f, match->name, strlen(match->name));
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
    cartSetString(cart, hgtaIdentifierDb, database);
    cartSetString(cart, hgtaIdentifierTable, curTable);
    cartSetString(cart, hgtaIdentifierFile, tn.forCgi);
    if (saveIdText)
	freez(&idTextForLf);
    else
	cartRemove(cart, hgtaPastedIdentifiers);
    if (foundTerms < totalTerms)
	{
	char *xrefTable, *aliasField;
	getXrefInfo(conn, &xrefTable, NULL, &aliasField);
	boolean xrefIsSame = xrefTable && sameString(curTable, xrefTable);
	warn("Note: %d of the %d given identifiers (e.g. %s) have no match in "
	     "table %s, field %s%s%s%s%s.  "
	     "Try the \"describe table schema\" button for more "
	     "information about the table and field.",
	     (totalTerms - foundTerms), totalTerms,
	     exampleMiss, curTable, idField,
	     (xrefTable ? (xrefIsSame ? "" : " or in alias table ") : ""),
	     (xrefTable ? (xrefIsSame ? "" : xrefTable) : ""),
	     (xrefTable ? (xrefIsSame ? " or in field " : ", field ") : ""),
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
/* File name identifiers are in, or NULL if not for curTable or no such file. */
{
char *fileName = cartOptionalString(cart, hgtaIdentifierFile);
if (fileName == NULL)
    return NULL;
if (! forCurTable())
    return NULL;
if (fileExists(fileName))
    return fileName;
else
    {
    cartRemove(cart, hgtaIdentifierFile);
    return NULL;
    }
}

struct hash *identifierHash(char *db, char *table)
/* Return hash full of identifiers from the given table (or NULL). */
{
char dbDotTable[2048];
if (sameString(table, WIKI_TRACK_TABLE))
    safecpy(dbDotTable, sizeof(dbDotTable), table);
else if (!sameString(db, database))
    safef(dbDotTable, sizeof(dbDotTable), "%s.%s", db, table);
else
    safecpy(dbDotTable, sizeof(dbDotTable), table);
if (! (sameString(dbDotTable, curTable) ||
       sameString(connectingTableForTrack(dbDotTable), curTable)) )
    return NULL;
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
cartRemove(cart, hgtaPastedIdentifiers);
mainPageAfterOpen(conn);
htmlClose();
}


