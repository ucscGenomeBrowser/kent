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
#include "binRange.h"
#include "wikiLink.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.3 2007/06/21 23:00:10 hiram Exp $";

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


static struct wikiTrack *startNewItem(char *chrom, int itemStart,
	int itemEnd, char *strand)
/* create the database item to get a new one started */
{
char descriptionKey[256];
struct sqlConnection *conn = hConnectCentral();
char *userName = NULL;
int score = 0;
struct wikiTrack *newItem;

if (! wikiTrackEnabled(&userName))
    errAbort("create new wiki item: wiki track not enabled");
if (NULL == userName)
    errAbort("create new wiki item: user not logged in ?");

safef(descriptionKey,ArraySize(descriptionKey),
	"UCSCGeneAnnotation:%s-%d", database, 0);

AllocVar(newItem);
newItem->bin = binFromRange(itemStart, itemEnd);
newItem->chrom = cloneString(chrom);
newItem->chromStart = itemStart;
newItem->chromEnd = itemEnd;
newItem->name = cloneString(curGeneId);
newItem->score = score;
safef(newItem->strand, sizeof(newItem->strand), "%s", strand);
newItem->db = cloneString(database);
newItem->owner = cloneString(userName);
#define GENE_CLASS "Genes and Gene Prediction Tracks"
newItem->class = cloneString(GENE_CLASS);
newItem->color = cloneString("#000000");
newItem->creationDate = cloneString("0");
newItem->lastModifiedDate = cloneString("0");
newItem->descriptionKey = cloneString(descriptionKey);
newItem->id = 0;
newItem->alignID = cloneString(curGeneId);

wikiTrackSaveToDbEscaped(conn, newItem, WIKI_TRACK_TABLE, 1024);

int id = sqlLastAutoId(conn);
safef(descriptionKey,ArraySize(descriptionKey),
	"UCSCGeneAnnotation:%s-%d", database, id);

wikiTrackFree(&newItem);

char query[1024];
safef(query, ArraySize(query), "UPDATE %s set creationDate=now(),lastModifiedDate=now(),descriptionKey='%s' WHERE id='%d'",
    WIKI_TRACK_TABLE, descriptionKey, id);

sqlUpdate(conn,query);
hDisconnectCentral(&conn);

char wikiItemId[64];
safef(wikiItemId,ArraySize(wikiItemId),"%d", id);
struct wikiTrack *item = findWikiItemId(wikiItemId);

hPrintf("created item: %s<BR>\n", item->name);
addDescription(item, userName, curGeneChrom,
    curGeneStart, curGeneEnd, cart, database);
return(item);
}

static void addComments(struct wikiTrack **item, char *userName)
{
if (*item)
    {
    addDescription(*item, userName, curGeneChrom,
	curGeneStart, curGeneEnd, cart, database);
    }
else
    {
    *item = startNewItem(curGeneChrom, curGeneStart, curGeneEnd,
	curGenePred->strand);
    }
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

if (cartVarExists(cart, hggDoWikiAddComment))
    addComments(&item, userName);
else
    cartRemove(cart, NEW_ITEM_COMMENT);

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
