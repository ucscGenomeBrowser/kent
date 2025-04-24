/* pubs.c - display details of publiations literature track (pubsxxx tables) */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgc.h"
#include "hgColors.h"
#include "trackDb.h"
#include "web.h"
#include "hash.h"
#include "net.h"
#include "obscure.h"
#include "common.h"
#include "string.h"
#include "dystring.h"
#include "dnautil.h"
#include "trackHub.h"
//#include "ctype.h"

// cgi var to activate debug output
static int pubsDebug = 0;

// global var for printArticleInfo to indicate if article has suppl info 
// Most publishers have supp data.
// If they don't have it, we can skip the fileType column in the table
bool pubsHasSupp = TRUE; 

// global var for printArticleInfo to indicate if article is elsevier
// If it's elsevier, we print the copyright line
bool pubsIsElsevier = FALSE; 

// the article source is used to modify other parts of the page
static char *articleSource;

// we need the external article PMC Id for YIF links
static char *extId = NULL;

// section types in mysql table, for all annotations tables
// we note where the hit is located in the document
static char *pubsSecNames[] ={
      "header", "abstract",
      "intro", "methods",
      "results", "discussion",
      "conclusions", "ack",
      "refs", "supplement", "unknown" };

// labels to show to user, have to correspond to pubsSecNames
static char *secLabels[] ={
      "Title", "Abstract",
      "Introduction", "Methods",
      "Results", "Discussion",
      "Conclusions", "Acknowledgements",
      "References", "Supplement", "Undetermined section (e.g. for a brief communication)" };

// whether a checkbox is checked by default, have to correspond to pubsSecNames
static int pubsSecChecked[] ={
      1, 1,
      1, 1,
      1, 1,
      1, 1,
      1, 1, 1 };

static char *pubsSequenceTable;


/* ------ functions to replace HTML4 tables with HTML5 constructs */
/* Web wrappers incorporating tag, id, and class HTML attributes, to support
 * styling and test */

/* Suffix -S for  "function accepts style parameter"
 * Suffix -C for  "function accepts class parameter"
 * Suffix -CI for "function accepts class and id parameter"
 *
 * Some functions are commented out because they are not yet used.
 */

static void web2Start(char *tag)
{
printf("<%s>\n", tag);
}

static void web2End(char *tag)
{
printf("</%s>\n", tag);
}

static void web2StartS(char *style, char *tag)
{
printf("<%s style=\"%s\">\n", tag, style);
}

static void web2StartC(char *class, char *tag)
{
printf("<%s class=\"%s\">\n", tag, class);
}

static void web2StartCI(char *class, char *id, char *tag)
{
if ((id==NULL) && (class==NULL))
    web2Start(tag);
else if (id==NULL)
    web2StartC(class, tag);
else
    printf("<%s class=\"%s\" id=\"%s\">\n", tag, class, id);
}

static void web2PrintS(char *style, char *tag, char *label)
{
printf("<%s style=\"%s\">%s</%s>\n", tag, style, label, tag);
}

//static void web2PrintC(char *class, char *tag, char *label)
//{
//printf("<%s class=\"%s\">%s</%s>\n", tag, class, label, tag);
//}

//static void web2Print(char *tag, char *label)
//{
//printf("<%s>%s</%s>\n", tag, label, tag);
//}

static void web2StartTableC(char *class)             { web2StartC(class, "table"); }

static void web2StartTheadC(char *class)             { web2StartC(class, "thead"); }
static void web2EndThead()                           { web2End("thead"); }

static void web2StartTbodyS(char *style)             { web2StartS(style, "tbody"); }

static void web2StartCell()                          { web2Start("td"); }
static void web2EndCell()                            { web2End("td"); }
static void web2StartCellS(char *style)              { web2StartS(style, "td"); }
//static void web2PrintCell(char *label)               { web2Print("td", label); }
static void web2PrintCellS(char *style, char *label) { web2PrintS(style, "td", label); }

static void web2StartRow()                           { web2Start("tr"); }
static void web2EndRow()                             { web2End("tr"); }

//static void web2StartTbody()                         { web2Start("tbody"); }
static void web2EndTbody()                           { web2End("tbody"); }

//static void web2StartTable()                         { web2Start("table"); }
static void web2EndTable()                           { web2EndTbody(); web2End("table"); }

static void web2StartDivCI (char *class, char *id)    { web2StartCI(class, id, "div"); }
static void web2StartDivC (char *class)               { web2StartC(class, "div"); }

static void web2EndDiv(char *comment) 
{
printf("</div> <!-- %s -->\n", comment);
}

//static void web2Img(char *url, char *alt, int width, int hspace, int vspace) 
//{
//printf("<img src=\"%s\" alt=\"%s\" width=\"%d\" hspace=\"%d\" vspace=\"%d\">\n", url, alt, width, hspace, vspace);
//}

static void web2ImgLink(char *url, char *imgUrl, char *alt, int width, int hspace, int vspace) 
{
printf("<a href=\"%s\"><img src=\"%s\" alt=\"%s\" width=\"%d\" hspace=\"%d\" vspace=\"%d\"></a>\n", url, imgUrl, alt, width, hspace, vspace);
}

static void web2PrintHeaderCell(char *label, int width)
/* Print th heading cell with given width in percent */
{
printf("<th width=\"%d%%\">", width);
printf("%s</th>", label);
}

