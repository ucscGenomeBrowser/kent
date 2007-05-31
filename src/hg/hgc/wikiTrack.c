/* Handle details pages for wiggle tracks. */

#include "common.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "hgConfig.h"
#include "obscure.h"
#include "binRange.h"
#include "web.h"
#include "net.h"
#include "grp.h"
#include "htmlPage.h"
#include "wikiLink.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.6 2007/05/31 23:02:17 hiram Exp $";

#define NEW_ITEM_SCORE "newItemScore"
#define NEW_ITEM_STRAND "newItemStrand"
#define NEW_ITEM_CLASS "newItemClass"
#define NEW_ITEM_COMMENT "newItemComment"
#define NEW_ITEM_NAME "defaultName"
#define ITEM_NOT_CLASSIFIED "Not classified"
#define ITEM_SCORE_DEFAULT "1000"
#define NEW_ITEM_COMMENT_DEFAULT "enter description and comments"
#define ADD_ITEM_COMMENT_DEFAULT "add comments"
#define NO_ITEM_COMMENT_SUPPLIED "(no initial description supplied)"
#define NEW_ITEM_CATEGORY "[[Category:Genome Annotation]]"
#define WIKI_ITEM_ID "wikiItemId"

static char *encodedHgcReturnUrl(int hgsid)
/* Return a CGI-encoded hgSession URL with hgsid.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "http://%s/cgi-bin/hgc?%s&g=%s&c=%s&o=%d&l=%d&r=%d&db=%s&i=Make+new+entry",
    cgiServerName(), cartSidUrlString(cart), WIKI_TRACK_TABLE, seqName,
	winStart, winStart, winEnd, database);
/*
g=wikiTrack
c=chrII
o=13934373
l=13934373
r=15279311
t=15279311
i=Make+new+entry
db=ce3
    cgiContinueHiddenVar("c");
    cgiContinueHiddenVar("o");
    cgiContinueHiddenVar("l");
    cgiContinueHiddenVar("r");
printf("../cgi-bin/hgc?%s&g=ccdsGene&i=%s&c=%s&o=%d&l=%d&r=%d&db=%s",
       cartSidUrlString(cart), ccdsInfo->ccds, seqName, 
       winStart, winStart, winEnd, database);
*/
return cgiEncode(retBuf);
}   

static char *wikiTrackUserLoginUrl(int hgsid)
/* Return the URL for the wiki user login page. */
{
char buf[2048];
char *retEnc = encodedHgcReturnUrl(hgsid);
if (! wikiLinkEnabled())
    errAbort("wikiLinkUserLoginUrl called when wiki is not enabled (specified "
             "in hg.conf).");
safef(buf, sizeof(buf),
      "%s/index.php?title=Special:UserloginUCSC&returnto=%s",
      cfgOptionDefault(CFG_WIKI_URL, NULL), retEnc);
freez(&retEnc);
return(cloneString(buf));
}

void offerLogin(char *loginType)
{
char *wikiHost = wikiLinkHost();
printf("<P>Please login to %s the annotation track.</P>\n", loginType);
printf("<P>The login page is handled by our "
       "<A HREF=\"http://%s/\" TARGET=_BLANK>wiki system</A>:\n", wikiHost);
printf("<A HREF=\"%s\"><B>click here to login.</B></A><BR />\n",
	wikiTrackUserLoginUrl(cartSessionId(cart)));
printf("The wiki also serves as a forum for users "
       "to share knowledge and ideas.\n</P>\n");
}

#ifdef NOT
boolean stringInBetween(char *begin, char *end, char *haystack,
	char **start, char **stop)
/* return true if string starting with begin and ending with end
 *	is found within haystack.  In addition, return the start and
 *	stop locations of that string.
 * If not found, return false
 */
{
char *pos, *p;
int len;
if ((p = stringIn(begin, haystack)) != NULL)
    {
    pos = p + strlen(begin);
    if ((p = stringIn(end, pos)) != NULL)
        {
	start = pos;
	stop = p + strlen(end);
        return TRUE;
        }
    }
return FALSE;
}
#endif

#define WIKI_NO_TEXT_RESPONSE "There is currently no text in this page"

