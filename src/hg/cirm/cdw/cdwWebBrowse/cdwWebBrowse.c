/* cdwWebBrowse - Browse CIRM data warehouse.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "portable.h"
#include "paraFetch.h"
#include "tagStorm.h"
#include "rql.h"
#include "cart.h"
#include "cdw.h"
#include "cdwLib.h"
#include "hui.h"
#include "hgColors.h"
#include "web.h"
#include "jsHelper.h"

/* Global vars */
struct cart *cart;	// User variables saved from click to click
struct hash *oldVars;	// Previous cart, before current round of CGI vars folded in

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwWebBrowse is a cgi script not meant to be run from command line.\n"
  );
}

struct rqlStatement *rqlStatementParseString(char *string)
/* Return a parsed-out RQL statement based on string */
{
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(string));
struct rqlStatement *rql = rqlStatementParse(lf);
lineFileClose(&lf);
return rql;
}

static char *lookupField(void *record, char *key)
/* Lookup a field in a tagStanza. */
{
struct tagStanza *stanza = record;
return tagFindVal(stanza, key);
}

boolean statementMatch(struct rqlStatement *rql, struct tagStanza *stanza,
	struct lm *lm)
/* Return TRUE if where clause and tableList in statement evaluates true for tdb. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, stanza, lookupField, lm);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

void rBuildStanzaRefList(struct tagStorm *tags, struct tagStanza *stanzaList,
    struct rqlStatement *rql, struct lm *lm, int *pMatchCount, struct slRef **pList)
/* Recursively add stanzas that match query to list */
{
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > *pMatchCount)
	{
	if (statementMatch(rql, stanza, lm))
	    {
	    refAdd(pList, stanza);
	    *pMatchCount += 1;
	    }
	if (stanza->children != NULL)
	    rBuildStanzaRefList(tags, stanza->children, rql, lm, pMatchCount, pList);
	}
    }
}

struct slRef *tagStanzasMatchingQuery(struct tagStorm *tags, char *query)
/* Return list of references to stanzas that match RQL query */
{
struct rqlStatement *rql = rqlStatementParseString(query);
int matchCount = 0;
struct slRef *list = NULL;
struct lm *lm = lmInit(0);
rBuildStanzaRefList(tags, tags->forest, rql, lm, &matchCount, &list);
rqlStatementFree(&rql);
lmCleanup(&lm);
return list;
}

void rTagStormCountDistinct(struct tagStanza *list, char *tag, struct hash *uniq)
/* Fill in hash with number of times have seen each value of tag */
{
char *requiredTag = "accession";
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (tagFindVal(stanza, requiredTag))
	{
	char *val = tagFindVal(stanza, tag);
	if (val != NULL)
	    {
	    hashIncInt(uniq, val);
	    }
	}
    rTagStormCountDistinct(stanza->children, tag, uniq);
    }
}

struct hash *tagCountVals(struct tagStorm *tags, char *tag)
/* Return an integer valued hash filled with counts of the
 * number of times each value is used */
{
struct hash *uniq = hashNew(0);
rTagStormCountDistinct(tags->forest, tag, uniq);
return uniq;
}

int hashElCmpIntVal(const void *va, const void *vb)
/* Compare two hashEl val with highest going first. */
{
struct hashEl *a = *((struct hashEl **)va);
struct hashEl *b = *((struct hashEl **)vb);
return b->val - a->val;
}

struct dyString *printPopularTags(struct hash *hash, int maxSize)
/* Get all hash elements, sorted by count, and print all the ones that fit */
{
maxSize -= 3;  // Leave room for ...
struct dyString *dy = dyStringNew(0);

struct hashEl *hel, *helList = hashElListHash(hash);
slSort(&helList, hashElCmpIntVal);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    int oldSize = dy->stringSize;
    if (oldSize != 0)
        dyStringAppend(dy, ", ");
    dyStringPrintf(dy, "%s (%d)", hel->name, ptToInt(hel->val));
    if (dy->stringSize >= maxSize)
        {
	dy->string[oldSize] = 0;
	dy->stringSize = oldSize;
	dyStringAppend(dy, "...");
	break;
	}
    }
hashElFreeList(&helList);
return dy;
}

long long sumCounts(struct hash *hash)
/* Figuring hash is integer valued, return sum of all vals in hash */
{
long long total = 0;
struct hashEl *hel, *helList = hashElListHash(hash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    int val = ptToInt(hel->val);
    total += val;
    }
hashElFreeList(&helList);
return total;
}


int matchCount = 0;
boolean doSelect = FALSE;

