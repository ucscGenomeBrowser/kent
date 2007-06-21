/* wikiTrack - handle the wikiTrack section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "web.h"
#include "hgConfig.h"
#include "hgGene.h"
#include "htmlPage.h"
#include "hgColors.h"
#include "hdb.h"
#include "wikiLink.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.2 2007/06/21 21:59:16 hiram Exp $";

static char *hgGeneUrl()
{
static char retBuf[1024];
safef(retBuf, ArraySize(retBuf), "cgi-bin/hgGene?%s=1&%s",
	hggDoWikiTrack, cartSidUrlString(cart));
return retBuf;
}

static char *encodedHgGeneReturnUrl()
/* Return a CGI-encoded hgGene URL with hgsid.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "http://%s/%s", cgiServerName(), hgGeneUrl());
return cgiEncode(retBuf);
}   

static char *wikiTrackUserLoginUrl()
/* Return the URL for the wiki user login page. */
{
char *retEnc = encodedHgGeneReturnUrl();
char buf[2048];

if (! wikiLinkEnabled())
    errAbort("wikiLinkUserLoginUrl called when wiki is not enabled (specified "
             "in hg.conf).");

safef(buf, sizeof(buf),
      "%s/index.php?title=Special:UserloginUCSC&returnto=%s",
      cfgOptionDefault(CFG_WIKI_URL, NULL), retEnc);
freez(&retEnc);
return(cloneString(buf));
}

static void offerLogin()
/* display login prompts to the wiki when user isn't already logged in */
{
char *wikiHost = wikiLinkHost();
char *loginUrl = wikiTrackUserLoginUrl();
printf("<P>Please login to add annotations to this UCSC gene.</P>\n");
printf("<P>The login page is handled by our "
       "<A HREF=\"http://%s/\" TARGET=_BLANK>wiki system</A>:\n", wikiHost);
printf("<A HREF=\"%s\"><B>click here to login.</B></A><BR />\n",
	loginUrl);
printf("The wiki also serves as a forum for users "
       "to share knowledge and ideas.\n</P>\n");
freeMem(loginUrl);
}