static char *stripEditURLs(char *rendered)
/* test for actual text, remove edit sections and any html comment strings */
{
char *stripped = cloneString(rendered);
char *found = NULL;
char *begin = "<div class=\"editsection\"";
char *end = "></a>";

/* XXXX is this response going to be language dependent ? */
if (stringIn(WIKI_NO_TEXT_RESPONSE,rendered))
	return NULL;

/* remove any edit sections */
while (NULL != (found = stringBetween(begin, end, stripped)) )
    {
    if (strlen(found) > 0)
	{
	struct dyString *rm = newDyString(1024);
	dyStringPrintf(rm, "%s%s%s", begin, found, end);
	stripString(stripped, rm->string);
	freeMem(found);
	freeDyString(&rm);
	}
    }

/* and remove comment strings from the wiki */
begin = "<!--";
end = "-->";
while (NULL != (found = stringBetween(begin, end, stripped)) )
    {
    if (strlen(found) > 0)
	{
	struct dyString *rm = newDyString(1024);
	dyStringPrintf(rm, "%s%s%s", begin, found, end);
	stripString(stripped, rm->string);
	freeMem(found);
	freeDyString(&rm);
	}
    }

return stripped;
}

static void startEditForm(char *actionType)
{
hPrintf("<FORM ACTION=\"%s\">\n\n", hgcName());
cartSaveSession(cart);
cgiMakeHiddenVar("g", actionType);
cgiContinueHiddenVar("c");
cgiContinueHiddenVar("o");
hPrintf("\n");
cgiContinueHiddenVar("l");
cgiContinueHiddenVar("r");
hPrintf("\n");
}

static char *fetchWikiRawText(char *descriptionKey)
/* fetch page from wiki in raw form as it is in the edit form */
{
char wikiPageUrl[512];
safef(wikiPageUrl, sizeof(wikiPageUrl), "%s/index.php/%s?action=raw",
	cfgOptionDefault(CFG_WIKI_URL, NULL), descriptionKey);
struct lineFile *lf = netLineFileMayOpen(wikiPageUrl);

struct dyString *wikiPage = newDyString(1024);
if (lf)
    {
    char *line;
    int lineSize;
    while (lineFileNext(lf, &line, &lineSize))
	dyStringPrintf(wikiPage, "%s\n", line);
    lineFileClose(&lf);
    }

/* test for text, remove any edit sections and comment strings */
char *rawText = NULL;
if (wikiPage->string)
    {
    /* XXXX is this response going to be language dependent ? */
    if (stringIn(WIKI_NO_TEXT_RESPONSE,wikiPage->string))
	return NULL;
    rawText = dyStringCannibalize(&wikiPage);
    }
freeDyString(&wikiPage);

return rawText;
}

static char *fetchWikiRenderedText(char *descriptionKey)
/* fetch page from wiki in rendered form, strip it of edit URLS,
 *	html comments, and test for actual proper return.
 *  returned string can be freed after use */
{
/* fetch previous description comments from the wiki */
char wikiPageUrl[512];
safef(wikiPageUrl, sizeof(wikiPageUrl), "%s/index.php/%s?action=render",
	cfgOptionDefault(CFG_WIKI_URL, NULL), descriptionKey);
struct lineFile *lf = netLineFileMayOpen(wikiPageUrl);

struct dyString *wikiPage = newDyString(1024);
if (lf)
    {
    char *line;
    int lineSize;
    while (lineFileNext(lf, &line, &lineSize))
	dyStringPrintf(wikiPage, "%s\n", line);
    lineFileClose(&lf);
    }

/* test for text, remove any edit sections and comment strings */
char *strippedRender = NULL;
if (wikiPage->string)
    strippedRender = stripEditURLs(wikiPage->string);
freeDyString(&wikiPage);

return strippedRender;
}

static struct wikiTrack *findItem(char *itemName, int wikiItemId)
/* given a wikiItemId (or zero) return the row from the table */
{
struct wikiTrack *item;
char query[256];
struct sqlConnection *conn = hConnectCentral();

/* XXXX need to do proper location limit search here, and take care of
 *	multiple hits
 */
if (0 == wikiItemId)
    safef(query, ArraySize(query), "SELECT * FROM %s WHERE name='%s' "
	"AND db='%s' limit 1", WIKI_TRACK_TABLE, itemName, database);
else
    safef(query, ArraySize(query), "SELECT * FROM %s WHERE id='%d' limit 1",
	WIKI_TRACK_TABLE, wikiItemId);

item = wikiTrackLoadByQuery(conn, query);
if (NULL == item)
    errAbort("display wiki item: failed to load item '%s'", itemName);
hDisconnectCentral(&conn);

return item;
}

