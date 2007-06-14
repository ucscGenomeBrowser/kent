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
#include "hui.h"
#include "htmlPage.h"
#include "wikiLink.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.13 2007/06/14 22:19:40 hiram Exp $";

#define NEW_ITEM_SCORE "newItemScore"
#define NEW_ITEM_STRAND "newItemStrand"
#define NEW_ITEM_CLASS "newItemClass"
#define NEW_ITEM_COMMENT "newItemComment"
#define NEW_ITEM_NAME "defaultName"
#define NEW_ITEM_COLOR "itemColor"
#define ITEM_NOT_CLASSIFIED "Not classified"
#define ITEM_SCORE_DEFAULT "1000"
#define NEW_ITEM_COMMENT_DEFAULT "enter description and comments"
#define ADD_ITEM_COMMENT_DEFAULT "add comments"
#define NO_ITEM_COMMENT_SUPPLIED "(no initial description supplied)"
#define NEW_ITEM_CATEGORY "[[Category:Genome Annotation]]"
#define DEFAULT_BROWSER "genome.ucsc.edu"
#define TEST_EMAIL_VERIFIED "GenomeAnnotation:TestEmailVerified"
#define EMAIL_NEEDS_TO_BE_VERIFIED \
	"You must confirm your e-mail address before editing pages"
#define USER_PREFERENCES_MESSAGE \
    "Please set and validate your e-mail address through your"
#define WIKI_NO_TEXT_RESPONSE "There is currently no text in this page"

static char *colorMenuJS = "onchange=\"updateColorSelectBox();\" style=\"width:8em;\"";

static void colorMenuOutput()
/* the item color pull-down menu in the create item form */
{
hPrintf("<INPUT NAME=\"noName\" VALUE=\"\" SIZE=1 STYLE=\"display:none;\" >\n");

hPrintf("<SELECT NAME=\"itemColor\" style=\"width:8em; background-color:#000000;\" %s>\n", colorMenuJS);
hPrintf("<OPTION SELECTED VALUE = \"#000000\" style=\"background-color:#000000;\" >\n");
hPrintf("<OPTION value = \"#333333\" style=\"background-color:#333333;\" >\n");
hPrintf("<OPTION VALUE = \"#555555\" style=\"background-color:#555555;\" >\n");
hPrintf("<OPTION VALUE = \"#777777\" style=\"background-color:#777777;\" >\n");
hPrintf("<OPTION VALUE = \"#999999\" style=\"background-color:#999999;\" >\n");
hPrintf("<OPTION VALUE = \"#bbbbbb\" style=\"background-color:#bbbbbb;\" >\n");
hPrintf("<OPTION VALUE = \"#dddddd\" style=\"background-color:#dddddd;\" >\n");
hPrintf("<OPTION VALUE = \"#ff0000\" style=\"background-color:#ff0000;\" >\n");
hPrintf("<OPTION VALUE = \"#dd2200\" style=\"background-color:#dd2200;\" >\n");
hPrintf("<OPTION VALUE = \"#bb4400\" style=\"background-color:#bb4400;\" >\n");
hPrintf("<OPTION VALUE = \"#996600\" style=\"background-color:#996600;\" >\n");
hPrintf("<OPTION VALUE = \"#778800\" style=\"background-color:#778800;\" >\n");
hPrintf("<OPTION VALUE = \"#55aa00\" style=\"background-color:#55aa00;\" >\n");
hPrintf("<OPTION VALUE = \"#33cc00\" style=\"background-color:#33cc00;\" >\n");
hPrintf("<OPTION VALUE = \"#00ff00\" style=\"background-color:#00ff00;\" >\n");
hPrintf("</SELECT>\n");
}

