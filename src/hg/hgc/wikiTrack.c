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

static char const rcsid[] = "$Id: wikiTrack.c,v 1.45 2008/05/29 23:39:02 hiram Exp $";

#define ITEM_SCORE_DEFAULT "1000"
#define ADD_ITEM_COMMENT_DEFAULT "add comments"

static char *colorMenuJS = "onchange=\"updateColorSelectBox();\" style=\"width:8em;\"";

static void colorMenuOutput()
/* the item color pull-down menu in the create item form */
{
hPrintf("<INPUT NAME=\"colorPullDown\" VALUE=\"\" SIZE=1 STYLE=\"display:none;\" >\n");

hPrintf("<SELECT NAME=\"itemColor\" style=\"width:8em; background-color:#000000;\" %s>\n", colorMenuJS);
hPrintf("<OPTION SELECTED VALUE = \"#000000\" style=\"background-color:#000000;\" >black</OPTION>\n");
hPrintf("<OPTION value = \"#0000ff\" style=\"background-color:#0000ff;\" >blue</OPTION>\n");
hPrintf("<OPTION VALUE = \"#9600c8\" style=\"background-color:#9600c8;\" >purple</OPTION>\n");
hPrintf("<OPTION VALUE = \"#ff0000\" style=\"background-color:#ff0000;\" >red</OPTION>\n");
hPrintf("<OPTION VALUE = \"#e67800\" style=\"background-color:#e67800;\" >orange</OPTION>\n");
hPrintf("<OPTION VALUE = \"#dcdc00\" style=\"background-color:#dcdc00;\" >yellow</OPTION>\n");
hPrintf("<OPTION VALUE = \"#00b400\" style=\"background-color:#00b400;\" >green</OPTION>\n");
hPrintf("<OPTION VALUE = \"#8c8c8c\" style=\"background-color:#8c8c8c;\" >gray</OPTION>\n");
hPrintf("</SELECT>\n");
}