static void displayItem(struct wikiTrack *item, char *userName, int wikiItemId)
/* given an already fetched item, get the item description from
 *	the wiki.  Put up edit form(s) if userName is not NULL
 */
{ 
char *url = cfgOptionDefault(CFG_WIKI_URL, NULL);
char *strippedRender = fetchWikiRenderedText(item->descriptionKey);

hPrintf("<B>Classification group:&nbsp;</B>%s<BR />\n", item->class);
printPosOnChrom(item->chrom, item->chromStart, item->chromEnd,
    item->strand, FALSE, item->name);
hPrintf("<B>Score:&nbsp;</B>%u<BR />\n", item->score);
hPrintf("<B>Created </B>%s<B> by:&nbsp;</B>", item->creationDate);
hPrintf("<A HREF=\"%s/index.php/User:%s\" TARGET=_blank>%s</A><BR />\n", url,
    item->owner, item->owner);
hPrintf("<B>Last update:&nbsp;</B>%s<BR />\n", item->lastModifiedDate);
if ((NULL != userName) && sameWord(userName, item->owner))
    hPrintf("<B>Owner'%s' has deletion rights</B><BR />\n", item->owner);

hPrintf("<BR />\n");

if (NULL == userName)
    {
    offerLogin("add comments to items on");
    }
else
    {
    startEditForm(G_ADD_WIKI_COMMENTS);
    webPrintLinkTableStart();
    /* first row is a title line */
    char label[256];
    safef(label, ArraySize(label), "<B>'%s' adding comments to item '%s'</B>\n",
	userName, item->name);
    webPrintWideLabelCell(label, 2);
    webPrintLinkTableNewRow();
    /* second row is initial comment/description text entry */
    webPrintWideCellStart(2, HG_COL_TABLE);
    hPrintf("<B>add comments:</B><BR />");
    cgiMakeTextArea(NEW_ITEM_COMMENT, ADD_ITEM_COMMENT_DEFAULT, 3, 40);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    webPrintLinkCellStart();
    cgiMakeButton("submit", "add comments");
    webPrintLinkCellEnd();
    webPrintLinkTableEnd();
    char idString[128];
    safef(idString, ArraySize(idString), "%d", item->id);
    cgiMakeHiddenVar("i", item->name);
    cgiMakeHiddenVar(WIKI_ITEM_ID, idString);
    hPrintf("</FORM>");

    hPrintf("For extensive edits, it is more convenient to edit the ");
hPrintf("<A HREF=\"%s/index.php/%s\" TARGET=_blank>wiki article</A> for this item's description", url, item->descriptionKey);
    }
if (strippedRender)
    {
    hPrintf("<HR>\n%s<BR />\n", strippedRender);
    freeMem(strippedRender);
    }
else
    hPrintf("<BR />\n(no comments for this item at the current time)<BR />\n");

}

