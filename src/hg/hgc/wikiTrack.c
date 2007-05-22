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
#include "wikiLink.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.1 2007/05/22 19:10:12 hiram Exp $";

static char *encodedHgcReturnUrl(int hgsid)
/* Return a CGI-encoded hgSession URL with hgsid.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "http://%s/cgi-bin/hgc?hgsid=%d",
      cgiServerName(), hgsid);
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
    webPrintLinkTableStart();
    char label[256];
    safef(label, ArraySize(label), "<B>Create new item, owner: '%s'</B>\n",
	userName);
    webPrintWideLabelCell(label, 2);
    webPrintLinkTableNewRow();
    hPrintf("<FORM ACTION=\"%s\">\n\n", hgcName());
    cartSaveSession(cart);
    cgiMakeHiddenVar("g", "htcCreateWikiItem");
    cgiContinueHiddenVar("c");
    cgiContinueHiddenVar("o");
    cgiContinueHiddenVar("l");
    cgiContinueHiddenVar("r");
    webPrintWideCellStart(2, HG_COL_TABLE);
    puts("<B>position:&nbsp;</B>");
    savePosInTextBox(seqName, winStart+1, winEnd);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    webPrintWideCellStart(2, HG_COL_TABLE);
    char *strand = cartUsualString(cart, "newItemStrand", "plus");
    boolean plusStrand = sameWord("plus",strand) ? TRUE : FALSE;
    hPrintf("<B>strand:&nbsp;");
    cgiMakeRadioButton("newItemStrand", "plus", plusStrand);
    hPrintf("&nbsp;+&nbsp;&nbsp;");
    cgiMakeRadioButton("newItemStrand", "minus", ! plusStrand);
    hPrintf("&nbsp;-</B>");
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    webPrintWideCellStart(2, HG_COL_TABLE);
    hPrintf("<B>item name:&nbsp;</B>");
    cgiMakeTextVar("i", "defaultName", 18);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    webPrintWideCellStart(2, HG_COL_TABLE);
    hPrintf("<B>item score:&nbsp;</B>");
    cgiMakeTextVar("newItemScore", "1000", 4);
    hPrintf("&nbsp;(range:&nbsp;0&nbsp;to&nbsp;1000)");
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
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
cartHtmlEnd();
}

void doCreateWikiItem(char *itemName, char *chrom, int winStart, int winEnd)
    /* handle create item clicks for wikiTrack */
{
char *url = cfgOptionDefault("wikiTrack.URL", NULL);
int itemStart = 0;
int itemEnd = 0;
char *chrName = NULL;
char *pos = NULL;
char *strand = cartUsualString(cart, "newItemStrand", "plus");
boolean plusStrand = sameWord("plus",strand) ? TRUE : FALSE;
struct dyString *wikiItemName = newDyString(256);
struct sqlConnection *conn = hConnectCentral();
char *userName = NULL;
char *color = "000000";
int score = 0;
struct wikiTrack *newItem;

if (! wikiTrackEnabled(&userName))
    errAbort("create new wiki item: wiki track not enabled");

score = sqlSigned(cartUsualString(cart, "newItemScore", "1000"));
pos = stripCommas(cartOptionalString(cart, "getDnaPos"));
if (NULL == pos)
    errAbort("create new wiki item: called incorrectly, without getDnaPos");

hgParseChromRange(pos, &chrName, &itemStart, &itemEnd);

dyStringPrintf(wikiItemName,"BED:%s:%s:%d-%d:%s", database, chrName,
	itemStart+1, itemEnd, plusStrand ? "%2B" : "%2D");

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
newItem->color = cloneString(color);
newItem->creationDate = cloneString("0");
newItem->lastModifiedDate = cloneString("0");
newItem->descriptionKey = cloneString(wikiItemName->string);

wikiTrackSaveToDbEscaped(conn, newItem, WIKI_TRACK_TABLE, 1024);

wikiTrackFree(&newItem);

char query[256];
safef(query, ArraySize(query), "UPDATE %s set creationDate=now(),lastModifiedDate=now() WHERE name='%s'", WIKI_TRACK_TABLE, itemName);
sqlUpdate(conn,query);

safef(query, ArraySize(query), "SELECT * FROM %s WHERE name='%s' AND db='%s' limit 1",
	WIKI_TRACK_TABLE, itemName, database);
newItem = wikiTrackLoadByQuery(conn, query);
if (NULL == newItem)
    errAbort("create new wiki item: failed to load item %s", itemName);
hDisconnectCentral(&conn);

cartWebStart(cart, "%s", "User Annotation Track");
hPrintf("<H2>");
hPrintf("Created new item '%s', ", newItem->name);

hPrintf("Position: %s:", newItem->chrom);
printLongWithCommas(stdout, (long long)(newItem->chromStart+1));
hPrintf("-");
printLongWithCommas(stdout, (long long)newItem->chromEnd);
hPrintf(" %s </H2>\n", newItem->strand);
/* only the + and - need to be encoded to 2B or 2D */
hPrintf("<H3>%s</H3>\n", wikiItemName->string);

hPrintf("Please ");
hPrintf("<A HREF=\"%s/index.php?title=%s&amp;action=edit\" TARGET=_blank>add comments/description</A> to this new item", url, wikiItemName->string);
freeDyString(&wikiItemName);
cartHtmlEnd();
}