static void web2PrintCellF(char *format, ...)
/* print a td with format */
{
va_list args;
va_start(args, format);

web2StartCell();
vprintf(format, args);
web2EndCell();
va_end(args);
}

static void web2StartSection(char *id, char *format, ...)
/* create a new section on the web page */
{
va_list args;
va_start(args, format);

puts("<!-- START NEW SECTION -->\n");
web2StartDivCI("section", id);
web2StartDivC("subheadingBar windowSize");
vprintf(format, args);
web2EndDiv("subheadingBar");
va_end(args);
}

static void web2EndSection()
/* end section */
{
web2EndDiv("section");
}

/* ------  */

static void printDebug(char *text) 
{
if (pubsDebug)
    printf("%s<BR>", text);
}

static char *mangleUrl(char *url) 
/* add publisher specific parameters to url and return new url*/
{
if (!stringIn("sciencedirect.com", url))
    return url;
    
// cgi param to add the "UCSC matches" sciverse application to elsevier's sciencedirect
char *sdAddParam = "?svAppaddApp=298535"; 
char *longUrl = catTwoStrings(url, sdAddParam);
char *newUrl = replaceChars(longUrl, "article", "svapps");
return newUrl;
}

static void printPositionAndSize(int start, int end, bool showSize)
{
printf("<B>Position:</B>&nbsp;"
           "<A HREF=\"%s&amp;db=%s&amp;position=%s%%3A%d-%d\">",
                  hgTracksPathAndSettings(), database, seqName, start+1, end);
char startBuf[64], endBuf[64];
sprintLongWithCommas(startBuf, start + 1);
sprintLongWithCommas(endBuf, end);
printf("%s:%s-%s</A><BR>\n", seqName, startBuf, endBuf);
long size = end - start;
sprintLongWithCommas(startBuf, size);
if (showSize)
    printf("<B>Genomic Size:</B>&nbsp;%s<BR>\n", startBuf);
}

static void printFilterLink(char *pslTrack, char *articleId, char *articleTable)
/* print a link to hgTracks with an additional cgi param to activate the single article filter */
{
    int start = cgiOptionalInt("o", -1);
    if (start==-1)
        return;
    int end = cgiInt("t");
    char qBuf[1024];
    struct sqlConnection *conn = hAllocConn(database);
    sqlSafef(qBuf, sizeof(qBuf), "SELECT CONCAT(firstAuthor, year) FROM %s WHERE articleId='%s';", articleTable, articleId);
    char *dispId = sqlQuickString(conn, qBuf);

    printf("      <div class=\"subsection\">");

    if (!containsStringNoCase(pslTrack, "Bing"))
        {
        printf(
        "      <P><A HREF=\"%s&amp;db=%s&amp;position=%s%%3A%d-%d&amp;pubsFilterArticleId=%s&amp;%s=pack&amp;hgFind.matches=%s\">",
            hgTracksPathAndSettings(), database, seqName, start+1, end, articleId, pslTrack, dispId);

        printf("Show these sequence matches individually on genome browser</A> (activates track \""
            "Individual matches for article\")</P>");
        }

    printPositionAndSize(start, end, 1);
    printf(
        "      </div> <!-- class: subsection --> \n");
    hFreeConn(&conn);
    
}

static char *makeSqlMarkerList(void)
/* return list of sections from cgi vars, format like "'abstract','header'" */
{
int secCount = sizeof(pubsSecNames)/sizeof(char *);
struct dyString *dy = dyStringNew(1024);
int i;
int found = 0;
for (i=0; i<secCount; i++) 
    {
    // add ' around name and add to list
    char *secName = pubsSecNames[i];
    if (cgiOptionalInt(secName, pubsSecChecked[i]))
	{
	if (i != 0)
	    sqlDyStringPrintf(dy, ",");
        sqlDyStringPrintf(dy, "'%s'", secName);
	++found;
	}
    }

if (found==0)
    errAbort("You need to specify at least one article section.");

return dyStringCannibalize(&dy);
}


static struct sqlResult *queryMarkerRows(struct sqlConnection *conn, char *markerTable, \
    char *articleTable, char *item, int itemLimit, char *sectionList, char *artExtIdFilter)
/* query marker rows from mysql, based on http parameters  
 * optionally filter on sections or just a single article
 * */
{
/* Mysql specific setting to make the group_concat function return longer strings */
//sqlSafef(query, sizeof query, "SET SESSION group_concat_max_len = 100000");
//sqlUpdate(conn, query);
 
struct dyString *query = dyStringNew(4000);

// no need to check for illegal characters in sectionList
sqlDyStringPrintf(query, "SELECT distinct %s.articleId, url, title, authors, citation, year, "  
    "pmid FROM %s "
    //"group_concat(snippet, concat(\" (section: \", section, \")\") SEPARATOR ' (...) ') FROM %s "
    "JOIN %s USING (articleId) "
    "WHERE markerId='%s' AND section in (%-s) ",
    markerTable, markerTable, articleTable, item, sectionList);

if (isNotEmpty(artExtIdFilter))
    sqlDyStringPrintf(query, " AND extId='%s' ", artExtIdFilter);

sqlDyStringPrintf(query,
    //"GROUP by articleId "
    "ORDER BY year DESC "
    "LIMIT %d", 
     itemLimit);

    printDebug(dyStringContents(query));

struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
dyStringFree(&query);

return sr;
}