static char *encodedHgcReturnUrl(int hgsid)
/* Return a CGI-encoded hgSession URL with hgsid.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "http://%s/cgi-bin/hgc?%s&g=%s&c=%s&o=%d&l=%d&r=%d&db=%s&i=0",
    cgiServerName(), cartSidUrlString(cart), WIKI_TRACK_TABLE, seqName,
	winStart, winStart, winEnd, database);
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

static void offerLogin(char *loginType)
/* display login prompts to the wiki when user isn't already logged in */
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
hPrintf("<FORM ID=\"editForm\" NAME=\"editForm\" ACTION=\"%s\">\n\n", hgcName());
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
/* fetch page from wiki in rendered form, strip it of edit URLs,
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

static struct wikiTrack *findItem(char *wikiItemId)
/* given a wikiItemId return the row from the table */
{
struct wikiTrack *item;
char query[256];
struct sqlConnection *conn = hConnectCentral();

safef(query, ArraySize(query), "SELECT * FROM %s WHERE id='%s' limit 1",
	WIKI_TRACK_TABLE, wikiItemId);

item = wikiTrackLoadByQuery(conn, query);
if (NULL == item)
    errAbort("display wiki item: failed to load item '%s'", wikiItemId);
hDisconnectCentral(&conn);

return item;
}

static struct htmlPage *fetchEditPage(char *descriptionKey)
/* fetch edit page for descriptionKey page name in wiki */
{
struct htmlCookie *cookie;
char wikiPageUrl[512];

/* must pass the session cookie from the wiki in order to edit */
AllocVar(cookie);
cookie->name = cloneString(cfgOption(CFG_WIKI_SESSION_COOKIE));
cookie->value = cloneString(findCookieData(cookie->name));

/* fetch the edit page to get the wpEditToken, and current contents */
safef(wikiPageUrl, sizeof(wikiPageUrl), "%s/index.php/%s?action=edit",
	cfgOptionDefault(CFG_WIKI_URL, NULL), descriptionKey);

char *fullText = htmlSlurpWithCookies(wikiPageUrl,cookie);
struct htmlPage *page = htmlPageParseOk(wikiPageUrl, fullText);
/* fullText pointer is placed in page->fullText */

return (page);
}

static boolean emailVerified()
/* TRUE indicates email has been verified for this wiki user */
{
struct htmlPage *page = fetchEditPage(TEST_EMAIL_VERIFIED);
char *stringFound = stringIn(EMAIL_NEEDS_TO_BE_VERIFIED, page->fullText);
htmlPageFree(&page);
if (NULL == stringFound)
    return TRUE;
else
    return FALSE;
}

static void createPageHelp(char *pageFileName)
/* find the specified html help page and display it, or issue a missing
 *	page message so the site administrator can fix it.
 */
{
char helpName[PATH_LEN], *helpBuf;

hPrintf("<HR />\n");

safef(helpName, ArraySize(helpName), "%s%s/%s.html", hDocumentRoot(), HELP_DIR,
	pageFileName);
if (fileExists(helpName))
    readInGulp(helpName, &helpBuf, NULL);
else
    {
    char missingHelp[512];
    safef(missingHelp, ArraySize(missingHelp),
        "<P>(missing help text file in %s)</P>\n", helpName);
    helpBuf = cloneString(missingHelp);
    }
puts(helpBuf);
}

static void displayItem(struct wikiTrack *item, char *userName)
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
    {
    startEditForm(G_DELETE_WIKI_ITEM);
    webPrintLinkTableStart();
    webPrintLinkCellStart();
    hPrintf("Owner '%s' has deletion rights&nbsp;&nbsp;", item->owner);
    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    cgiMakeButton("submit", "DELETE");
    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    hPrintf("&nbsp;(no questions asked)");
    webPrintLinkCellEnd();
    webPrintLinkTableEnd();
    char idString[128];
    safef(idString, ArraySize(idString), "%d", item->id);
    cgiMakeHiddenVar("i", idString);
    hPrintf("</FORM>");
    }

if (NULL != userName)
    hPrintf("<B>Mark this wiki article as <em>"
	"<A HREF=\"%s/index.php/%s?action=watch\" TARGET=_blank>watched</A>"
	"</em> to receive email notices of any comment additions.</B><BR />\n",
	   url, item->descriptionKey);

hPrintf("<B>Description and comments from the wiki article: "
    "<A HREF=\"%s/index.php/%s\" TARGET=_blank>%s</A> are shown below:</B><HR>\n",
       url, item->descriptionKey, item->descriptionKey);