void doWikiTrack(char *itemName, char *chrom, int winStart, int winEnd)
/* handle item clicks on wikiTrack - may create new items */
{
char *userName = NULL;

if (wikiTrackEnabled(&userName) && startsWith("Make new entry", itemName))
    {
    cartWebStart(cart, "%s", "User Annotation Track: Create new item");
    if (NULL == userName)
	{
	offerLogin("add new items to");
	cartHtmlEnd();
	return;
	}

    startEditForm(G_CREATE_WIKI_ITEM);

    webPrintLinkTableStart();
    /* first row is a title line */
    char label[256];
    safef(label, ArraySize(label), "<B>Create new item, owner: '%s'</B>\n",
	userName);
    webPrintWideLabelCell(label, 2);
    webPrintLinkTableNewRow();
    /* second row is group classification pull-down menu */
    webPrintWideCellStart(2, HG_COL_TABLE);
    puts("<B>classification group:&nbsp;</B>");
    struct grp *group, *groupList = hLoadGrps();
    int groupCount = 0;
    for (group = groupList; group; group=group->next)
	++groupCount;
    char **classMenu = NULL;
    classMenu = (char **)needMem((size_t)(groupCount * sizeof(char *)));
    groupCount = 0;
    classMenu[groupCount++] = cloneString(ITEM_NOT_CLASSIFIED);
    for (group = groupList; group; group=group->next)
	{
	if (differentWord("Custom Tracks", group->label))
	    classMenu[groupCount++] = cloneString(group->label);
	}
    grpFreeList(&groupList);

    cgiMakeDropList(NEW_ITEM_CLASS, classMenu, groupCount,
	    cartUsualString(cart,NEW_ITEM_CLASS,ITEM_NOT_CLASSIFIED));
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    /* third row is position entry box */
    webPrintWideCellStart(2, HG_COL_TABLE);
    puts("<B>position:&nbsp;</B>");
    savePosInTextBox(seqName, winStart+1, winEnd);
    hPrintf("&nbsp;(size: ");
    printLongWithCommas(stdout, (long long)(winEnd - winStart));
    hPrintf(")");
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    /* fourth row is strand selection radio box */
    webPrintWideCellStart(2, HG_COL_TABLE);
    char *strand = cartUsualString(cart, NEW_ITEM_STRAND, "plus");
    boolean plusStrand = sameWord("plus",strand) ? TRUE : FALSE;
    hPrintf("<B>strand:&nbsp;");
    cgiMakeRadioButton(NEW_ITEM_STRAND, "plus", plusStrand);
    hPrintf("&nbsp;+&nbsp;&nbsp;");
    cgiMakeRadioButton(NEW_ITEM_STRAND, "minus", ! plusStrand);
    hPrintf("&nbsp;-</B>");
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    /* fifth row is item name text entry */
    webPrintWideCellStart(2, HG_COL_TABLE);
    hPrintf("<B>item name:&nbsp;</B>");
    cgiMakeTextVar("i", NEW_ITEM_NAME, 18);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    /* sixth row is item score text entry */
    webPrintWideCellStart(2, HG_COL_TABLE);
    hPrintf("<B>item score:&nbsp;</B>");
    cgiMakeTextVar(NEW_ITEM_SCORE, ITEM_SCORE_DEFAULT, 4);
    hPrintf("&nbsp;(range:&nbsp;0&nbsp;to&nbsp;%s)", ITEM_SCORE_DEFAULT);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    /* seventh row is initial comment/description text entry */
    webPrintWideCellStart(2, HG_COL_TABLE);
    hPrintf("<B>initial comments/description:</B><BR />");
    cgiMakeTextArea(NEW_ITEM_COMMENT, NEW_ITEM_COMMENT_DEFAULT, 5, 40);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    /* seventh row is the submit and cancel buttons */
    webPrintLinkCellStart();
    cgiMakeButton("submit", "create new item");
    hPrintf("</FORM>");
    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    hPrintf("<FORM ACTION=\"%s\">", hgTracksName());
    cgiMakeButton("cancel", "cancel");
    hPrintf("</FORM>");
    webPrintLinkCellEnd();
    webPrintLinkTableEnd();
    }
else
    {
    struct wikiTrack *item = findItem(itemName, 0);
    cartWebStart(cart, "%s (%s)", "User Annotation Track", item->name);
    /* if we can get the hgc clicks to add item id to the incoming data,
     *	then use that item Id here
     */
    displayItem(item, userName, 0);
    }

cartHtmlEnd();
}

static void htmlCloneFormVarSet(struct htmlForm *parent,
	struct htmlForm *clone, char *name, char *val)
/* clone form variable from parent, with new value,
 * if *val is NULL, clone val from parent
 */
{
struct htmlFormVar *cloneVar;
struct htmlFormVar *var;
if (parent == NULL)
    errAbort("Null parent form passed to htmlCloneFormVarSet");
if (clone == NULL)
    errAbort("Null clone form passed to htmlCloneFormVarSet");
var = htmlFormVarGet(parent, name);
if (var == NULL)
    errAbort("Variable '%s' not found in parent in htmlCloneFormVarSet", name);

AllocVar(cloneVar);
cloneVar->name = cloneString(var->name);
cloneVar->tagName = cloneString(var->tagName);
cloneVar->type = cloneString(var->type);
if (NULL == val)
    cloneVar->curVal = cloneString(var->curVal);
else
    cloneVar->curVal = cloneString(val);
slAddHead(&clone->vars, cloneVar);

}