static struct sqlResult *querySnippets(struct sqlConnection *conn, char *markerTable, \
    char *articleId, char *markerId, char *sectionList)
/* query marker snippet rows from mysql for an article, markerId combination */
{
char query[4000];
sqlSafef(query, sizeof(query), "SELECT section, snippet FROM %s "  
    "WHERE articleId=%s AND markerId='%s' AND section in (%-s) ", 
    markerTable, articleId, markerId, sectionList);
struct sqlResult *sr = sqlGetResult(conn, query);
return sr;
}

static void printSectionCheckboxes()
/* show a little form with checkboxes where user can select sections they want to show */
{
int labelCount = sizeof(secLabels)/sizeof(char *);

int i;
printf("<P>\n");
printf("<B>Sections of article searched:</B><BR>\n");
printf("<FORM ACTION=\"hgc?%s&o=%s&t=%s&g=%s&i=%s\" METHOD=\"get\">\n",
    cartSidUrlString(cart), cgiString("o"), cgiString("t"), cgiString("g"), cgiString("i"));

for (i=0; i<labelCount; i++) 
{
    char *name = pubsSecNames[i];
    // checkboxes default to 0 unless checked, see 
    // http://stackoverflow.com/questions/2520952/how-come-checkbox-state-is-not-always-passed-along-to-php-script
    printf("<INPUT TYPE=\"hidden\" name=\"%s\" value=\"0\" />\n", pubsSecNames[i]);
    printf("<INPUT TYPE=\"checkbox\" name=\"%s\" ", name);

    int isChecked = cgiOptionalInt(name, pubsSecChecked[i]);
    if (isChecked)
        printf("value=\"1\" checked=\"yes\">%s</INPUT>\n", secLabels[i]);
    else
        printf("value=\"1\">%s</INPUT>\n", secLabels[i]);
}

printf("<INPUT TYPE=\"hidden\" name=\"o\" value=\"%s\" />\n", cgiString("o"));
printf("<INPUT TYPE=\"hidden\" name=\"g\" value=\"%s\" />\n", cgiString("g"));
printf("<INPUT TYPE=\"hidden\" name=\"t\" value=\"%s\" />\n", cgiString("t"));
printf("<INPUT TYPE=\"hidden\" name=\"i\" value=\"%s\" />\n", cgiString("i"));
printf("<INPUT TYPE=\"hidden\" name=\"hgsid\" value=\"%s\" />\n", cart->sessionId);
printf("<BR>");
printf("<INPUT TYPE=\"submit\" VALUE=\"Submit\" />\n");
printf("</FORM><P>\n");
}

static void printLimitWarning(struct sqlConnection *conn, char *markerTable, 
    char *item, int itemLimit, char *sectionList)
{
char query[4000];
// no need to check for illegal characters in sectionList
sqlSafef(query, sizeof(query), "SELECT COUNT(*) from %s WHERE markerId='%s' AND section in (%-s) ", markerTable, item, sectionList);
if (sqlNeedQuickNum(conn, query) > itemLimit) 
    {
    printf("<b>This marker is mentioned more than %d times</b><BR>\n", itemLimit);
    printf("The results would take too long to load in your browser and are "
    "therefore limited to the %d most recent articles.<P>\n", itemLimit);
    }
}

/* keep only uppercase letters in string*/
void eraseAllButUpper(char *s)
{
char *in, *out;
char c;

in = out = s;
for (;;)
    {
    c = *in++;
    if (c == 0)
        break;
    if (isupper(c))
        *out++ = c;
    }
*out = 0;
}

static char* printShortArticleInfo(char **row) {
/* print a two-line description of article */
char *articleId = row[0];
char *url       = row[1];
char *title     = row[2];
char *authors   = row[3];
char *citation  = row[4];
char *year      = row[5];
char *pmid      = row[6];
url = mangleUrl(url);
printf("<A HREF=\"%s\">%s</A><BR> ", url, title);
// cut author string at 40 chars, like scholar
printf("<span style=\"color:gray\">");
if (strlen(authors)>40)
    {
    authors[60] = 0;
    printf("<SMALL>%s...</SMALL> ", authors);
    }
else if (!isEmpty(authors))
    printf("<SMALL>%s</SMALL> ", authors);

// first word of citation is journal name
char *words[10];
int wordCount = chopCommas(citation, words);
char *journal = NULL;
if (wordCount!=0)
    journal = words[0];

// optional: print the little gray line with author, journal, year info
bool didPrint = FALSE;
printf("<small>");
if (year!=NULL && differentWord(year, "0"))
    {
    printf("%s", year);
    didPrint = TRUE;
    }
if (!isEmpty(journal))
    {
    printf(" - %s ", journal);
    didPrint = TRUE;
    }
if (!isEmpty(pmid) && strcmp(pmid, "0")!=0 )
    {
    printf(", <A HREF=\"https://www.ncbi.nlm.nih.gov/pubmed/%s\">PMID%s</A>\n", pmid, pmid);
    didPrint = TRUE;
    }
printf("</small></span>\n");
if (didPrint)
    printf("<BR>\n");

if (pubsDebug)
    printf("articleId=%s", articleId);

return articleId;
}

