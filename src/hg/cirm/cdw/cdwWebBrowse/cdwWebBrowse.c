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
#include "cdwValid.h"
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

struct dyString *printPopularTags(struct hash *hash, int maxSize)
/* Get all hash elements, sorted by count, and print all the ones that fit */
{
maxSize -= 3;  // Leave room for ...
struct dyString *dy = dyStringNew(0);

struct hashEl *hel, *helList = hashElListHash(hash);
slSort(&helList, hashElCmpIntValDesc);
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


static int matchCount = 0;
static boolean doSelect = FALSE;

static void rMatchesToRa(struct tagStorm *tags, struct tagStanza *list, 
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
	    if (cdwRqlStatementMatch(rql, stanza, lm))
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

void showMatching(char *rqlQuery, int limit, struct tagStorm *tags)
/* Show stanzas that match query */
{
struct dyString *dy = dyStringCreate("%s", rqlQuery);
int maxLimit = 10000;
if (limit > maxLimit)
    limit = maxLimit;
dyStringPrintf(dy, " limit %d", limit);
struct rqlStatement *rql = rqlStatementParseString(dy->string);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagStormFieldList(tags);
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
struct hash *hash = tagStormCountTagVals(tags, "lab");
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
struct hash *hash = tagStormCountTagVals(tags, tag);
printf("%s tag values ordered by usage\n", tag);
struct hashEl *hel, *helList = hashElListHash(hash);
slSort(&helList, hashElCmpIntValDesc);
webPrintLinkTableStart();
webPrintLabelCell("#");
webPrintLabelCell(tag);
webPrintLabelCell("matching files");
int valIx = 0, maxValIx = 100;
for (hel = helList; hel != NULL && ++valIx <= maxValIx; hel = hel->next)
    {
    printf("<TR>\n");
    webPrintIntCell(valIx);
    webPrintLinkCell(hel->name);
    webPrintIntCell(ptToInt(hel->val));
    printf("</TR>\n");
    }
webPrintLinkTableEnd();
}

void doBrowseFormat(struct sqlConnection *conn)
/* Browse through available formats */
{
struct tagStorm *tags = cdwTagStorm(conn);
struct hash *countHash = tagStormCountTagVals(tags, "format");
struct slPair *format, *formatList = cdwFormatList();
struct hash *formatHash = hashNew(0);
for (format = formatList; format != NULL; format = format->next)
    hashAdd(formatHash, format->name, format->val);

printf("file formats ordered by usage\n");
webPrintLinkTableStart();
webPrintLabelCell("count");
webPrintLabelCell("format");
webPrintLabelCell("description");
struct hashEl *hel, *helList = hashElListHash(countHash);
slSort(&helList, hashElCmpIntValDesc);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    printf("<TR>\n");
    webPrintIntCell(ptToInt(hel->val));
    webPrintLinkCell(hel->name);
    webPrintLinkCell(emptyForNull(hashFindVal(formatHash, hel->name)));
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

void doQuery(struct sqlConnection *conn)
/* Print up query page */
{
printf("Enter a SQL query below using  'file' for the table name.<BR><BR>\n");
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "query");
printf("query: ");
char *queryVar = "cdwQuerySql";
char *query = cartUsualString(cart, queryVar, "select * from files where md5");
cgiMakeTextVar(queryVar, query, 80);
printf(" limit: ");
char *limitVar = "cdwQueryLimit";
int limit = cartUsualInt(cart, limitVar, 10);
cgiMakeIntVar(limitVar, limit, 7);
cgiMakeSubmitButton();


printf("<PRE><TT>\n");
struct tagStorm *tags = cdwTagStorm(conn);
showMatching(query, limit, tags);
printf("</TT></PRE>\n");
printf("</FORM>\n");
}

void tagSummaryRow(struct tagStorm *tags, char *tag)
/* Print out row in a high level tag counting table */
{
printf("<TR>");
struct hash *hash = tagStormCountTagVals(tags, tag);
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
struct tagStorm *tags = cdwTagStorm(conn);

/* Print sentence with summary of bytes, files, and labs */
char query[256];
printf("The CIRM Stem Cell Hub contains ");
sqlSafef(query, sizeof(query),
    "select sum(size) from cdwFile,cdwValidFile where cdwFile.id=cdwValidFile.id");
long long totalBytes = sqlQuickLongLong(conn, query);
printLongWithCommas(stdout, totalBytes);
printf(" bytes of data in ");
sqlSafef(query, sizeof(query), "select count(*) from cdwValidFile");
long long fileCount = sqlQuickLongLong(conn, query);
printLongWithCommas(stdout, fileCount);
printf(" files");
printf(" from %d labs.<BR><BR>\n", labCount(tags));

/* Print out high level tags table */
static char *highLevelTags[] = 
    {"data_set_id", "lab", "assay", "format", "read_size",
    "body_part", "submit_dir", "lab_quake_markers", "species"};
printf("This table is a summary of important metadata tags and the number of files associated ");
printf("with each. ");
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
struct slName *tag, *tagList = tagStormFieldList(tags);
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
printf("This being a prototype, there's not much help available.  Try clicking and hovering over the menu bar. ");
printf("The query option has you type in a SQL-like query. Try 'select * from files where accession' to get all metadata tags from files that have passed basic format validations.\n");
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
    doQuery(conn);
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
    doBrowseFormat(conn);
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
localWebStartWrapper("CIRM Stem Cell Hub Browser V0.08");
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
