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
#include "grp.h"
#include "wikiLink.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.2 2007/05/22 22:18:30 hiram Exp $";

#define NEW_ITEM_SCORE "newItemScore"
#define NEW_ITEM_STRAND "newItemStrand"
#define NEW_ITEM_CLASS "newItemClass"
#define NEW_ITEM_COMMENT "newItemComment"
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
      cfgOptionDefault("wikiTrack.URL", NULL), retEnc);
freez(&retEnc);
return(cloneString(buf));
}

static void displayItem(char *itemName, char *userName)
{ 
char *url = cfgOptionDefault("wikiTrack.URL", NULL);
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
    cgiMakeTextVar("i", "defaultName", 18);
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
struct dyString *wikiItemName = newDyString(256);
struct sqlConnection *conn = hConnectCentral();
char *userName = NULL;
char *color = "000000";
int score = 0;
struct wikiTrack *newItem;

if (! wikiTrackEnabled(&userName))
    errAbort("create new wiki item: wiki track not enabled");

score = sqlSigned(cartUsualString(cart, NEW_ITEM_SCORE, ITEM_SCORE_DEFAULT));
pos = stripCommas(cartOptionalString(cart, "getDnaPos"));
if (NULL == pos)
    errAbort("create new wiki item: called incorrectly, without getDnaPos");

hgParseChromRange(pos, &chrName, &itemStart, &itemEnd);

dyStringPrintf(wikiItemName,"BED:%s:%s:%d-%d:%s_%s", database, chrName,
    itemStart+1, itemEnd, plusStrand ? "%2B" : "%2D",
	cgiEncode(itemName));

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
newItem->descriptionKey = cloneString(wikiItemName->string);

wikiTrackSaveToDbEscaped(conn, newItem, WIKI_TRACK_TABLE, 1024);

wikiTrackFree(&newItem);

char query[256];
safef(query, ArraySize(query), "UPDATE %s set creationDate=now(),lastModifiedDate=now() WHERE name='%s'", WIKI_TRACK_TABLE, itemName);
sqlUpdate(conn,query);

displayItem(itemName, userName);

freeDyString(&wikiItemName);
cartHtmlEnd();
}