static char *encodedHgcReturnUrl(int id)
/* Return a CGI-encoded hgc URL with hgsid and given id.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "http://%s/cgi-bin/hgc?%s&g=%s&c=%s&o=%d&l=%d&r=%d&db=%s&i=%d",
    cgiServerName(), cartSidUrlString(cart), WIKI_TRACK_TABLE, seqName,
	winStart, winStart, winEnd, database, id);
return cgiEncode(retBuf);
}   

static char *wikiTrackUserLoginUrl(int id)
/* Return the URL for the wiki user login page. */
{
char *retEnc = encodedHgcReturnUrl(id);
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

static void offerLogin(int id, char *loginType)
/* display login prompts to the wiki when user isn't already logged in */
{
char *wikiHost = wikiLinkHost();
char *loginUrl = wikiTrackUserLoginUrl(id);
printf("<P>Please login to %s the annotation track.</P>\n", loginType);
printf("<P>The login page is handled by our "
       "<A HREF=\"http://%s/\" TARGET=_BLANK>wiki system</A>:\n", wikiHost);
printf("<A HREF=\"%s\"><B>click here to login.</B></A><BR>\n", loginUrl);
printf("The wiki also serves as a forum for users "
       "to share knowledge and ideas.\n</P>\n");
freeMem(loginUrl);
freeMem(wikiHost);
webIncludeHelpFile("wikiTrackAddCommentHelp", TRUE);
webIncludeHelpFile("wikiTrack", TRUE);
}

static void startForm(char *name, char *actionType)
{
hPrintf("<FORM ID=\"%s\" NAME=\"%s\" ACTION=\"%s\">\n\n", name, name, hgcName());
cartSaveSession(cart);
cgiMakeHiddenVar("g", actionType);
cgiContinueHiddenVar("c");
cgiContinueHiddenVar("o");
hPrintf("\n");
cgiContinueHiddenVar("l");
cgiContinueHiddenVar("r");
hPrintf("\n");
}

static struct bed *multipleItems(struct wikiTrack *item)
{
struct sqlResult *sr;
char **row;
struct sqlConnection *wikiConn = wikiConnect();
char query[1024];
struct bed *bedList = NULL;

safef(query, ArraySize(query), "SELECT chrom,chromStart,chromEnd,id FROM %s "
    "WHERE descriptionKey='%s' ORDER BY chrom,chromStart;",
	WIKI_TRACK_TABLE, item->descriptionKey);
sr = sqlGetResult(wikiConn, query);
while ( (row = sqlNextRow(sr)) != NULL)
    {
    int elId = sqlUnsigned(row[3]);
    if (elId == item->id)
	continue;
    struct bed *bed;
    AllocVar(bed);
    bed->chrom = cloneString(row[0]);
    bed->chromStart = sqlUnsigned(row[1]);
    bed->chromEnd = sqlUnsigned(row[2]);
    bed->score = elId;
    slAddHead(&bedList,bed);
    }
sqlFreeResult(&sr);
slSort(&bedList, bedCmpExtendedChr);
wikiDisconnect(&wikiConn);
return bedList;
}

static void displayItem(struct wikiTrack *item, char *userName)
/* given an already fetched item, get the item description from
 *	the wiki.  Put up edit form(s) if userName is not NULL
 */
{ 
boolean geneAnnotation = FALSE;
char *url = cfgOptionDefault(CFG_WIKI_URL, NULL);

if (isNotEmpty(item->geneSymbol) && differentWord(item->geneSymbol,"0"))
    {
    hPrintf("<B>This is a UCSC gene annotation</B><BR>\n");
    geneAnnotation = TRUE;
    }
else
    hPrintf("<B>This is a genome location annotation, not a gene annotation.</B>\n");

printPosOnChrom(item->chrom, item->chromStart, item->chromEnd,
    item->strand, FALSE, item->name);
if (geneAnnotation)
    {
    struct bed *itemList = multipleItems(item);
    if (slCount(itemList) > 0)
	{
	hPrintf("&nbsp;&nbsp;This gene symbol <B>'%s'</B> is also found in the following "
	    "locations:<BR>\n", item->name);
	struct bed *el;
	for (el = itemList; el; el = el->next)
	    {
	    printf("&nbsp;&nbsp;&nbsp;&nbsp;<B>%s:</B> "
	       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
	       item->name, hgTracksPathAndSettings(), database, el->chrom,
		el->chromStart+1, el->chromEnd);
	    printf("%s:%d-%d</A><BR>\n", el->chrom, el->chromStart+1,
		el->chromEnd);
	    }
	}
    }
hPrintf("<B>Created </B>%s<B> by:&nbsp;</B>", item->creationDate);
hPrintf("<A HREF=\"%s/index.php/User:%s\" TARGET=_blank>%s</A><BR>\n", url,
    item->owner, item->owner);
hPrintf("<B>Most recent quick update:&nbsp;</B>%s<BR>\n",
    item->lastModifiedDate);

boolean editor = isWikiEditor(userName);

if ((NULL != userName) &&
	(editor || (sameWord(userName, item->owner) && !geneAnnotation)))
    {
    hPrintf("<em>(comments for deleted items remain in the wiki, with a "
	"note<BR>&nbsp;&nbsp;that the item has been deleted.)</em><BR>\n");
    startForm("deleteForm", G_DELETE_WIKI_ITEM);
    char idString[128];
    safef(idString, ArraySize(idString), "%d", item->id);
    cgiMakeHiddenVar("i", idString);
    hPrintf("\n");
    webPrintLinkTableStart();
    webPrintLinkCellStart();

    if (editor && (differentWord(userName, item->owner) || geneAnnotation))
	hPrintf("Editor '%s' has deletion rights&nbsp;&nbsp;", userName);
    else
	hPrintf("Owner '%s' has deletion rights&nbsp;&nbsp;", item->owner);

    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    cgiMakeButton("submit", "DELETE");
    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    hPrintf("&nbsp;(no questions asked)");
    webPrintLinkCellEnd();
    webPrintLinkTableEnd();
    hPrintf("\n</FORM>\n");
    }

hPrintf("<B>View the wiki article "
  "<A HREF=\"%s/index.php/%s\" TARGET=_blank>%s:</A> to see "
	"existing comments.</B><BR>\n",
       url, item->descriptionKey, item->descriptionKey);

if (NULL != userName)
    hPrintf("<B>Mark this wiki article as <em>"
	"<A HREF=\"%s/index.php/%s?action=watch\" TARGET=_blank>watched</A>"
	"</em> to receive email notices of any comment additions.</B><BR>\n",
	   url, item->descriptionKey);


hPrintf("<HR>\n");

if (NULL == userName)
    {
    offerLogin(item->id, "add comments to items on");
    }
else if (emailVerified()) /* prints message when not verified */
    {
    startForm("addComments", G_ADD_WIKI_COMMENTS);
    char idString[128];
    safef(idString, ArraySize(idString), "%d", item->id);
    cgiMakeHiddenVar("i", idString);
    hPrintf("\n");
    webPrintLinkTableStart();
    /* first row is a title line */
    char label[256];
    safef(label, ArraySize(label),
	"'%s' quick add comments to item '%s'\n", userName, item->name);
    webPrintWideLabelCell(label, 2);
    webPrintLinkTableNewRow();
    /* second row is initial comment/description text entry */
    webPrintWideCellStart(2, HG_COL_TABLE);
    cgiMakeTextArea(NEW_ITEM_COMMENT, ADD_ITEM_COMMENT_DEFAULT, 3, 70);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    /*webPrintLinkCellStart(); more careful explicit alignment */
    hPrintf("<TD BGCOLOR=\"#%s\" ALIGN=\"CENTER\" VALIGN=\"TOP\">",
	    HG_COL_TABLE);
    cgiMakeButton("submit", "add comments");
    hPrintf("\n</FORM>\n");
    webPrintLinkCellEnd();
    /*webPrintLinkCellStart(); doesn't valign center properly */
    hPrintf("<TD BGCOLOR=\"#%s\" ALIGN=\"CENTER\" VALIGN=\"TOP\">",
	    HG_COL_TABLE);
    hPrintf("\n<FORM ID=\"cancel\" NAME=\"cancel\" ACTION=\"%s\">", hgTracksName());
    cgiMakeButton("cancel", "return to tracks display");
    hPrintf("\n</FORM>\n");
    webPrintLinkCellEnd();
    webPrintLinkTableEnd();

    hPrintf("For more extensive edits, please edit the ");
    hPrintf("wiki article <A HREF=\"%s/index.php/%s\" TARGET=_blank>%s</A> "
       "in the wiki editing system.<BR>", url, item->descriptionKey,
	    item->descriptionKey);
    webIncludeHelpFile("wikiTrackAddCommentHelp", TRUE);
    webIncludeHelpFile("wikiTrack", TRUE);
    }
}	/*	displayItem()	*/

static void outputJavaScript()
/* java script functions used in the create item form */
{
hPrintf("<SCRIPT TYPE=\"text/javascript\">\n");

hPrintf("function updateColorSelectBox() {\n"
" var form = document.getElementById(\"createItem\");\n"
" document.createItem.colorPullDown.style.display='inline';\n"
" document.createItem.colorPullDown.select();\n"
" document.createItem.colorPullDown.style.display='none';\n"
" form.itemColor.style.background = form.itemColor[form.itemColor.selectedIndex].value;\n"
" form.itemColor.style.color = form.itemColor[form.itemColor.selectedIndex].value;\n"
"}\n");
hPrintf("</SCRIPT>\n");
}

void doWikiTrack(char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle item clicks on wikiTrack - may create new items */
{
char *userName = NULL;

if (wikiTrackEnabled(database, &userName) && sameWord("0", wikiItemId))
    {
    cartWebStart(cart, "%s", "User Annotation Track: Create new item");
    if (NULL == userName)
	{
	offerLogin(0, "add new items to");
	return;
	}

    if (emailVerified()) /* prints message when not verified */
	{
	outputJavaScript();
	startForm("createItem", G_CREATE_WIKI_ITEM);

	webPrintLinkTableStart();
	/* first row is a title line */
	char label[256];
	safef(label, ArraySize(label), "Create new item, owner: '%s'\n",
	    userName);
	webPrintWideLabelCell(label, 2);
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
#ifdef NOT
	webPrintLinkTableNewRow();
	/* sixth row is item score text entry */
	webPrintWideCellStart(2, HG_COL_TABLE);
	hPrintf("<B>item score:&nbsp;</B>");
	cgiMakeTextVar(NEW_ITEM_SCORE, ITEM_SCORE_DEFAULT, 4);
	hPrintf("&nbsp;(range:&nbsp;0&nbsp;to&nbsp;%s)", ITEM_SCORE_DEFAULT);
	webPrintLinkCellEnd();
#endif
	webPrintLinkTableNewRow();
	/* seventh row is item color pull-down menu */
	webPrintWideCellStart(2, HG_COL_TABLE);
	hPrintf("<B>item color:&nbsp;</B>");
	colorMenuOutput();
	webPrintLinkCellEnd();
	webPrintLinkTableNewRow();
	/* seventh row is initial comment/description text entry */
	webPrintWideCellStart(2, HG_COL_TABLE);
	hPrintf("<B>initial comments/description:</B><BR>");
	cgiMakeTextArea(NEW_ITEM_COMMENT, NEW_ITEM_COMMENT_DEFAULT, 5, 70);
	webPrintLinkCellEnd();
	webPrintLinkTableNewRow();
	/* seventh row is the submit and cancel buttons */
	/*webPrintLinkCellStart(); more careful explicit alignment */
	hPrintf("<TD BGCOLOR=\"#%s\" ALIGN=\"CENTER\" VALIGN=\"TOP\">",
		HG_COL_TABLE);
	cgiMakeButton("submit", "create new item");
	hPrintf("\n</FORM>\n");
	webPrintLinkCellEnd();
	/*webPrintLinkCellStart(); doesn't valign center properly */
	hPrintf("<TD BGCOLOR=\"#%s\" ALIGN=\"CENTER\" VALIGN=\"TOP\">",
		HG_COL_TABLE);
	hPrintf("\n<FORM ID=\"cancel\" NAME=\"cancel\" ACTION=\"%s\">", hgTracksName());
	cgiMakeButton("cancel", "cancel");
	hPrintf("\n</FORM>\n");
	webPrintLinkCellEnd();
	webPrintLinkTableEnd();
	hPrintf("This entry form starts the comments for this new item.<BR>\n"
	    "Subsequent edits will be performed in the wiki editing system."
	    "<BR>\n");
	webIncludeHelpFile("wikiTrackCreateItemHelp", TRUE);
	webIncludeHelpFile("wikiTrack", TRUE);
	}

    }
else
    {
    struct wikiTrack *item = findWikiItemId(wikiItemId);
    cartWebStart(cart, "%s (%s)", "User Annotation Track", item->name,
	wikiItemId);
    /* if we can get the hgc clicks to add item id to the incoming data,
     *	then use that item Id here
     */
    displayItem(item, userName);
    }

}	/*	void doWikiTrack()	*/

static void updateLastModifiedDate(int id)
/* set lastModifiedDate to now() */
{
char query[512];
struct sqlConnection *wikiConn = wikiConnect();

safef(query, ArraySize(query),
    "UPDATE %s set lastModifiedDate=now() WHERE id='%d'",
	WIKI_TRACK_TABLE, id);
sqlUpdate(wikiConn,query);
wikiDisconnect(&wikiConn);
}

static void deleteItem(int id)
/* delete the item with specified id */
{
char query[512];
struct sqlConnection *wikiConn = wikiConnect();
safef(query, ArraySize(query), "DELETE FROM %s WHERE id='%d'",
	WIKI_TRACK_TABLE, id);
sqlUpdate(wikiConn,query);
wikiDisconnect(&wikiConn);
}

void doDeleteWikiItem(char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle delete item clicks for wikiTrack */
{
char *userName = NULL;
struct wikiTrack *item = findWikiItemId(wikiItemId);

cartWebStart(cart, "%s (%s)", "User Annotation Track, deleted item: ",
	item->name);
if (NULL == wikiItemId)
    errAbort("delete wiki item: NULL wikiItemId");
if (! wikiTrackEnabled(database, &userName))
    errAbort("delete wiki item: wiki track not enabled");

/*	this may be a multiple loci gene symbol annotation */
struct bed *itemList = multipleItems(item);
char comments[1024];
if (slCount(itemList) > 0)
    safef(comments,ArraySize(comments), "This item '''%s''' on assembly %s "
    "at %s:%d-%d (and %d other loci) has been deleted from the wiki track\n\n",
	item->name, item->db, item->chrom, item->chromStart, item->chromEnd,
	slCount(itemList));
else
    safef(comments,ArraySize(comments), "This item '''%s''' on assembly %s "
    "at %s:%d-%d has been deleted from the wiki track\n\n", item->name,
	item->db, item->chrom, item->chromStart, item->chromEnd);

prefixComments(item, comments, userName, seqName, winStart, winEnd,
    database, NULL, "(deleted item)");
deleteItem(sqlSigned(wikiItemId));
/*	this may be a multiple loci gene symbol annotation */
if (slCount(itemList) > 0)
    {
    struct bed *el;
    for (el = itemList; el; el = el->next)
	deleteItem(el->score);
    }
hPrintf("<BR>\n");
hPrintf("<FORM ID=\"delete\" NAME=\"delete\" ACTION=\"%s\">", hgTracksName());
cgiMakeButton("submit", "return to tracks display");
hPrintf("\n</FORM>\n");
hPrintf("<BR>\n");
webIncludeHelpFile("wikiTrack", TRUE);
}

void doAddWikiComments(char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle add comment item clicks for wikiTrack */
{
char *userName = NULL;
struct wikiTrack *item = findWikiItemId(wikiItemId);

cartWebStart(cart, "%s (%s)", "User Annotation Track", item->name);
if (NULL == wikiItemId)
    errAbort("add wiki comments: NULL wikiItemId");
if (! wikiTrackEnabled(database, &userName))
    errAbort("add wiki comments: wiki track not enabled");
if (NULL == userName)
    errAbort("add wiki comments: user not logged in ?");

addDescription(item, userName, seqName, winStart, winEnd, cart, database, NULL,
	NULL);
updateLastModifiedDate(sqlSigned(wikiItemId));
displayItem(item, userName);
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
struct sqlConnection *wikiConn = wikiConnect();
char *userName = NULL;
char *color = cartUsualString(cart, NEW_ITEM_COLOR, "#000000");
int score = 0;
struct wikiTrack *newItem;

if (! wikiTrackEnabled(database, &userName))
    errAbort("create new wiki item: wiki track not enabled");
if (NULL == userName)
    errAbort("create new wiki item: user not logged in ?");

#ifdef NOT
score = sqlSigned(cartUsualString(cart, NEW_ITEM_SCORE, ITEM_SCORE_DEFAULT));
#endif
score = 0;
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
newItem->geneSymbol = cloneString("0");

wikiTrackSaveToDbEscaped(wikiConn, newItem, WIKI_TRACK_TABLE, 1024);

int id = sqlLastAutoId(wikiConn);
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
sqlUpdate(wikiConn,query);
wikiDisconnect(&wikiConn);

cartWebStart(cart, "%s %s", "User Annotation Track, created new item: ",
	newItemName);

char wikiItemId[64];
safef(wikiItemId,ArraySize(wikiItemId),"%d", id);
struct wikiTrack *item = findWikiItemId(wikiItemId);

addDescription(item, userName, seqName, winStart, winEnd, cart, database, NULL,
	NULL);
displayItem(item, userName);
}	/*	void doCreateWikiItem()	*/