if (strippedRender)
    {
    hPrintf("\n%s\n<HR>\n", strippedRender);
    freeMem(strippedRender);
    }
else
    hPrintf("<HR>\n(no comments for this item at the current time)"
	"<BR /><HR>\n");

if (NULL == userName)
    {
    offerLogin("add comments to items on");
    }
else
    {
    if (! emailVerified())
	{
	hPrintf("<P>%s.  %s <A HREF=\"%s/index.php/Special:Preferences\" "
	    "TARGET=_blank>user preferences.</A></P>\n",
	    EMAIL_NEEDS_TO_BE_VERIFIED, USER_PREFERENCES_MESSAGE, 
		cfgOptionDefault(CFG_WIKI_URL, NULL));
	}
    else
	{
	startEditForm(G_ADD_WIKI_COMMENTS);
	webPrintLinkTableStart();
	/* first row is a title line */
	char label[256];
	safef(label, ArraySize(label),
	    "<B>'%s' adding comments to item '%s'</B>\n", userName, item->name);
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
	cgiMakeHiddenVar("i", idString);
	hPrintf("</FORM>");

	hPrintf("For extensive edits, it may be more convenient to edit the ");
	hPrintf("wiki article <A HREF=\"%s/index.php/%s\" TARGET=_blank>%s</A> "
	   "for this item's description", url, item->descriptionKey,
		item->descriptionKey);
	createPageHelp("wikiTrackAddCommentHelp");
	}
    }
}	/*	displayItem()	*/

static void outputJavaScript()
/* java script functions used in the create item form */
{
hPrintf("<SCRIPT>\n");

hPrintf("function updateColorSelectBox() {\n"
" var form = document.getElementById(\"editForm\");\n"
" document.editForm.noName.style.display='inline';\n"
" document.editForm.noName.select();\n"
" document.editForm.noName.style.display='none';\n"
" form.itemColor.style.background = form.itemColor[form.itemColor.selectedIndex].value;\n"
" form.itemColor.style.color = form.itemColor[form.itemColor.selectedIndex].value;\n"
"}\n");
hPrintf("</SCRIPT>\n");
}

void doWikiTrack(char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle item clicks on wikiTrack - may create new items */
{
char *userName = NULL;

if (wikiTrackEnabled(&userName) && sameWord("0", wikiItemId))
    {
    cartWebStart(cart, "%s", "User Annotation Track: Create new item");
    if (NULL == userName)
	{
	offerLogin("add new items to");
	cartHtmlEnd();
	return;
	}

    if (! emailVerified())
	hPrintf("<P>%s.  %s <A HREF=\"%s/index.php/Special:Preferences\" "
	    "TARGET=_blank>user preferences.</A></P>\n",
	    EMAIL_NEEDS_TO_BE_VERIFIED, USER_PREFERENCES_MESSAGE, 
		cfgOptionDefault(CFG_WIKI_URL, NULL));
    else
	{
	outputJavaScript();
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
	/* seventh row is item color pull-down menu */
	webPrintWideCellStart(2, HG_COL_TABLE);
	hPrintf("<B>item color:&nbsp;</B>");
	colorMenuOutput();
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
	hPrintf("<FORM NAME=\"cancel\" ACTION=\"%s\">", hgTracksName());
	cgiMakeButton("cancel", "cancel");
	hPrintf("</FORM>");
	webPrintLinkCellEnd();
	webPrintLinkTableEnd();
	createPageHelp("wikiTrackCreateItemHelp");
	}
    }
else
    {
    struct wikiTrack *item = findItem(wikiItemId);
    cartWebStart(cart, "%s (%s)", "User Annotation Track", item->name,
	wikiItemId);
    /* if we can get the hgc clicks to add item id to the incoming data,
     *	then use that item Id here
     */
    displayItem(item, userName);
    }

cartHtmlEnd();
}	/*	void doWikiTrack()	*/

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

static void addDescription(char * descriptionKey, char *itemName)
{
char *newComments = cartNonemptyString(cart, NEW_ITEM_COMMENT);
struct dyString *content = newDyString(1024);

/* was nothing changed in the add comments entry box ? */
if (sameWord(ADD_ITEM_COMMENT_DEFAULT,newComments))
    return;

#ifdef NOT
struct htmlCookie *cookie;
char wikiPageUrl[512];

/* must pass the session cookie from the wiki in order to edit */
AllocVar(cookie);
cookie->name = cloneString(cfgOption(CFG_WIKI_SESSION_COOKIE));
cookie->value = cloneString(findCookieData(cookie->name));

/* fetch the edit page to get the wpEditToken, and current contents */
safef(wikiPageUrl, sizeof(wikiPageUrl), "%s/index.php/%s?action=edit",
	cfgOptionDefault(CFG_WIKI_URL, NULL), descriptionKey);

char *fullText = htmlSlurpWithCookies(wikiPageUrl,cookie);
struct htmlPage *page = htmlPageParseOk(wikiPageUrl, fullText);
#endif

struct htmlPage *page = fetchEditPage(descriptionKey);

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
"[http://%s/cgi-bin/hgTracks?db=%s&wikiTrack=pack&position=%s:%d-%d %s %s]"
	"&nbsp;&nbsp;<B>'%s'</B>&nbsp;&nbsp;''created: ~~~~''<BR /><BR />\n",
	NEW_ITEM_CATEGORY,
	    cfgOptionDefault(CFG_WIKI_BROWSER, DEFAULT_BROWSER), database,
		seqName, winStart, winEnd, database, newPos, itemName);
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
}	/*	static void addDescription()	*/

