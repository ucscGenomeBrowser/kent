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

static char const rcsid[] = "$Id: wikiTrack.c,v 1.7 2007/06/25 23:07:28 hiram Exp $";

static char *hgGeneUrl()
{
static char retBuf[1024];
safef(retBuf, ArraySize(retBuf), "cgi-bin/hgGene?%s=1&%s",
	hggDoWikiTrack, cartSidUrlString(cart));
return retBuf;
}

static char *wikiTrackUserLoginUrl()
/* Return the URL for the wiki user login page. */
{
char *retEnc = encodedReturnUrl(hgGeneUrl);
char buf[2048];

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
printf("<A HREF=\"%s\"><B>click here to login.</B></A><BR>\n", loginUrl);
printf("The wiki also serves as a forum for users "
       "to share knowledge and ideas.\n</P>\n");
freeMem(loginUrl);
}

static struct bed *bedItem(char *chr, int start, int end, char *name)
{
struct bed *bb;
AllocVar(bb);
bb->chrom = chr; /* do not need to clone chr string, it is already a clone */
bb->chromStart = start;
bb->chromEnd = end;
bb->name = cloneString(name);
return bb;
}

static struct bed *geneCluster(struct sqlConnection *conn, char *geneSymbol,
	struct bed **returnBed)
/* simple cluster of all knownGenes with name geneSymbol
 *	any items overlapping are clustered together
 */
{
struct sqlResult *sr;
char **row;
struct bed *bed;
struct bed *bedList = NULL;
struct bed *clustered = NULL;
char query[1024];

safef(query, ArraySize(query), "SELECT e.chrom,e.txStart,e.txEnd,e.alignID FROM knownGene e, kgXref j WHERE e.alignID = j.kgID AND j.geneSymbol ='%s' ORDER BY e.chrom,e.txStart", geneSymbol);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(bed);
    bed->chrom = cloneString(row[0]);
    bed->chromStart = sqlUnsigned(row[1]);
    bed->chromEnd = sqlUnsigned(row[2]);
    bed->name = cloneString(row[3]);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
slSort(&bedList, bedCmpExtendedChr);

hPrintf("raw bed list count: %d<BR>\n", slCount(bedList));

/* now cluster that list */
int start = BIGNUM;
int end = 0;
char *prevChr = NULL;
for (bed = bedList; bed; bed = bed->next)
    {
    int txStart = bed->chromStart;
    int txEnd = bed->chromEnd;
    if (prevChr)
	{
	boolean notOverlap = TRUE;
	if ((txEnd > start) && (txStart < end))
	    notOverlap = FALSE;
	if (notOverlap || differentWord(prevChr,bed->chrom))
	    {
	    struct bed *bb = bedItem(prevChr, start, end, geneSymbol);
	    slAddHead(&clustered, bb);
	    start = txStart;
	    end = txEnd;
	    prevChr = cloneString(bed->chrom);
	    }
	else
	    {
	    if (start > txStart)
		start = txStart;
	    if (end < txEnd)
		end = txEnd;
	    }
	if (differentWord(prevChr,bed->chrom))
	    {
		freeMem(prevChr);
		prevChr = cloneString(bed->chrom);
	    }
	}
    else
	{
	start = txStart;
	end = txEnd;
	prevChr = cloneString(bed->chrom);
	}
    }
struct bed *bb = bedItem(prevChr, start, end, geneSymbol);
slAddHead(&clustered, bb);
slSort(&clustered, bedCmpExtendedChr);
if (returnBed)
    *returnBed = bedList;
else
    bedFreeList(&bedList);
hPrintf("custered bed list count: %d<BR>\n", slCount(clustered));
return clustered;
}	/*	static struct bed *geneCluster()	*/

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
newItem->name = cloneString(curGeneName);
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
newItem->geneSymbol = cloneString(curGeneId);

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
struct wikiTrack *item = findWikiItemByGeneSymbol(database, curGeneId);
char title[1024];
struct bed *bedList = NULL;
struct bed *clusterList = geneCluster(conn, curGeneName, &bedList);

safef(title,ArraySize(title), "UCSC gene annotations %s", curGeneName);
cartWebStart(cart, title);

/* safety check */
int locusLocationCount = slCount(clusterList);
int rawListCount = slCount(bedList);
if ((0 == rawListCount) || (0 == locusLocationCount))
    errAbort("hgGene.doWikiTrack: can not find any genes called %s",
	curGeneName);

/* we already know the wiki track is enabled since we are here,
 *	now calling this just to see if user is logged into the wiki
 */
if(!wikiTrackEnabled(&userName))
    errAbort("hgGene.doWikiTrack: called when wiki track is not enabled");

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
else if (emailVerified())  /* prints message when not verified */
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
	"'%s' adding comments to gene %s (%s)\n",
	    userName, curGeneName, curGeneId);
    webPrintWideLabelCell(label, 2);
    webPrintLinkTableNewRow();
    /* second row is initial comment/description text entry */
    webPrintWideCellStart(2, HG_COL_TABLE);
    hPrintf("<B>add comments:</B><BR>");
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
    }

createPageHelp("wikiTrackAddCommentHelp");

hPrintf("<HR>\n");

hPrintf("<B>There are %d locus locations for gene %s:</B><BR>\n",
    locusLocationCount, curGeneName);
struct bed *el;
for (el = clusterList; el; el = el->next)
    hPrintf("%s:%d-%d<BR>\n", el->chrom, el->chromStart, el->chromEnd);
hPrintf("<B>From %d separate UCSC gene IDs:</B><BR>\n", rawListCount);
for (el = bedList; el; el = el->next)
    {
    hPrintf("%s", el->name);
    if (el->next)
	hPrintf(", ");
    }
hPrintf("<BR>\n");

cartWebEnd();

bedFreeList(&bedList);
bedFreeList(&clusterList);
}