static void addDescription(char * descriptionKey)
{
char *newComments = cartNonemptyString(cart, NEW_ITEM_COMMENT);
struct dyString *content = newDyString(1024);
struct htmlCookie *cookie;
char wikiPageUrl[512];

/* was nothing changed in the add comments entry box ? */
if (sameWord(ADD_ITEM_COMMENT_DEFAULT,newComments))
    return;

/* must pass the session cookie from the wiki in order to edit */
AllocVar(cookie);
cookie->name = cloneString(cfgOption(CFG_WIKI_SESSION_COOKIE));
cookie->value = cloneString(findCookieData(cookie->name));

/* fetch the edit page to get the wpEditToken, and current contents */
safef(wikiPageUrl, sizeof(wikiPageUrl), "%s/index.php/%s?action=edit",
	cfgOptionDefault(CFG_WIKI_URL, NULL), descriptionKey);

char *fullText = htmlSlurpWithCookies(wikiPageUrl,cookie);
struct htmlPage *page = htmlPageParseOk(wikiPageUrl, fullText);

/* create a stripped down edit form, we don't want all the variables */
struct htmlForm *strippedEditForm;
AllocVar(strippedEditForm);

struct htmlForm *currentEditForm = htmlFormGet(page,"editform");
if (NULL == currentEditForm)
    errAbort("addDescription: can not get editform ?");
strippedEditForm->name = cloneString(currentEditForm->name);
/* the lower case "post" in the editform does not work ? */
/*strippedEditForm->method = cloneString(currentEditForm->method);*/
strippedEditForm->method = cloneString("POST");
strippedEditForm->startTag = currentEditForm->startTag;
strippedEditForm->endTag = currentEditForm->endTag;

/* fetch any current page contents in the edit form to continue them */
struct htmlFormVar *wpTextbox1 =
	htmlPageGetVar(page, currentEditForm, "wpTextbox1");

if (wpTextbox1->curVal && (strlen(wpTextbox1->curVal) > 2))
    {
    char *rawText = fetchWikiRawText(descriptionKey);
    dyStringPrintf(content, "%s<P>\n''comments added: ~~~~''<BR /><BR />\n",
	rawText);
    }
else
    {
    char position[128];
    char *newPos;
    snprintf(position, 128, "%s:%d-%d", seqName, winStart+1, winEnd);
    newPos = addCommasToPos(position);
    dyStringPrintf(content, "%s\n<P>"
"[http://genome-hiram.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d %s %s]"
	"&nbsp;&nbsp;''created: ~~~~''<BR /><BR />\n",
	NEW_ITEM_CATEGORY, database, seqName, winStart, winEnd,
	    database, newPos);
    }

if (sameWord(NEW_ITEM_COMMENT_DEFAULT,newComments))
    dyStringPrintf(content, "%s\n</P>\n", NO_ITEM_COMMENT_SUPPLIED);
else
    dyStringPrintf(content, "%s\n</P>\n", newComments);


htmlCloneFormVarSet(currentEditForm, strippedEditForm,
	"wpTextbox1", content->string);
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpSummary", "");
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpSection", "");
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpMinoredit", "1");
/*
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpSave", "Save page");
*/
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpEdittime", NULL);
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpEditToken", NULL);

htmlPageSetVar(page,currentEditForm, "wpTextbox1", content->string);
htmlPageSetVar(page,currentEditForm, "wpSummary", "");
htmlPageSetVar(page,currentEditForm, "wpSection", "");
htmlPageSetVar(page,currentEditForm, "wpMinoredit", "1");
htmlPageSetVar(page,currentEditForm, "wpSave", "Save page");

char newUrl[1024];
/* fake out htmlPageFromForm since it doesn't understand the colon : */
safef(newUrl, ArraySize(newUrl), "%s%s",
	"http://genomewiki.ucsc.edu", currentEditForm->action);
/* something, somewhere encoded the & into &amp; which does not work */
char *fixedString = replaceChars(newUrl, "&amp;", "&");
currentEditForm->action = cloneString(fixedString);
strippedEditForm->action = cloneString(fixedString);
struct htmlPage *editPage = htmlPageFromForm(page,strippedEditForm,"submit", "Submit");

if (NULL == editPage)
    errAbort("addDescription: the edit is failing ?");

freeDyString(&content);
}

static void updateLastModifiedDate(int id)
/* set lastModifiedDate to now() */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

safef(query, ArraySize(query),
    "UPDATE %s set lastModifiedDate=now() WHERE id='%d'",
	WIKI_TRACK_TABLE, id);
sqlUpdate(conn,query);
hDisconnectCentral(&conn);
}

void doDeleteWikiItem(char *itemName, char *chrom, int winStart, int winEnd)
/* handle delete item clicks for wikiTrack */
{
char *userName = NULL;
char *idString = cartUsualString(cart, WIKI_ITEM_ID, NULL);
cartWebStart(cart, "%s (%s)", "User Annotation Track, delete item: ", itemName);
if (NULL == idString)
    errAbort("delete wiki item: NULL wikiItemId");
if (! wikiTrackEnabled(&userName))
    errAbort("delete wiki item: wiki track not enabled");
struct wikiTrack *item = findItem(itemName, sqlSigned(idString));
displayItem(item, userName, sqlSigned(idString));
cartHtmlEnd();
}

