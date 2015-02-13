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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwWebBrowse is a cgi script not meant to be run from command line.\n"
  );
}

struct cart *cart;
struct hash *oldVars;

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

void highLevelSummary(struct sqlConnection *conn, struct tagStorm *tags,
    char *tagArray[], int tagArraySize)
{
int i;
for (i=0; i<tagArraySize; ++i)
    {
    char *tag = tagArray[i];
    struct hash *hash = tagCountVals(tags, tag);
    int count = hash->elCount;
    struct dyString *dy = printPopularTags(hash, 120);
    printf("<B>%s (%d):</B>\t%s\n", tag, count, dy->string);
    dyStringFree(&dy);
    hashFree(&hash);
    }
}

char *highLevelTags[] = {"file_name", "lab", "meta", "seq_sample", "lab_quake_sample", "format", 
    "body_part", "submit_dir", "lab_quake_fluidics_cell", "lab_quake_markers", "species"};

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

int matchCount = 0;
boolean doSelect = FALSE;

void traverse(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > matchCount)
	{
	if (stanza->children)
	    traverse(tags, stanza->children, rql, lm);
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
/* Turn rqlQuery string into a parsed out rqlStatement. */
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(rqlQuery));
struct rqlStatement *rql = rqlStatementParse(lf);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagTreeFieldList(tags);
slSort(&allFieldList, slNameCmp);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);

/* Traverse tag tree outputting when rql statement matches in select case, just
 * updateing count in count case. */
doSelect = sameWord(rql->command, "select");
struct lm *lm = lmInit(0);
traverse(tags, tags->forest, rql, lm);
if (sameWord(rql->command, "count"))
    printf("%d\n", matchCount);

}

void browseCdw(struct sqlConnection *conn)
/* Show some overall information about cdw */
{
struct tagStorm *tags = cdwTagStorm(conn);
uglyf("BROWSING CDW 9!<BR>\n");
printf("<PRE><TT>\n");
highLevelSummary(conn, tags, highLevelTags, ArraySize(highLevelTags));
printf("</TT></PRE>\n");
printf("query: ");
char *queryVar = "cdwWebBrowse.query";
char *query = cartUsualString(cart, queryVar, "select * from x where file_name limit 3");
cgiMakeTextVar("cdwWebBrowse.query", query, 80);
cgiMakeSubmitButton();


printf("<PRE><TT>\n");
showMatching(query, tags);
printf("</TT></PRE>\n");
}

void doHome(struct sqlConnection *conn)
/* Print home page message.  Welcome user, provide brief instructions and hi level stats. */
{
char query[256];
sqlSafef(query, sizeof(query), "select count(*) from cdwValidFile");
printf("The CIRM Stem Cell Hub contains %d files\n", sqlQuickNum(conn, query));
sqlSafef(query, sizeof(query),
    "select sum(size) from cdwFile,cdwValidFile where cdwFile.id=cdwValidFile.id");
long long totalBytes = sqlQuickLongLong(conn, query);
printf("and ");
printLongWithCommas(stdout, totalBytes);
printf(" bytes of data.");
}

void doSearch(struct sqlConnection *conn)
/* Print up search page */
{
printf("query: ");
char *queryVar = "cdwWebBrowse.query";
char *query = cartUsualString(cart, queryVar, "select * where file_name limit 10");
cgiMakeTextVar(queryVar, query, 80);
cgiMakeSubmitButton();


printf("<PRE><TT>\n");
struct tagStorm *tags = cdwTagStorm(conn);
showMatching(query, tags);
printf("</TT></PRE>\n");
}

void doSummary(struct sqlConnection *conn)
{
struct tagStorm *tags = cdwTagStorm(conn);
printf("<PRE><TT>\n");
highLevelSummary(conn, tags, highLevelTags, ArraySize(highLevelTags));
printf("</TT></PRE>\n");
}


void dispatch(struct sqlConnection *conn)
/* Dispatch page after the menu bar to routine depending on cdwCommand variable */
{
char *command = cartOptionalString(cart, "cdwCommand");
if (command == NULL)
    {
    doHome(conn);
    }
else if (sameString(command, "search"))
    {
    doSearch(conn);
    }
else if (sameString(command, "summary"))
    {
    doSummary(conn);
    }
else
    {
    uglyf("unrecognized command %s<BR>\n", command);
    browseCdw(conn);
    }
}

