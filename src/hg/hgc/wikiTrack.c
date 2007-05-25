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

static char const rcsid[] = "$Id: wikiTrack.c,v 1.3 2007/05/25 21:33:37 hiram Exp $";

#define NEW_ITEM_SCORE "newItemScore"
#define NEW_ITEM_STRAND "newItemStrand"
#define NEW_ITEM_CLASS "newItemClass"
#define NEW_ITEM_COMMENT "newItemComment"
#define NEW_ITEM_NAME "defaultName"
#define ITEM_NOT_CLASSIFIED "Not classified"
#define ITEM_SCORE_DEFAULT "1000"

static char *encodedHgcReturnUrl(int hgsid)
/* Return a CGI-encoded hgSession URL with hgsid.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "http://%s/cgi-bin/hgc?%s&g=%s&c=%s&o=%d&l=%d&r=%d",
    cgiServerName(), cartSidUrlString(cart), WIKI_TRACK_TABLE, seqName,
	winStart, winStart, winEnd);
/*
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

static void displayItem(char *itemName, char *userName)
/* given an itemName and possibly a wiki userName, fetch the item data
 *	from the hgcentral database, and the item description from
 *	the wiki
 */
{ 
char *url = cfgOptionDefault(CFG_WIKI_URL, NULL);
struct wikiTrack *item;
char query[256];
struct sqlConnection *conn = hConnectCentral();
cartWebStart(cart, "%s (%s)", "User Annotation Track", itemName);
safef(query, ArraySize(query), "SELECT * FROM %s WHERE name='%s' AND db='%s' limit 1",
    WIKI_TRACK_TABLE, itemName, database);
item = wikiTrackLoadByQuery(conn, query);
if (NULL == item)
    errAbort("display wiki item: failed to load item %s", itemName);
hDisconnectCentral(&conn);

/*safef(wikiPageUrl, sizeof(wikiPageUrl), * "%s/index.php/%s?action=raw",*/
/* fetch previous description comments from the wiki */
char wikiPageUrl[512];
safef(wikiPageUrl, sizeof(wikiPageUrl), "%s/index.php/%s?action=render",
	cfgOptionDefault(CFG_WIKI_URL, NULL), item->descriptionKey);
struct lineFile *lf = netLineFileMayOpen(wikiPageUrl);
/*struct dyString *wikiPage = netSlurpUrl(wikiPageUrl);*/
struct dyString *wikiPage = newDyString(1024);
if (lf)
    {
    char *line;
    int lineSize;
    while (lineFileNext(lf, &line, &lineSize))
	dyStringPrintf(wikiPage, "%s\n", line);
    lineFileClose(&lf);
    }

hPrintf("<B>Classification group:&nbsp;</B>%s<BR>\n", item->class);
printPosOnChrom(item->chrom, item->chromStart, item->chromEnd,
    item->strand, FALSE, item->name);
hPrintf("<B>Score:&nbsp;</B>%u<BR>\n", item->score);
hPrintf("<B>Created </B>%s<B> by:&nbsp;</B>", item->creationDate);
hPrintf("<A HREF=\"%s/index.php/User:%s\" TARGET=_blank>%s</A><BR>\n", url,
    item->owner, item->owner);
hPrintf("<B>Last update:&nbsp;</B>%s<BR>\n", item->lastModifiedDate);
if (NULL == userName)
    {
    hPrintf("<P>To add comments to this item: \n");
    hPrintf("<A HREF=\"%s\"><B>login to genomewiki</B></A></P>\n",
       wikiTrackUserLoginUrl(cartSessionId(cart)));
    }
else
    {
    hPrintf("Add ");
hPrintf("<A HREF=\"%s/index.php?title=%s&amp;action=edit\" TARGET=_blank>comments</A> to this item's description", url, item->descriptionKey);
    }
if (wikiPage->string)
    hPrintf("comments from '%s'<BR>\n%s<BR>\n", wikiPageUrl, wikiPage->string);
else
    hPrintf("comments from '%s'<BR>\nempty<BR>\n", wikiPageUrl);
freeDyString(&wikiPage);
hPrintf("<hr>\n<PRE>");
dumpCookieList();
hPrintf("</PRE>\n<hr>\n");
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
	hPrintf("<P>To add items to the wiki track, \n");
	hPrintf("<A HREF=\"%s\"><B>login to genomewiki</B></A></P>\n",
	   wikiTrackUserLoginUrl(cartSessionId(cart)));
	cartHtmlEnd();
	return;
	}
    hPrintf("<FORM ACTION=\"%s\">\n\n", hgcName());
    cartSaveSession(cart);
    cgiMakeHiddenVar("g", G_CREATE_WIKI_ITEM);
    cgiContinueHiddenVar("c");
    cgiContinueHiddenVar("o");
    hPrintf("\n");
    cgiContinueHiddenVar("l");
    cgiContinueHiddenVar("r");
    hPrintf("\n");

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
    hPrintf("<B>initial comments/description:</B><BR>");
    cgiMakeTextArea(NEW_ITEM_COMMENT, "enter description and comments", 5, 40);
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
    displayItem(itemName, userName);
    }