static void printSnippets(struct sqlResult *srSnip) 
{
char **snipRow;
struct hash *doneSnips = newHash(0); // avoid printing a sentence twice
int snipCount = 0;
struct slPair *secSnips = NULL;

// add all pairs to the list, remove duplicated snippets (ignore all lowercase chars)
while ((snipRow = sqlNextRow(srSnip)) != NULL)
    {
    char *section  = cloneString(snipRow[0]);
    char *snippet  = cloneString(snipRow[1]);
    char *snipHash = cloneString(snippet);
    eraseAllButUpper(snipHash);
    if (hashLookup(doneSnips, snipHash)!=NULL)
        {
        //printf("<b>already seen</b></br>");
        continue;
        }
    slPairAdd(&secSnips, section, snippet);
    hashAdd(doneSnips, snipHash, 0);
    snipCount++;
    }
hashFree(&doneSnips);

// now iterate over list and print
struct slPair *pair;
printf("<DIV CLASS=\"snips\">\n");
int i = 0;
for (pair = secSnips; pair != NULL; pair = pair->next)
    {
    char *section = pair->name;
    char *snippet = pair->val;
    
    // print snippet
    printf("<I>");
    printAddWbr(snippet, 40);
    printf("<style=\"color:gray\">...</style>\n");
    if (differentWord(section, "unknown"))
        printf(" <SPAN style=\"color:gray\">(%s)</SPAN>", section);
    printf("</I>");
    printf("<BR>");

    if (snipCount>2) 
        {
        if (i==0)
	    {
            // alternative to "more": <img src=\"../images/add_sm.gif\">
            printf("<A id='snowSnips_more' class=\"showSnips\" href=\"#\" >more</A><BR><DIV class=\"hiddenSnips\" style=\"display:none\">");
	    jsOnEventById("click", "snowSnips_more", "$(this).nextUntil('.shownSnips').slideToggle(); return false;");
	    }
        if (i==snipCount-2)
            printf("</DIV><div class=\"shownSnips\"></div>");
        }
    i++;
    }
slPairFreeList(&secSnips);
sqlFreeResult(&srSnip); 
printf("</DIV><P>");
}

static void printMarkerSnippets(struct sqlConnection *conn, char *articleTable, 
    char *markerTable, char *item)
/* print page with article info and snippets from articles */
{
/* do not show more snippets than this limit */
int itemLimit=100;

char *artExtIdFilter = cgiOptionalString("pubsFilterExtId");
/* This will have to be activated with the move to new Elsevier identifiers, ~Oct 2013 */
//if (startsWith(artExtIdFilter))
    //replaceInStr(artExtIdFilter, "ELS", "PII")

char *sectionList = makeSqlMarkerList();
if (artExtIdFilter==NULL)
    {
    printSectionCheckboxes();
    printLimitWarning(conn, markerTable, item, itemLimit, sectionList);
    printf("<H3>Snippets from Publications:</H3>");
    }

struct sqlResult *sr = queryMarkerRows(conn, markerTable, articleTable, item, \
    itemLimit, sectionList, artExtIdFilter);

// better readable if not across the whole screen
printf("<DIV style=\"width:1024px; font-size:100%%\">\n");
char **row;

// loop over articles and print out snippets for each
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *articleId = printShortArticleInfo(row);
    struct sqlConnection *snipConn = hAllocConn(database);
    struct sqlResult *srSnip = querySnippets(snipConn, markerTable, articleId, item, sectionList);
    printSnippets(srSnip);
    hFreeConn(&snipConn);
    printf("<HR>");
    }
printf("</DIV>\n");

freeMem(sectionList);
sqlFreeResult(&sr);
}

static char *urlToLogoUrl(char *pubsArticleTable, char *articleId, char *urlOrig)
/* return a string with relative path of logo for publisher given the url of
 * fulltext or a table/articleId, has to be freed 
*/
{
struct sqlConnection *conn = hAllocConn(database);
char *pubCode = NULL;
if (hHasField("hgFixed", pubsArticleTable, "publisher"))
    {
    char query[4000];
    sqlSafef(query, sizeof(query), "SELECT publisher from %s where articleId=%s", 
        pubsArticleTable, articleId);
    pubCode = sqlQuickString(conn, query);
    // if no publisher is specified, we use the source (e.g. bing, pmc, elsevier, etc)
    // to find the logo
    if (isEmpty(pubCode))
        {
        sqlSafef(query, sizeof(query), "SELECT source from %s where articleId=%s", 
            pubsArticleTable, articleId);
        pubCode = sqlQuickString(conn, query);
        }
    }
else 
    {
    // get top-level domain url if not publisher field
    char url[1024];
    memcpy(url, urlOrig, sizeof(url));
    char *slashParts[20];
    // split http://www.sgi.com/test -> to [http:,www.sgi.com,test]
    int partCount = chopString(url, "/", slashParts, ArraySize(slashParts));
    if (partCount<3)
        return NULL;
    // split www.sgi.com to [www,sgi,com]
    char *dotParts[20];
    partCount = chopString(slashParts[1], ".", dotParts, ArraySize(dotParts));
    if (partCount<3)
        return NULL;
    pubCode = dotParts[partCount-2];
    }
    
// construct path to image
char *logoUrl = needMem(512);
safef(logoUrl, 512, "../images/pubs_%s.png", pubCode);
return logoUrl;
}