void rMatchesToRa(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list outputting matching stanzas as ra. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > matchCount)
	{
	if (stanza->children)
	    rMatchesToRa(tags, stanza->children, rql, lm);
	else    /* Just apply query to leaves */
	    {
	    if (statementMatch(rql, stanza, lm))
		{
		++matchCount;
		if (doSelect)
		    {
		    struct slName *field;
		    for (field = rql->fieldList; field != NULL; field = field->next)
			{
			char *val = tagFindVal(stanza, field->name);
			if (val != NULL)
			    printf("%s\t%s\n", field->name, val);
			}
		    printf("\n");
		    }
		}
	    }
	}
    }
}

void showMatching(char *rqlQuery, struct tagStorm *tags)
/* Show stanzas that match query */
{
struct rqlStatement *rql = rqlStatementParseString(rqlQuery);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagTreeFieldList(tags);
slSort(&allFieldList, slNameCmp);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);

/* Traverse tag tree outputting when rql statement matches in select case, just
 * updateing count in count case. */
doSelect = sameWord(rql->command, "select");
struct lm *lm = lmInit(0);
rMatchesToRa(tags, tags->forest, rql, lm);
if (sameWord(rql->command, "count"))
    printf("%d\n", matchCount);

}

int labCount(struct tagStorm *tags)
/* Return number of different labs in tags */
{
struct hash *hash = tagCountVals(tags, "lab");
int count = hash->elCount;
hashFree(&hash);
return count;
}

void showTableFromQuery(struct tagStorm *tags, char *fields, char *where,  int limit,
    char *filtersVar)
/* Construct query and display results as a table */
{
struct dyString *rqlQuery = dyStringNew(0);
struct slName *field, *fieldList = commaSepToSlNames(fields);

dyStringPrintf(rqlQuery, "select %s from files where (%s)", fields, where);
if (filtersVar)
    {
    for (field = fieldList; field != NULL; field = field->next)
        {
	char varName[128];
	safef(varName, sizeof(varName), "%s_%s", filtersVar, field->name);
	char *val = trimSpaces(cartUsualString(cart, varName, ""));
	if (!isEmpty(val))
	    {
	    dyStringPrintf(rqlQuery, " and ");
	    if (anyWild(val))
	         {
		 char *converted = sqlLikeFromWild(val);
		 char *escaped = makeEscapedString(converted, '"');
		 dyStringPrintf(rqlQuery, "%s like \"%s\"", field->name, escaped);
		 freez(&escaped);
		 freez(&converted);
		 }
	    else if (val[0] == '>' || val[0] == '<')
	         {
		 dyStringPrintf(rqlQuery, "%s and %s %s", field->name, field->name, val);
		 }
	    else
	         {
		 char *escaped = makeEscapedString(val, '"');
		 dyStringPrintf(rqlQuery, "%s = \"%s\"", field->name, escaped);
		 freez(&escaped);
		 }
	    }
	}
    }

uglyf("query: %s<BR>\n", rqlQuery->string);
/* Turn rqlQuery string into a parsed out rqlStatement. */
struct slRef *refList = tagStanzasMatchingQuery(tags, rqlQuery->string);

/* Set up our table within table look. */
if (filtersVar)
    {
    printf("First row of table below, above labels, can be used to filter results. ");    
    printf("Wildcard * and ? characters are allowed in text fields. ");
    printf("&GT;min or &LT;max, is allowed in numerical fields. ");
    cgiMakeButton("submit", "update");
    printf(" %d pass filter", slCount(refList));
    }

webPrintLinkTableStart();

if (filtersVar)
    {
    char varName[256];
    printf("<TR>");
    for (field = fieldList; field != NULL; field = field->next)
        {
	safef(varName, sizeof(varName), "%s_%s", filtersVar, field->name);
	webPrintLinkCellStart();
	cartMakeTextVar(cart, varName, "", 12);
	webPrintLinkCellEnd();
	}
    printf("</TR>");
    }

/* Print column labels */
for (field = fieldList; field != NULL; field = field->next)
    webPrintLabelCell(field->name);

struct slRef *ref;
int count = 0;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    if (++count > limit)
         break;
    struct tagStanza *stanza = ref->val;
    printf("<TR>\n");
    struct slName *field;
    for (field = fieldList; field != NULL; field = field->next)
	{
	char *val = emptyForNull(tagFindVal(stanza, field->name));
	int valLen = strlen(val);
	int maxLen = 18;
	char shortVal[maxLen+1];
	if (valLen > maxLen)
	    {
	    memcpy(shortVal, val, maxLen-3);
	    shortVal[maxLen-3] = 0;
	    strcat(shortVal, "...");
	    }
	else
	    strcpy(shortVal, val);
	webPrintLinkCellStart();
	printf("%s", shortVal);
	webPrintLinkCellEnd();
	}
    printf("</TR>\n");
    }