void doAddWikiComments(char *itemName, char *chrom, int winStart, int winEnd)
/* handle add comment item clicks for wikiTrack */
{
char *userName = NULL;
char *idString = cartUsualString(cart, WIKI_ITEM_ID, NULL);
cartWebStart(cart, "%s (%s)", "User Annotation Track", itemName);
if (NULL == idString)
    errAbort("add wiki comments: NULL wikiItemId");
if (! wikiTrackEnabled(&userName))
    errAbort("add wiki comments: wiki track not enabled");
struct wikiTrack *item = findItem(itemName, sqlSigned(idString));
addDescription(item->descriptionKey);
updateLastModifiedDate(sqlSigned(idString));
displayItem(item, userName, sqlSigned(idString));
cartHtmlEnd();
}

void doCreateWikiItem(char *itemName, char *chrom, int winStart, int winEnd)
    /* handle create item clicks for wikiTrack */
{
int itemStart = 0;
int itemEnd = 0;
char *chrName = NULL;
char *pos = NULL;
char *strand = cartUsualString(cart, NEW_ITEM_STRAND, "plus");
char *class = cartUsualString(cart, NEW_ITEM_CLASS, ITEM_NOT_CLASSIFIED);
boolean plusStrand = sameWord("plus",strand) ? TRUE : FALSE;
char descriptionKey[256];
struct sqlConnection *conn = hConnectCentral();
char *userName = NULL;
char *color = "000000";
int score = 0;
struct wikiTrack *newItem;

if (! wikiTrackEnabled(&userName))
    errAbort("create new wiki item: wiki track not enabled");
if (NULL == userName)
    errAbort("create new wiki item: user not logged in ?");

score = sqlSigned(cartUsualString(cart, NEW_ITEM_SCORE, ITEM_SCORE_DEFAULT));
pos = stripCommas(cartOptionalString(cart, "getDnaPos"));
if (NULL == pos)
    errAbort("create new wiki item: called incorrectly, without getDnaPos");

hgParseChromRange(pos, &chrName, &itemStart, &itemEnd);

safef(descriptionKey,ArraySize(descriptionKey),
	"GenomeAnnotation:%s-%d", database, 0);

AllocVar(newItem);
newItem->bin = binFromRange(itemStart, itemEnd);
newItem->chrom = cloneString(chrName);
newItem->chromStart = itemStart;
newItem->chromEnd = itemEnd;
newItem->name = cloneString(itemName);
newItem->score = score;
safef(newItem->strand, sizeof(newItem->strand), "%s", plusStrand ? "+" : "-");
newItem->db = cloneString(database);
newItem->owner = cloneString(userName);
newItem->class = cloneString(class);
newItem->color = cloneString(color);
newItem->creationDate = cloneString("0");
newItem->lastModifiedDate = cloneString("0");
newItem->descriptionKey = cloneString(descriptionKey);
newItem->id = 0;

wikiTrackSaveToDbEscaped(conn, newItem, WIKI_TRACK_TABLE, 1024);

int id = sqlLastAutoId(conn);
safef(descriptionKey,ArraySize(descriptionKey),
	"GenomeAnnotation:%s-%d", database, id);

wikiTrackFree(&newItem);

char newItemName[128];
char query[512];
if (sameWord(itemName,NEW_ITEM_NAME))
    {
    safef(newItemName, ArraySize(newItemName), "%s-%d", database, id);
    safef(query, ArraySize(query), "UPDATE %s set creationDate=now(),lastModifiedDate=now(),descriptionKey='%s',name='%s-%d' WHERE id='%d'",
	WIKI_TRACK_TABLE, descriptionKey, database, id, id);
    
    }
else
    {
    safef(newItemName, ArraySize(newItemName), "%s", itemName);
    safef(query, ArraySize(query), "UPDATE %s set creationDate=now(),lastModifiedDate=now(),descriptionKey='%s' WHERE id='%d'",
	WIKI_TRACK_TABLE, descriptionKey, id);
    }
sqlUpdate(conn,query);

cartWebStart(cart, "%s %s", "User Annotation Track, created new item: ",
	newItemName);

addDescription(descriptionKey);

struct wikiTrack *item = findItem(newItemName, id);
displayItem(item, userName, id);

cartHtmlEnd();
}