static char *printArticleInfo(struct sqlConnection *conn, char *item, char *pubsArticleTable)
/* Header with information about paper, return documentId */
{
char query[512];

sqlSafef(query, sizeof(query), "SELECT articleId, url, title, authors, citation, abstract, pmid, "
    "source, extId FROM %s WHERE articleId='%s'", pubsArticleTable, item);

struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
char *articleId=NULL;
if ((row = sqlNextRow(sr)) == NULL)
    {
    printf("Could not resolve articleId %s, this is an internal error.\n", item);
    printf("Please send an email to max@soe.ucsc.edu\n");
    sqlFreeResult(&sr);
    return NULL;
    }

articleId = cloneString(row[0]);
char *url      = row[1];
char *title    = row[2];
char *authors  = row[3];
char *cit      = row[4];
char *abstract = row[5];
char *pmid     = row[6];
articleSource  = row[7];
extId          = row[8];

url = mangleUrl(url);
if (strlen(abstract)==0) 
        abstract = "(No abstract available for this article. "
            "Please follow the link to the fulltext above by clicking on the titel or the fulltext image.)";

if (stringIn("sciencedirect.com", url)) 
    {
    pubsHasSupp = FALSE;
    pubsIsElsevier = TRUE;
    }

// authors  title
printf("<DIV style=\"width:1024px; font-size:100%%\">\n");
printf("<P>%s</P>\n", authors);
//
// logo of publisher
char *logoUrl = urlToLogoUrl(pubsArticleTable, articleId, url);
if (logoUrl)
    printf("<a href=\"%s\"><img align=\"right\" hspace=\"20\" src=\"%s\"></a>\n", url, logoUrl);
freeMem(logoUrl);

printf("<div style=\"width:800px\">");
printf("<A TARGET=\"_blank\" HREF=\"%s\"><B>%s</B></A></div>\n", url, title);
printf("</DIV>\n");
printf("</DIV>\n");

printf("<P style=\"width:1024px; font-size:80%%\">%s", cit);
if (strlen(pmid)!=0 && strcmp(pmid, "0"))
    printf(", <A HREF=\"https://www.ncbi.nlm.nih.gov/pubmed/%s\">PMID%s</A>\n", pmid, pmid);
printf("</P>\n");
printf("<P style=\"width:1024px; font-size:100%%\">%s</P>\n", abstract);

if (pubsIsElsevier)
    printf("<P><SMALL>Copyright 2012 Elsevier B.V. All rights reserved.</SMALL></P>");

sqlFreeResult(&sr);
return articleId;
}

static struct hash *getSeqIdHash(struct sqlConnection *conn, char *trackTable, \
    char *articleId, char *item, char *seqName, int start)
/* return a hash with the sequence IDs for a given chain of BLAT matches */
{
if (start==-1)
    return NULL;
char query[512];
/* check first if the column exists (some debugging tables on hgwdev don't have seqIds) */
sqlSafef(query, sizeof(query), "SHOW COLUMNS FROM %s LIKE 'seqIds';", trackTable);
char *seqIdPresent = sqlQuickString(conn, query);
if (!seqIdPresent) {
    return NULL;
}

/* get sequence-Ids for feature that was clicked (item&startPos are unique) and return as hash*/
sqlSafef(query, sizeof(query), "SELECT seqIds,'' FROM %s WHERE name='%s' "
    "and chrom='%s' and chromStart=%d;", trackTable, item, seqName, start);
    printDebug(query);

// split comma-sep list into parts
char *seqIdCoordString = sqlQuickString(conn, query);
char *seqIdCoords[1024];
if (isEmpty(seqIdCoordString))
    return NULL;
int partCount = chopString(seqIdCoordString, ",", seqIdCoords, ArraySize(seqIdCoords));
int i;

struct hash *seqIdHash = NULL;
seqIdHash = newHash(0);
for (i=0; i<partCount; i++) 
    {
    if (pubsDebug)
        printf("annotId %s<br>", seqIdCoords[i]);
    hashAdd(seqIdHash, seqIdCoords[i], NULL);
    }
return seqIdHash;
}


static void printSeqHeaders(bool showDesc, bool isClickedSection) 
/* print table and headers */
{
//style=\"margin: 10px auto; width: 98%%\"style=\"background-color: #fcecc0\"
web2StartTableC("stdTbl centeredStdTbl");
web2StartTheadC("stdTblHead");
if (showDesc)
    web2PrintHeaderCell("Article file", 10);

// yif sequences have no flanking text at M. Krauthammer's request
if (stringIn("yif", articleSource))
    web2PrintHeaderCell("Matching sequences", 60);
else
    web2PrintHeaderCell("One row per sequence, with flanking text, sequence in bold", 60);

if (pubsDebug)
    web2PrintHeaderCell("Identifiers", 30);

if (!isClickedSection && !pubsDebug)
    web2PrintHeaderCell("Link to matching genomic location", 20);
web2EndThead();
web2StartTbodyS("font-family: Arial, Helvetica, sans-serif; line-height: 1.5em; font-size: 0.9em;");
}