/* Get rid of table within table look */
webPrintLinkTableEnd();
}


void doBrowseFiles(struct sqlConnection *conn)
/* Print list of files */
{
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "browseFiles");
struct tagStorm *tags = cdwTagStorm(conn);
showTableFromQuery(tags, 
    "file_name,lab,assay,data_set_id,format,paired_end_mate,read_size,item_count,map_ratio,"
    "species,lab_quake_markers,body_part",
    "file_name", 200, "cdwFilter");
printf("</FORM>\n");
}

void doBrowsePopular(struct sqlConnection *conn, char *tag)
/* Print list of most popular tags of type */
{
struct tagStorm *tags = cdwTagStorm(conn);
struct hash *hash = tagCountVals(tags, tag);
printf("most popular %s values\n", tag);
struct hashEl *hel, *helList = hashElListHash(hash);
slSort(&helList, hashElCmpIntVal);
webPrintLinkTableStart();
webPrintLabelCell(tag);
webPrintLabelCell("matching files");
int valIx = 0, maxValIx = 10;
for (hel = helList; hel != NULL && valIx < maxValIx; hel = hel->next, ++maxValIx)
    {
    printf("<TR>\n");
    webPrintLinkCell(hel->name);
    webPrintIntCell(ptToInt(hel->val));
    printf("</TR>\n");
    }
webPrintLinkTableEnd();
}

void doBrowseLab(struct sqlConnection *conn)
/* Put up information on labs */
{
struct tagStorm *tags = cdwTagStorm(conn);
printf("Here is a table of labs that have contributed data<BR>\n");
webPrintLinkTableStart();
webPrintLabelCell("name");
webPrintLabelCell("#files");
webPrintLabelCell("PI");
webPrintLabelCell("institution");
webPrintLabelCell("web page");
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwLab");
struct cdwLab *lab, *labList = cdwLabLoadByQuery(conn, query);
for (lab = labList; lab != NULL; lab = lab->next)
    {
    printf("<TR>\n");
    webPrintLinkCell(lab->name);
    char rqlQuery[256];
    safef(rqlQuery, sizeof(rqlQuery), "select * from files where file_name and lab='%s'", 
	lab->name);
    struct slRef *labFileList = tagStanzasMatchingQuery(tags, rqlQuery);
    int fileCount = slCount(labFileList);
    slFreeList(&labFileList);
    webPrintIntCell(fileCount);
    webPrintLinkCell(lab->pi);
    webPrintLinkCell(lab->institution);
    webPrintLinkCellStart();
    printf("<A HREF=\"%s\">%s</A>", lab->url, lab->url);
    webPrintLinkCellEnd();
    printf("</TR>\n");
    }
webPrintLinkTableEnd();
}


void doSearch(struct sqlConnection *conn)
/* Print up search page */
{
printf("Enter a SQL query below using  'file' for the table name.<BR><BR>\n");
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "query");
printf("query: ");
char *queryVar = "cdwWebBrowse.query";
char *query = cartUsualString(cart, queryVar, "select * from x where file_name limit 3");
cgiMakeTextVar(queryVar, query, 80);
cgiMakeSubmitButton();


printf("<PRE><TT>\n");
struct tagStorm *tags = cdwTagStorm(conn);
showMatching(query, tags);
printf("</TT></PRE>\n");
printf("</FORM>\n");
}

void tagSummaryRow(struct tagStorm *tags, char *tag)
/* Print out row in a high level tag counting table */
{
printf("<TR>");
struct hash *hash = tagCountVals(tags, tag);
int count = hash->elCount;
struct dyString *dy = printPopularTags(hash, 120);
webPrintLinkCell(tag);
webPrintIntCell(count);
webPrintLinkCell(dy->string);
int fileCount = sumCounts(hash);
webPrintIntCell(fileCount);
dyStringFree(&dy);
hashFree(&hash);
printf("</TR>");
}

void doHome(struct sqlConnection *conn)
/* Put up home/summary page */
{
static char *highLevelTags[] = 
    {"data_set_id", "lab", "assay", "format", "read_size",
    "body_part", "submit_dir", "lab_quake_markers", "species"};

struct tagStorm *tags = cdwTagStorm(conn);
char query[256];
sqlSafef(query, sizeof(query), "select count(*) from cdwValidFile");
printf("The CIRM Stem Cell Hub contains ");
long long fileCount = sqlQuickLongLong(conn, query);
printLongWithCommas(stdout, fileCount);
printf(" files\n");
sqlSafef(query, sizeof(query),
    "select sum(size) from cdwFile,cdwValidFile where cdwFile.id=cdwValidFile.id");
long long totalBytes = sqlQuickLongLong(conn, query);
printf("and ");
printLongWithCommas(stdout, totalBytes);
printf(" bytes of data from %d labs.<BR><BR>\n", labCount(tags));
printf("This table is a summary of important tags and the number of files associated with each. ");
printf("For a full table of all tags select Browse Tags from the menus.");
webPrintLinkTableStart();
webPrintLabelCell("tag name");
webPrintLabelCell("# val");
webPrintLabelCell("popular values (files)...");
webPrintLabelCell("# files");
int i;
for (i=0; i<ArraySize(highLevelTags); ++i)
    tagSummaryRow(tags, highLevelTags[i]);
webPrintLinkTableEnd();
}