cartHtmlEnd();
}

/*
 *  a login to the wiki created cookies in the ucsc.edu domain:
wikiHiramDb_hc1_UserName
wikiHiramDb_hc1_UserID
wikiHiramDb_hc1__session
 *  using the hgwdev-hiram browser created the cookie in the
 *  hgwdev-hiram.cse.ucsc.edu domain:
hguid.hgwdev-hiram
 *  logging out of the wiki created:
wikiHiramDb_hc1_LoggedOut
and left UserName and __session

while logged out:
wikiHiramDb_hc1__session=nbloaafcd8vmjefkavo69566s4 (0)
wikiHiramDb_hc1_UserName=Hiram (0)
hguid.hgwdev-hiram=2389762 (0)
wikiHiramDb_hc1_LoggedOut=20070524212656 (0)
after login:
wikiHiramDb_hc1__session=nbloaafcd8vmjefkavo69566s4 (0)
wikiHiramDb_hc1_UserName=Hiram (0)
hguid.hgwdev-hiram=2389762 (0)
wikiHiramDb_hc1_LoggedOut=20070524212656 (0)
wikiHiramDb_hc1_UserID=1 (0)
! ? no difference ?

and edit request returns Vars:
X - wpSection	input	hidden	
wpStarttime	input	hidden	20070524213126
X - wpEdittime	input	hidden	20070524213126
wpScrolltop	input	hidden	
X - wpTextbox1	textarea	n/a	

X - wpSummary	input	text	
X - wpMinoredit	input	checkbox	n/a
wpWatchthis	input	checkbox	n/a
X - wpSave	input	submit	n/a
wpPreview	input	submit	n/a
wpDiff	input	submit	n/a
X - wpEditToken	input	hidden	7d5150e607336e72655e685cd60c6bae
wpAutoSummary	input	hidden	d41d8cd98f00b204e9800998ecf8427e
search	input	text	
go	input	submit	n/a
fulltext	input	submit	n/a

The bulk page creator
http://meta.wikimedia.org/wiki/MediaWiki_Bulk_Page_Creator
mentions:
$submit_vars['wpTextbox1'] = $body;
$submit_vars['wpSummary'] = "";
$submit_vars['wpSection'] = "";
$submit_vars['wpEdittime'] = "";
$submit_vars['wpMinoredit'] = "1";
$submit_vars['wpSave'] = "Save page";
$submit_vars['wpEditToken'] = $token;

 */
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