void printHgTracksLink(char *db, char *chrom, int start, int end, char *linkText, char *optUrlStr)
/* print link to hgTracks for db at pos */
{
char buf[1024];
if (linkText==NULL) 
    {
    char startBuf[64], endBuf[64];
    sprintLongWithCommas(startBuf, start + 1);
    sprintLongWithCommas(endBuf, end);
    safef(buf, sizeof(buf), "%s:%s-%s (%s)", chrom, startBuf, endBuf, db);
    linkText = buf;
    }

if (optUrlStr==NULL)
    optUrlStr = "";
    
printf("<A HREF=\"%s&amp;db=%s&amp;position=%s:%d-%d&amp;%s\">%s</A>\n", hgTracksPathAndSettings(), db, chrom, start, end, optUrlStr, linkText);
}

void printGbLinks(struct slName *locs) 
/* print hash keys in format hg19/chr1:1-1000 as links */
{
struct slName *el;
for (el = locs; el != NULL; el = el->next) 
    {
    char *locString = el->name;
    char *db       = cloneNextWordByDelimiter(&locString, '/');
    char *chrom    = cloneNextWordByDelimiter(&locString, ':');
    char *startStr = cloneNextWordByDelimiter(&locString, '-');
    char *endStr   = cloneString(locString);

    int start = atoi(startStr);
    int end = atoi(endStr);
    printHgTracksLink(db, chrom, start, end, NULL, NULL);
    printf("<br>");
    freeMem(endStr); //XX why can't I free these?
    freeMem(chrom);
    freeMem(startStr);
    freeMem(db);
    }
}

void removeFlank (char *snippet) 
/* keep only the parts inside <b> to </b> of a string, modifies the string in place */
{
char *startPtr = stringIn("<B>", snippet);
char *endPtr   = stringIn("</B>", snippet);
if (startPtr!=0 && endPtr!=0 && startPtr<endPtr) {
    char *buf = stringBetween("<B>", "</B>", snippet);
    memcpy(snippet, buf, strlen(buf)+1);
    freeMem(buf);
    }
}


static void printYifSection(char *clickedFileUrl)
/* print section with the image on yif and a link to it */
{

// parse out yif file Id from yif url to generate link to yif page
struct netParsedUrl npu;
netParseUrl(clickedFileUrl, &npu);
struct hash *params = NULL;
struct cgiVar* paramList = NULL;
char *paramStr = strchr(npu.file, '?');
cgiParseInput(paramStr, &params, &paramList);
struct cgiVar *var = hashFindVal(params, "file");
char *figId = NULL;
if (var!=NULL && var->val!=NULL)
    figId = var->val;

char yifPageUrl[4096];
if (figId) 
    {
    safef(yifPageUrl, sizeof(yifPageUrl), "http://krauthammerlab.med.yale.edu/imagefinder/Figure.external?sp=S%s%%2F%s", extId, figId);
    }
else
    return;

web2StartSection("section", 
    "<A href=\"%s\">Yale Image Finder</a>: figure where sequences were found", 
    yifPageUrl);

web2ImgLink(yifPageUrl, clickedFileUrl, "Image from YIF", 600, 10, 10); 

web2EndSection();
}