void doBrowseTags(struct sqlConnection *conn)
/* Put up browse tags page */
{
struct tagStorm *tags = cdwTagStorm(conn);
struct slName *tag, *tagList = tagTreeFieldList(tags);
slSort(&tagList, slNameCmp);
printf("This is a list of all tags and their most popular values.");
webPrintLinkTableStart();
webPrintLabelCell("tag name");
webPrintLabelCell("# val");
webPrintLabelCell("popular values (files)...");
webPrintLabelCell("# files");
for (tag = tagList; tag != NULL; tag = tag->next)
    tagSummaryRow(tags, tag->name);
webPrintLinkTableEnd();
tagStormFree(&tags);
}

void doHelp(struct sqlConnection *con)
/* Put up help page */
{
printf("This being a prototype, there's not much help available.  Try clicking and hovering over the menu bar.");
printf("The search option has you type in a SQL-like query. Try 'select * from x where file_name' to get all data.\n");
}


void dispatch(struct sqlConnection *conn)
/* Dispatch page after to routine depending on cdwCommand variable */
{
char *command = cartOptionalString(cart, "cdwCommand");
if (command == NULL)
    {
    doHome(conn);
    }
else if (sameString(command, "query"))
    {
    doSearch(conn);
    }
else if (sameString(command, "browseFiles"))
    {
    doBrowseFiles(conn);
    }
else if (sameString(command, "browseTags"))
    {
    doBrowseTags(conn);
    }
else if (sameString(command, "browseLabs"))
    {
    doBrowseLab(conn);
    }
else if (sameString(command, "browseDataSets"))
    {
    doBrowsePopular(conn, "data_set_id");
    }
else if (sameString(command, "browseFormats"))
    {
    doBrowsePopular(conn, "format");
    }
else if (sameString(command, "help"))
    {
    doHelp(conn);
    }
else
    {
    uglyf("unrecognized command %s<BR>\n", command);
    }
}

void doMiddle()
/* Menu bar has been drawn.  We are in the middle of first section. */
{
struct sqlConnection *conn = sqlConnect(cdwDatabase);
dispatch(conn);
sqlDisconnect(&conn);
}

static char *localMenuBar()
/* Return menu bar string */
{
// menu bar html is in a stringified .h file
char *rawHtml = 
#include "cdwNavBar.h"
   ;

char uiVars[128];
safef(uiVars, sizeof(uiVars), "%s=%s", cartSessionVarName(), cartSessionId(cart));
return menuBarAddUiVars(rawHtml, "/cgi-bin/cdw", uiVars);
}

void localWebStartWrapper(char *titleString)
/* Output a HTML header with the given title.  Start table layout.  Draw menu bar. */
{
/* Do html header. We do this a little differently than web.c routines, mostly
 * in that we are strict rather than transitional HTML 4.01 */
    {
    puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
             "\"http://www.w3.org/TR/html4/strict.dtd\">");
    puts("<HTML><HEAD>\n");
    webPragmasEtc();
    printf("<TITLE>%s</TITLE>\n", titleString);
    webIncludeResourceFile("HGStyle.css");
    jsIncludeFile("jquery.js", NULL);
    jsIncludeFile("jquery.plugins.js", NULL);
    webIncludeResourceFile("nice_menu.css");
    printf("</HEAD>\n");
    printBodyTag(stdout);
    }

webStartSectionTables();    // Start table layout code
puts(localMenuBar());	    // Menu bar after tables open but before first section
webFirstSection(titleString);	// Open first section
webPushErrHandlers();	    // Now can do improved error handler
}


void localWebWrap(struct cart *theCart)
/* We got the http stuff handled, and a cart.  Now wrap a web page around it. */
{
cart = theCart;
localWebStartWrapper("CIRM Stem Cell Hub Browser V0.07");
pushWarnHandler(htmlVaWarn);
doMiddle();
webEndSectionTables();
printf("</BODY></HTML>\n");
}


char *excludeVars[] = {"cdwCommand", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
dnaUtilOpen();
oldVars = hashNew(0);
cartEmptyShell(localWebWrap, hUserCookie(), excludeVars, oldVars);
return 0;
}