static void addNewItemDescription(char * descriptionKey, char *userName)
{
char *newDescription = cartNonemptyString(cart, NEW_ITEM_COMMENT);
struct dyString *content = newDyString(1024);
struct htmlCookie *cookies = NULL;
struct htmlCookie *cookie;
char wikiPageUrl[512];
safef(wikiPageUrl, sizeof(wikiPageUrl), "%s/index.php/%s?action=edit",
	cfgOptionDefault(CFG_WIKI_URL, NULL), descriptionKey);
printf("addNewItem: '%s'<BR>\n", wikiPageUrl);
struct cgiVar *v;
for (v=cookieList; v != NULL; v = v->next)
    {
    if (startsWith("wiki", v->name) && endsWith(v->name, "_session"))
	{
	AllocVar(cookie);
	cookie->name = cloneString(v->name);
	cookie->value = cloneString(v->val);
	slAddHead(&cookies,cookie);
	}
    }
hPrintf("<HR>\n<B>Sending cookies:</B><PRE>\n");
for (cookie = cookies; cookie != NULL; cookie = cookie->next)
    {
    printf("%s\t%s\t%s\t%s\t%s\t%s\n",
        cookie->name, cookie->value, naForNull(cookie->domain),
        naForNull(cookie->path), naForNull(cookie->expires),
        (cookie->secure ? "SECURE" : "UNSECURE"));
    }
hPrintf("</PRE>\n");
char *fullText = htmlSlurpWithCookies(wikiPageUrl,cookies);
struct htmlPage *page = htmlPageParseOk(wikiPageUrl, fullText);
hPrintf("<HR>\n<B>Cookies returned:</B><PRE>\n");
for (cookie = page->cookies; cookie != NULL; cookie = cookie->next)
    {
    printf("%s\t%s\t%s\t%s\t%s\t%s\n",
        cookie->name, cookie->value, naForNull(cookie->domain),
        naForNull(cookie->path), naForNull(cookie->expires),
        (cookie->secure ? "SECURE" : "UNSECURE"));
    }
hPrintf("</PRE>\n");
hPrintf("<HR>\n<B>Vars returned:</B><PRE>\n");
struct htmlForm *form;
struct htmlFormVar *var;
for (form = page->forms; form != NULL; form = form->next)
    {
    for (var = form->vars; var != NULL; var = var->next)
        htmlFormVarPrint(var, stdout, "");
    }
hPrintf("</PRE>\n");
hPrintf("<HR>\n<B>Forms returned:</B><PRE>\n");
for (form = page->forms; form != NULL; form = form->next)
        htmlFormPrint(form, stdout);
hPrintf("</PRE>\n");

struct htmlForm *strippedEditForm;
AllocVar(strippedEditForm);

struct htmlForm *currentEditForm = htmlFormGet(page,"editform");
if (NULL == currentEditForm)
    errAbort("addNewItemDescription: can not get editform ?");
strippedEditForm->name = cloneString(currentEditForm->name);
/*strippedEditForm->method = cloneString(currentEditForm->method);*/
strippedEditForm->method = cloneString("POST");
strippedEditForm->startTag = currentEditForm->startTag;
strippedEditForm->endTag = currentEditForm->endTag;

/*
struct htmlFormVar *wpTextbox1 = htmlPageGetVar(page, currentEditForm, "Textbox1");
if (wpTextbox1->curVal)
    dyStringPrintf(content, "%s\n%s\nsignature here",
	wpTextbox1->curVal, newDescription);
else
*/
dyStringPrintf(content, "==NEW ITEM==\n\n%s\nsignature here", newDescription);

htmlCloneFormVarSet(currentEditForm, strippedEditForm,
	"wpTextbox1", content->string);
htmlCloneFormVarSet(currentEditForm, strippedEditForm,
	"wpSummary", "");
htmlCloneFormVarSet(currentEditForm, strippedEditForm,
	"wpSection", "");
htmlCloneFormVarSet(currentEditForm, strippedEditForm,
	"wpMinoredit", "1");
htmlCloneFormVarSet(currentEditForm, strippedEditForm,
	"wpSave", "Save page");
/*
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpStarttime", NULL);
*/
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpEdittime", NULL);
htmlCloneFormVarSet(currentEditForm, strippedEditForm, "wpEditToken", NULL);
/*
htmlPageSetVar(page,strippedEditForm, "wpRecreate", "1");
*/

htmlPageSetVar(page,currentEditForm, "wpTextbox1", content->string);
htmlPageSetVar(page,currentEditForm, "wpSummary", "");
htmlPageSetVar(page,currentEditForm, "wpSection", "");
htmlPageSetVar(page,currentEditForm, "wpMinoredit", "1");
htmlPageSetVar(page,currentEditForm, "wpSave", "Save page");
/*
struct htmlFormVar *wpEditToken = htmlPageGetVar(page, currentEditForm, "wpEditToken");
htmlPageSetVar(page,currentEditForm, "wpEditToken", wpEditToken->curVal);
*/
verboseSetLevel(4);
char newUrl[1024];
/* fake out htmlPageFromForm since it doesn't understand : */
safef(newUrl, ArraySize(newUrl), "%s%s",
	"http://genomewiki.ucsc.edu", currentEditForm->action);
char *fixedString = replaceChars(newUrl, "&amp;", "&");
verbose(3,"createItem: url: '%s'\n", fixedString);
currentEditForm->action = cloneString(fixedString);
strippedEditForm->action = cloneString(fixedString);
hPrintf("<PRE>\n");
hPrintf("Sending Form:\n================================================\n");
htmlFormPrint(strippedEditForm, stdout);
hPrintf("\n===============================================\n");
hPrintf("page url: '%s'\nform action: '%s'\n", page->url, strippedEditForm->action);
hPrintf("Sending Vars:\n");
for (var = strippedEditForm->vars; var != NULL; var = var->next)
    htmlFormVarPrint(var, stdout, "");
hPrintf("\n");
struct htmlPage *editPage = htmlPageFromForm(page,strippedEditForm,"submit", "Submit");
hPrintf("</PRE>\n<HR>");
hPrintf("%s", editPage->htmlText);
hPrintf("\n<HR>");
freeDyString(&content);
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
	"GenomeAnnotation:%s-%d", database, 1);

/*
struct dyString *wikiItemName = newDyString(256);
char *noBlanks = replaceChars(itemName," ","_");
dyStringPrintf(wikiItemName,"BED:%s:%s:%d-%d:%s:%s", database, chrName,
    itemStart+1, itemEnd, plusStrand ? "%2B" : "-",
	noBlanks);
freeDyString(&wikiItemName);
*/


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
    safef(query, ArraySize(query), "UPDATE %s set creationDate=now(),lastModifiedDate=now(),descriptionKey='%s',name='%s-%d' WHERE name='%s' AND db='%s'",
	WIKI_TRACK_TABLE, descriptionKey, database, id, itemName, database);
    
    }
else
    {
    safef(newItemName, ArraySize(newItemName), "%s", itemName);
    safef(query, ArraySize(query), "UPDATE %s set creationDate=now(),lastModifiedDate=now(),descriptionKey='%s' WHERE name='%s' AND db='%s'",
	WIKI_TRACK_TABLE, descriptionKey, itemName, database);
    }
sqlUpdate(conn,query);

addNewItemDescription(descriptionKey, userName);

displayItem(newItemName, userName);

cartHtmlEnd();
}