static bool printSeqSection(char *articleId, char *title, bool showDesc, struct sqlConnection *conn, struct hash* clickedSeqs, bool isClickedSection, bool fasta, char *pslTable, char *articleTable)
/* print a section with a table of sequences, show only sequences with IDs in hash,
 * There are two sections, respective sequences are shown depending on isClickedSection and clickedSeqs 
 *   - seqs that were clicked on (isClickedSection=True) -> show only seqs in clickedSeqs
 *   - other seqs (isClickedSection=False) -> show all other seqs
 * 
 * */
{
// get data from mysql
// I support two different schemas: new and old. On old tables, there is no fileUrl yet on the annotations
// that means that oldQuery just uses an empty string for the fileUrl field.
char *oldQuery = "SELECT fileDesc, snippet, locations, annotId, sequence, \"\" FROM %s WHERE articleId='%s'";
char *newQuery = "SELECT fileDesc, snippet, locations, annotId, sequence, fileUrl FROM %s WHERE articleId='%s'";

char *queryTemplate = oldQuery;
if (hHasField("hgFixed", pubsSequenceTable, "fileUrl"))
    queryTemplate = newQuery;

char query[4096];
sqlSafef(query, sizeof(query), queryTemplate, pubsSequenceTable, articleId);
printDebug(query);
struct sqlResult *sr = sqlGetResult(conn, query);

// construct title for section
char *otherFormat = NULL;
if (fasta)
    otherFormat = "table";
else
    otherFormat = "fasta";

char fullTitle[5000];
safef(fullTitle, sizeof(fullTitle), 
"%s&nbsp;<A HREF=\"../cgi-bin/hgc?%s&o=%s&t=%s&g=%s&i=%s&fasta=%d\"><SMALL>(%s format)</SMALL></A>\n", 
title, cartSidUrlString(cart), cgiOptionalString("o"), cgiOptionalString("t"), cgiString("g"), cgiString("i"), 
!fasta, otherFormat);

web2StartSection("pubsSection", "%s", fullTitle);

// print filtering link at start of table & table headers
if (isClickedSection) {
    printFilterLink(pslTable, articleId, articleTable);
    }

if (!fasta) 
    printSeqHeaders(showDesc, isClickedSection);

// output rows
char **row;

// the URL of the file from the clicked sequences, for YIF
char *clickedFileUrl = NULL; 

bool foundSkippedRows = FALSE;
int rowId = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    rowId++;

    char *fileDesc = row[0];
    char *snippet  = row[1];
    char *locString= row[2];
    //char *artId    = row[3];
    //char *fileId   = row[4];
    //char *seqId    = row[5];
    char *annotId = row[3];
    char *seq      = row[4];
    char *fileUrl  = row[5];

    // annotation (=sequence) ID is a 64 bit int with 10 digits for 
    // article, 3 digits for file, 5 for annotation
    //char annotId[100];
    
    // some debugging help
    //safef(annotId, 100, "%10d%03d%05d", atoi(artId), atoi(fileId), atoi(seqId));
    //if (pubsDebug)
        //printf("artId %s, file %s, annot %s -> annotId %s<br>", artId, fileId, seqId, annotId);

    // only display this sequence if we're in the right section
    if (clickedSeqs!=NULL && ((hashLookup(clickedSeqs, annotId)!=NULL) != isClickedSection)) {
        foundSkippedRows = TRUE;
        continue;
    }
    // if we're in the clicked section and the current sequence is one that matched here
    // then keep the current URL, as we might need it afterwards
    else
        clickedFileUrl = cloneString(fileUrl);

    // suppress non-matches if the sequences come from YIF as figures can 
    // contain tons of non-matching sequences
    if (stringIn("yif", articleSource) && isEmpty(locString)) {
        foundSkippedRows = TRUE;
        continue;
    }

    if (fasta)
        {
        if (strlen(extId)!=0)
            printf("<tt><pre>&gt;%s-%d\n", extId, rowId);
        else
            printf("<tt><pre>&gt;seq%d\n", rowId);
        writeSeqWithBreaks(stdout, seq, strlen(seq), 80);
        printf("</pre></tt>\n");
        }
    else
        {
        web2StartRow();

        // column 1: type of file (main or supp)
        if (showDesc) 
            {
            char linkStr[4096];
            if (isEmpty(fileDesc))
                fileDesc = "main text";
            safef(linkStr, sizeof(linkStr), "<a href=\"%s\">%s</a>", fileUrl, fileDesc);
            web2PrintCellS("word-break:break-all", linkStr);
            }
        
        // column 2: snippet
        web2StartCellS("word-break:break-all");
        if (stringIn("yif", articleSource))
            removeFlank(snippet);
        printAddWbr(snippet, 40);
        web2EndCell();

        // optional debug info column
        if (pubsDebug) 
            //web2PrintCellF("article %s, file %s, seq %s, annotId %s", artId, fileId, seqId, annotId);
            web2PrintCellF("annotId %s", annotId);

        // column 3: print links to locations, only print this in the 2nd section
        if (!isClickedSection && !pubsDebug) 
            {
            // format: hg19/chr1:300-400,mm9/chr1:60006-23234
            // split on "," then split on "/"
            //locs = charSepToSlNames(locString, ',');

            web2StartCell();
            char *locArr[1024];
            int partCount = chopString(locString, ",", locArr, ArraySize(locArr));
            if (partCount==0)
                printf("No matches");
            else
                {
                struct slName *locs;
                locs = slNameListFromStringArray(locArr, partCount);
                slUniqify(&locs, slNameCmp, slNameFree);
                printGbLinks(locs);
                printf("<br>");
                slFreeList(&locs);
                }
            web2EndCell();
            }
        web2EndRow();
        }
    }

if (!fasta)
    web2EndTable();

web2EndSection();
/* Yale Image finder files contain links to the image itself */
if (pubsDebug)
    printf("%s %s %d", articleSource, clickedFileUrl, isClickedSection);
if (stringIn("yif", articleSource) && (clickedFileUrl!=NULL) && isClickedSection) 
    printYifSection(clickedFileUrl);

freeMem(clickedFileUrl);

sqlFreeResult(&sr);
return foundSkippedRows;
}

static void printSeqInfo(struct sqlConnection *conn, char *trackTable,
    char *pslTable, char *articleId, char *item, char *seqName, int start, 
    bool fileDesc, bool fasta, char *articleTable)
    /* print sequences, split into two sections 
     * two sections: one for sequences that were clicked, one for all others*/
{
struct hash *clickedSeqs = getSeqIdHash(conn, trackTable, articleId, item, seqName, start);

bool skippedRows;
if (clickedSeqs) 
    skippedRows = printSeqSection(articleId, "Sequences matching here", \
        fileDesc, conn, clickedSeqs, 1, fasta, pslTable, articleTable);
else 
    skippedRows=1;

if (skippedRows) 
    {
    // the section title should change if the data comes from the yale image finder = a figure
    char *docType = "article";
    if (stringIn("yif", articleSource))
        docType = "figure";
    char title[1024];
    if (clickedSeqs)
        safef(title, sizeof(title), "Other Sequences in this %s", docType);
    // NO clicked seqs can happen if the hgc was called with no o or t parameters
    // from somewhere outside the browser, like elsevier or europmc
    else
        safef(title, sizeof(title), "Sequences in this %s", docType);

    printSeqSection(articleId, title, \
        fileDesc, conn, clickedSeqs, 0, fasta, pslTable, articleTable);
    }
freeHash(&clickedSeqs);
}