static void updateLastModifiedDate(int id)
/* set lastModifiedDate to now() */
{
char query[512];
struct sqlConnection *conn = hConnectCentral();

safef(query, ArraySize(query),
    "UPDATE %s set lastModifiedDate=now() WHERE id='%d'",
	WIKI_TRACK_TABLE, id);
sqlUpdate(conn,query);
hDisconnectCentral(&conn);
}

static void deleteItem(int id)
/* delete the item with specified id */
{
char query[512];
struct sqlConnection *conn = hConnectCentral();
safef(query, ArraySize(query), "DELETE FROM %s WHERE id='%d'",
	WIKI_TRACK_TABLE, id);
sqlUpdate(conn,query);
hDisconnectCentral(&conn);
}

void doDeleteWikiItem(char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle delete item clicks for wikiTrack */
{
char *userName = NULL;
struct wikiTrack *item = findItem(wikiItemId);

cartWebStart(cart, "%s (%s)", "User Annotation Track, deleted item: ",
	item->name);
if (NULL == wikiItemId)
    errAbort("delete wiki item: NULL wikiItemId");
if (! wikiTrackEnabled(&userName))
    errAbort("delete wiki item: wiki track not enabled");
deleteItem(sqlSigned(wikiItemId));
hPrintf("<BR />\n");
hPrintf("<FORM ACTION=\"%s\">", hgTracksName());
cgiMakeButton("submit", "Return to tracks display");
hPrintf("</FORM>");
hPrintf("<BR />\n");
cartHtmlEnd();
}

void doAddWikiComments(char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle add comment item clicks for wikiTrack */
{
char *userName = NULL;
struct wikiTrack *item = findItem(wikiItemId);

cartWebStart(cart, "%s (%s)", "User Annotation Track", item->name);
if (NULL == wikiItemId)
    errAbort("add wiki comments: NULL wikiItemId");
if (! wikiTrackEnabled(&userName))
    errAbort("add wiki comments: wiki track not enabled");

addDescription(item->descriptionKey, item->name);
updateLastModifiedDate(sqlSigned(wikiItemId));
displayItem(item, userName);
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
char *color = cartUsualString(cart, NEW_ITEM_COLOR, "#000000");
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
hDisconnectCentral(&conn);

cartWebStart(cart, "%s %s", "User Annotation Track, created new item: ",
	newItemName);

addDescription(descriptionKey, newItemName);

char wikiItemId[64];
safef(wikiItemId,ArraySize(wikiItemId),"%d", id);
struct wikiTrack *item = findItem(wikiItemId);
displayItem(item, userName);

cartHtmlEnd();
}	/*	void doCreateWikiItem()	*/