void doMiddle()
/* Write what goes between BODY and /BODY */
{
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
struct sqlConnection *conn = sqlConnect(cdwDatabase);
dispatch(conn);
sqlDisconnect(&conn);
printf("</FORM>\n");
}

static char *localMenuBar()
/* Return menu bar string and also make sure that all the javascript and css that
 * menu bar needs are in place. */
{
// menu bar html is in a stringified .h file
char *rawHtml = 
#include "cdwNavBar.h"
   ;

char uiVars[128];
safef(uiVars, sizeof(uiVars), "%s=%s", cartSessionVarName(), cartSessionId(cart));
return menuBarAddUiVars(rawHtml, "/cgi-bin/cdw", uiVars);
}

static void webStartWrapperDetailedInternal(char *title)
/* output a CGI and HTML header with the given title in printf format */
{
/* Print out <!DOCTYPE> <HTML> <HEAD> ... </HEAD> */
    {
    puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
             "\"http://www.w3.org/TR/html4/strict.dtd\">");
    puts(
	"<HTML>" "\n"
	"<HEAD>" "\n"
	);
    printf("\t<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">" "\n"
	 "\t<META http-equiv=\"Content-Script-Type\" content=\"text/javascript\">" "\n"
         "\t<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">" "\n"
         "\t<META HTTP-EQUIV=\"Expires\" CONTENT=\"-1\">" "\n"
	 "\t<TITLE>"
	 );
    htmlTextOut(title);
    printf("	</TITLE>\n    ");
    jsIncludeFile("jquery.js", NULL);
    jsIncludeFile("jquery.plugins.js", NULL);
    jsIncludeFile("utils.js", NULL);
    jsIncludeFile("ajax.js", NULL);
    webIncludeResourceFile("nice_menu.css");
    webIncludeResourceFile("HGStyle.css");
    printf("</HEAD>\n");
    printBodyTag(stdout);
    htmlWarnBoxSetup(stdout);// Sets up a warning box which can be filled with errors as they occur
    puts(commonCssStyles());
    }


/* Put up the hot links bar. */

puts(
    "<A NAME=\"TOP\"></A>" "\n"
    "" "\n"
    "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH=\"100%\">" "\n");

char *menuStr = localMenuBar();
puts(menuStr);


/* this HTML must be in calling code if skipSectionHeader is TRUE */
    {
    puts( // TODO: Replace nested tables with CSS (difficulty is that tables are closed elsewhere)
         "<!-- +++++++++++++++++++++ CONTENT TABLES +++++++++++++++++++ -->" "\n"
         "<TR><TD COLSPAN=3>\n"
         "<div id=firstSection>"
         "      <!--outer table is for border purposes-->\n"
         "      <TABLE WIDTH='100%' BGCOLOR='#" HG_COL_BORDER "' BORDER='0' CELLSPACING='0' "
                     "CELLPADDING='1'><TR><TD>\n"
         "    <TABLE BGCOLOR='#" HG_COL_INSIDE "' WIDTH='100%'  BORDER='0' CELLSPACING='0' "
                     "CELLPADDING='0'><TR><TD>\n"
         "     <div class='subheadingBar'><div class='windowSize' id='sectTtl'>"
         );
    htmlTextOut(title);

    puts("     </div></div>\n"
         "     <TABLE BGCOLOR='#" HG_COL_INSIDE "' WIDTH='100%' CELLPADDING=0>"
              "<TR><TH HEIGHT=10></TH></TR>\n"
         "     <TR><TD WIDTH=10>&nbsp;</TD><TD>\n\n"
         );
    }
webPushErrHandlers();
/* set the flag */
}	/*	static void webStartWrapperDetailedInternal()	*/


void localWebWrap()
{
webStartWrapperDetailedInternal("CIRM Stem Cell Hub Browser V3");
pushWarnHandler(htmlVaWarn);
doMiddle();
webEndSectionTables();
printf("</BODY></HTML>\n");
}

void doAfterCart(struct cart *theCart)
{
cart = theCart;
localWebWrap();
}

char *excludeVars[] = {"cdwCommand", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
dnaUtilOpen();
oldVars = hashNew(0);
cartEmptyShell(doAfterCart, hUserCookie(), excludeVars, oldVars);
return 0;
}