static void printTrackVersion(struct trackDb *tdb, struct sqlConnection *conn, char *item) 
{
char versionString[256];
char dateReference[256];
char headerTitle[512];
/* see if hgFixed.trackVersion exists */
boolean trackVersionExists = hTableExists("hgFixed", "trackVersion");

if (trackVersionExists)
    {
    char query[256];
    sqlSafef(query, sizeof(query), \
    "SELECT version,dateReference FROM hgFixed.trackVersion "
    "WHERE db = '%s' AND name = 'pubs' ORDER BY updateTime DESC limit 1", database);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;

    /* in case of NULL result from the table */
    versionString[0] = 0;
    while ((row = sqlNextRow(sr)) != NULL)
        {
        safef(versionString, sizeof(versionString), "version %s",
            row[0]);
        safef(dateReference, sizeof(dateReference), "%s",
            row[1]);
        }
    sqlFreeResult(&sr);
    }
else
    {
    versionString[0] = 0;
    dateReference[0] = 0;
    }

if (versionString[0])
    safef(headerTitle, sizeof(headerTitle), "%s - %s", item, versionString);
else
    safef(headerTitle, sizeof(headerTitle), "%s", item);

genericHeader(tdb, headerTitle);
}

static bioSeq *getSeq(struct sqlConnection *conn, char *table, char *id)
/* copied from otherOrgs.c */
{
char query[512];
struct sqlResult *sr;
char **row;
bioSeq *seq = NULL;
sqlSafef(query, sizeof(query), 
    "select sequence from %s where annotId = '%s'", table, id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(seq);
    seq->name = cloneString(id);
    seq->dna = cloneString(row[0]);
    seq->size = strlen(seq->dna);
    }
sqlFreeResult(&sr);

return seq;
}

void pubsAli(struct sqlConnection *conn, char *pslTable, char *seqTable, char *item)
/* this is just a ripoff from htcCdnaAli, similar to markd's transMapAli */
{
bioSeq *oSeq = NULL;

char title[1024];
safef(title, sizeof title, "Literature Sequence vs Genomic");
htmlFramesetStart(title);

struct psl *psl = getAlignments(conn, pslTable, item);
if (psl == NULL)
    errAbort("Couldn't find alignment at %s:%s", pslTable, item);

oSeq = getSeq(conn, seqTable, item);

if (oSeq == NULL)  
    errAbort("%s is in pslTable but not in sequence table. Internal error.", item);

enum gfType qt;
if (psl->qSize!=oSeq->size) 
    {
    qt = gftProt;
    // trying to correct pslMap's changes to qSize/qStarts and blockSizes
    psl->strand[1]=psl->strand[0];
    psl->strand[0]='+';
    psl->strand[2]=0;
    psl->qSize = psl->qSize/3;
    psl->match = psl->match/3;
    // Take care of codons that go over block boundaries:
    // Convert a block with blockSizes=58,32 and qStarts=0,58,
    // to blockSizes=19,11 and qStarts=0,19
    int i;
    int remaind = 0;
    for (i=0; i<psl->blockCount; i++)
        {
        psl->qStarts[i] = psl->qStarts[i]/3;

        int bs = psl->blockSizes[i];
        remaind += (bs % 3);
        if (remaind>=3)
        {
            bs += 1;
            remaind -= 3;
        }
        psl->blockSizes[i] = bs/3; 
        }

    }
else
    qt = gftDna;

showSomeAlignment(psl, oSeq, qt, 0, oSeq->size, NULL, 0, 0);
}

void doPubsDetails(struct trackDb *tdb, char *item)
/* publications custom display */
{

int start        = cgiOptionalInt("o", -1);
int end          = cgiOptionalInt("t", -1);
char *trackTable = trackHubSkipHubName(cgiString("g"));
char *aliTable   = cgiOptionalString("aliTable");
int fasta        = cgiOptionalInt("fasta", 0);
pubsDebug        = cgiOptionalInt("debug", 0);

char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
if (liftDb != NULL)
    database = liftDb;

struct sqlConnection *conn = hAllocConn(database);

char *articleTable = trackDbRequiredSetting(tdb, "pubsArticleTable");

if (stringIn("Psl", trackTable))
    { 
    if (aliTable!=NULL)
        {
        pubsSequenceTable = trackDbRequiredSetting(tdb, "pubsSequenceTable");
        pubsAli(conn, trackTable, pubsSequenceTable, item);
        return;
        }

    else
        {
        genericHeader(tdb, item);
        struct psl *psl = getAlignments(conn, trackTable, item);
        printf("<H3>Genomic Alignment with sequence found in publication fulltext</H3>");
        printAlignmentsSimple(psl, start, trackTable, trackTable, item);
        }
    }
else
    {
    printTrackVersion(tdb, conn, item);
    if (stringIn("Marker", trackTable))
        {
        char *markerTable = trackDbRequiredSetting(tdb, "pubsMarkerTable");
        if (start!=-1)
            printPositionAndSize(start, end, 0);
        printMarkerSnippets(conn, articleTable, markerTable, item);
        }
    else
        {
        pubsSequenceTable = trackDbRequiredSetting(tdb, "pubsSequenceTable");
        char *articleId = printArticleInfo(conn, item, articleTable);
        if (articleId!=NULL) 
            {
            char *pslTable = trackDbRequiredSetting(tdb, "pubsPslTrack");
            printSeqInfo(conn, trackTable, pslTable, articleId, item, \
                seqName, start, pubsHasSupp, fasta, articleTable);
            }
    }
}

printTrackHtml(tdb);
hFreeConn(&conn);
}