#ifdef NOT
static void addDescription(struct wikiTrack *item, char *userName,
    char *seqName, int winStart, int winEnd)
{
char *newComments = cartNonemptyString(cart, NEW_ITEM_COMMENT);
struct dyString *content = newDyString(1024);

/* was nothing changed in the add comments entry box ? */
if (sameWord(ADD_ITEM_COMMENT_DEFAULT,newComments))
    return;

struct htmlPage *page = fetchEditPage(item->descriptionKey);

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

/* decide on whether adding comments to existing text, or starting a
 *	new article from scratch.
 *	This function could be extended to actually checking the current
 *	contents to see if the "Category:" or "created:" lines have been
 *	removed, and then restore them.
 */
if (wpTextbox1->curVal && (strlen(wpTextbox1->curVal) > 2))
    {
    char *rawText = fetchWikiRawText(item->descriptionKey);
    dyStringPrintf(content, "%s\n\n''comments added: ~~~~''\n\n",
	rawText);
    }
else
    {
    boolean recreateHeader = FALSE;
    char position[128];
    char *newPos;
    char *userSignature;
    /* In the case where this is a restoration of the header lines,
     *	may be a different creator than this user adding comments.
     *	So, get the header line correct to represent the actual creator.
     */
    if (sameWord(userName, item->owner))
	userSignature = cloneString("~~~~");
    else
	{
	struct dyString *tt = newDyString(1024);
	dyStringPrintf(tt, "[[User:%s|%s]] ", item->owner, item->owner);
	dyStringPrintf(tt, "%s", item->creationDate);
	userSignature = dyStringCannibalize(&tt);
	recreateHeader = TRUE;
	}
    snprintf(position, 128, "%s:%d-%d", seqName, winStart+1, winEnd);
    newPos = addCommasToPos(position);
    dyStringPrintf(content, "%s\n"
"[http://%s/cgi-bin/hgTracks?db=%s&wikiTrack=pack&position=%s:%d-%d %s %s]"
	"&nbsp;&nbsp;<B>'%s'</B>&nbsp;&nbsp;''created: %s''\n\n",
	NEW_ITEM_CATEGORY,
	    cfgOptionDefault(CFG_WIKI_BROWSER, DEFAULT_BROWSER), database,
		seqName, winStart+1, winEnd, database, newPos, item->name,
		userSignature);
    if (recreateHeader)
	dyStringPrintf(content, "\n\n''comments added: ~~~~''\n\n");
    }

if (sameWord(NEW_ITEM_COMMENT_DEFAULT,newComments))
    dyStringPrintf(content, "%s\n\n", NO_ITEM_COMMENT_SUPPLIED);
else
    dyStringPrintf(content, "%s\n\n", newComments);

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
#endif

static void addComments(struct wikiTrack *item, char *userName)
{
if (item)
    {
    hPrintf("add comments to %s, %s, %s:%d-%d<BR>\n",
	item->name, userName, curGeneChrom, curGeneStart, curGeneEnd);
    addDescription(item, userName, curGeneChrom,
	curGeneStart, curGeneEnd, cart, database);
/*
    cartRemove(cart, NEW_ITEM_COMMENT);
*/
    }
else
    hPrintf("starting new item comments here<BR>\n");
}

void doWikiTrack(struct sqlConnection *conn)
/* display wiki track business */
{
char *userName = NULL;
struct wikiTrack *item = findWikiItemByAlignID(database, curGeneId);
char title[1024];

safef(title,ArraySize(title), "UCSC gene annotations %s", curGeneId);
cartWebStart(cart, title);

/* we already know the wiki track is enabled since we are here,
 *	now calling this just to see if user is logged into the wiki
 */
if(!wikiTrackEnabled(&userName))
    errAbort("wikiTrackPrint: called when wiki track is not enabled");

if (!cartVarExists(cart, hggDoWikiAddComment))
    cartRemove(cart, NEW_ITEM_COMMENT);

if (cartVarExists(cart, NEW_ITEM_COMMENT))
    hPrintf("%s cart var exists '%s'<BR>\n", NEW_ITEM_COMMENT,
	cartUsualString(cart, NEW_ITEM_COMMENT, "no comment"));
else
    hPrintf("%s cart var does not exist<BR>\n", NEW_ITEM_COMMENT);

if (cartVarExists(cart, hggDoWikiAddComment))
    addComments(item, userName);

if (NULL != item)
    {
    displayComments(item);
    }
else
    {
    hPrintf("<em>(no annotations for this gene at this time)</em><BR>\n<HR>\n");
    }

if (isEmpty(userName))
    offerLogin();
else
    {
    hPrintf("<FORM ID=\"hgg_wikiAddComment\" NAME=\"hgg_wikiAddComment\" "
	"METHOD=\"POST\" ACTION=\"../cgi-bin/hgGene\">\n\n");
    cartSaveSession(cart);
    cgiMakeHiddenVar(hggDoWikiTrack, "1");
    cgiMakeHiddenVar(hggDoWikiAddComment, "1");
    cgiMakeHiddenVar("db", database);
    cgiMakeHiddenVar("hgg_gene", curGeneId);
    webPrintLinkTableStart();
    /* first row is a title line */
    char label[256];
    safef(label, ArraySize(label),
	"'%s' adding comments to gene '%s'\n", userName, curGeneId);
    webPrintWideLabelCell(label, 2);
    webPrintLinkTableNewRow();
    /* second row is initial comment/description text entry */
    webPrintWideCellStart(2, HG_COL_TABLE);
    hPrintf("<B>add comments:</B><BR />");
    cgiMakeTextArea(NEW_ITEM_COMMENT, ADD_ITEM_COMMENT_DEFAULT, 3, 40);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    hPrintf("<TD BGCOLOR=\"#%s\" ALIGN=\"CENTER\" VALIGN=\"TOP\">",
	    HG_COL_TABLE);
    cgiMakeButton("submit", "add comments");
    hPrintf("\n</FORM>\n");
    webPrintLinkCellEnd();
    hPrintf("<TD BGCOLOR=\"#%s\" ALIGN=\"CENTER\" VALIGN=\"TOP\">",
	    HG_COL_TABLE);
    webPrintLinkCellEnd();
    webPrintLinkTableEnd();

    if (NULL != item)
	{
	char *url = cfgOptionDefault(CFG_WIKI_URL, NULL);
	hPrintf("For extensive edits, it may be more convenient to edit the ");
	hPrintf("wiki article <A HREF=\"%s/index.php/%s\" TARGET=_blank>%s</A> "
	   "for this item's description", url, item->descriptionKey,
		item->descriptionKey);
	}
    createPageHelp("wikiTrackAddCommentHelp");
    }

cartWebEnd();
}
