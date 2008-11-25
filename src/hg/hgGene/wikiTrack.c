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

static char const rcsid[] = "$Id: wikiTrack.c,v 1.23 2008/11/25 22:40:50 hiram Exp $";

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

static struct bed *bedItem(char *chr, int start, int end, char *name,
    int plusCount, int negativeCount)
{
struct bed *bb;
AllocVar(bb);
bb->chrom = chr; /* do not need to clone chr string, it is already a clone */
bb->chromStart = start;
bb->chromEnd = end;
bb->name = cloneString(name);
if ((0 == negativeCount) && (plusCount > 0))
    safecpy(bb->strand, sizeof(bb->strand), "+");
else if ((0 == plusCount) && (negativeCount > 0))
    safecpy(bb->strand, sizeof(bb->strand), "-");
else
    safecpy(bb->strand, sizeof(bb->strand), " ");
return bb;
}

static char *canonicalGene(struct sqlConnection *conn, char *id, char **protein)
/* given UCSC gene id, find canonical UCSC gene id and protein if asked for */
{
char *geneName;
struct sqlResult *sr;
char **row;
char query[1024];

safef(query, ArraySize(query), "SELECT e.transcript,e.protein FROM "
	"knownCanonical e, knownIsoforms j "
	"WHERE e.clusterId = j.clusterId AND j.transcript ='%s'", id);

sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row)
    {
    geneName = cloneString(row[0]);
    if (protein)
	*protein = cloneString(row[1]);
    }
else
    geneName = NULL;

sqlFreeResult(&sr);
return geneName;
}

static struct bed *geneCluster(struct sqlConnection *conn, char *geneSymbol,
	struct bed **allIsoforms, struct bed **allProteins)
/* simple cluster of all knownGenes with name geneSymbol
 *	any items overlapping are clustered together
 * returned answer is the cluster list
 * also, if given, return full list of genes in allIsoforms
 */
{
struct sqlResult *sr;
char **row;
struct bed *bed;
struct bed *protein;
struct bed *bedList = NULL;
struct bed *proteinList = NULL;
struct bed *clustered = NULL;
char query[1024];

if (! (sqlTableExists(conn, "knownGene") && sqlTableExists(conn, "kgXref")))
    {
    if (allIsoforms)
	*allIsoforms = NULL;
    return NULL;
    }

safef(query, ArraySize(query),
	"SELECT e.chrom,e.txStart,e.txEnd,e.alignID,e.strand "
	"FROM knownGene e, kgXref j WHERE e.alignID = j.kgID AND "
	"j.geneSymbol ='%s' ORDER BY e.chrom,e.txStart", geneSymbol);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(bed);
    bed->chrom = cloneString(row[0]);
    bed->chromStart = sqlUnsigned(row[1]);
    bed->chromEnd = sqlUnsigned(row[2]);
    bed->name = cloneString(row[3]);
    safecpy(bed->strand, sizeof(bed->strand), row[4]);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
slSort(&bedList, bedCmpExtendedChr);
for (bed = bedList; bed; bed = bed->next)
    {
    char *swissProtAcc = getSwissProtAcc(conn, spConn, bed->name);
    AllocVar(protein);
    protein->chrom = cloneString(bed->chrom);
    protein->chromStart = bed->chromStart;
    protein->chromEnd = bed->chromEnd;
    protein->name = cloneString(swissProtAcc);
    slAddHead(&proteinList, protein);
    }
slSort(&proteinList, bedCmpExtendedChr);

/* now cluster that list, anything overlapping collapses into one item */
int start = BIGNUM;
int end = 0;
char *prevChr = NULL;
int strandPlus = 0;
int strandNegative = 0;
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
	    struct bed *bb = bedItem(prevChr, start, end, geneSymbol,
		strandPlus, strandNegative);
	    slAddHead(&clustered, bb);
	    start = txStart;
	    end = txEnd;
	    /* do not need to freeMem(prevChr) because it is now used
	     * in the bed item
	     */
	    prevChr = cloneString(bed->chrom);
	    strandPlus = 0;
	    strandNegative = 0;
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
    if (sameWord("+",bed->strand))
	++strandPlus;
    if (sameWord("-",bed->strand))
	++strandNegative;
    }
/* and the final item is waiting to go on the list */
struct bed *bb = bedItem(prevChr, start, end, geneSymbol,
	strandPlus, strandNegative);
slAddHead(&clustered, bb);
slSort(&clustered, bedCmpExtendedChr);
if (allIsoforms)
    *allIsoforms = bedList;
else
    bedFreeList(&bedList);
if (allProteins)
    *allProteins = proteinList;
else
    bedFreeList(&proteinList);
return clustered;
}	/*	static struct bed *geneCluster()	*/

static int addWikiTrackItem(char *db, char *chrom, int start, int end,
    char *name, int score, char *strand, char *owner, char *class,
	char *color, char *category, char *geneSymbol, char *wikiKey)
/* create new wikiTrack row with given parameters */
{
struct sqlConnection *wikiConn = wikiConnect();
struct wikiTrack *newItem;

AllocVar(newItem);
newItem->bin = binFromRange(start, end);
newItem->chrom = cloneString(chrom);
newItem->chromStart = start;
newItem->chromEnd = end;
newItem->name = cloneString(name);
newItem->score = score;
safef(newItem->strand, sizeof(newItem->strand), "%s", strand);
newItem->db = cloneString(db);
newItem->owner = cloneString(owner);
newItem->class = cloneString(class);
newItem->color = cloneString(color);
newItem->creationDate = cloneString("0");
newItem->lastModifiedDate = cloneString("0");
newItem->descriptionKey = cloneString("0");
newItem->id = 0;
newItem->geneSymbol = cloneString(geneSymbol);

wikiTrackSaveToDbEscaped(wikiConn, newItem, WIKI_TRACK_TABLE, 1024);

int id = sqlLastAutoId(wikiConn);
char descriptionKey[256];
/* when wikiKey is NULL, assign the default key of category:db-id,
 *	else, it is the proper key
 */
if (wikiKey)
    safef(descriptionKey,ArraySize(descriptionKey), "%s", wikiKey);
else
    safef(descriptionKey,ArraySize(descriptionKey),
	"%s:%s-%d", category, db, id);

wikiTrackFree(&newItem);

char query[1024];
safef(query, ArraySize(query), "UPDATE %s set creationDate=now(),lastModifiedDate=now(),descriptionKey='%s' WHERE id='%d'",
    WIKI_TRACK_TABLE, descriptionKey, id);

sqlUpdate(wikiConn,query);
wikiDisconnect(&wikiConn);
return (id);
}

static struct wikiTrack *startNewItem(struct sqlConnection *conn,
    char *chrom, int itemStart, int itemEnd, char *name, char *strand,
	struct bed *clusterList, struct bed *allIsoforms,
	    struct bed *allProteins)
/* create the database item to get a new one started */
{
char *userName = NULL;
int score = 0;
int id = 0;
char *description = descriptionString(curGeneId, conn);
char *aliases = aliasString(curGeneId, conn);
struct dyString *extraHeader = dyStringNew(0);
char *protein = NULL;
char *canonical = canonicalGene(conn, curGeneId, &protein);
char transcriptTag[1024];

safef(transcriptTag, ArraySize(transcriptTag), "%s", curGeneId);

if (canonical)
    dyStringPrintf(extraHeader,
	"Canonical gene details [http://%s/cgi-bin/hgGene?hgg_chrom=none&org=%s&db=0&hgg_gene=%s "
	    "%s %s]<BR>\n",
	    cfgOptionDefault(CFG_WIKI_BROWSER, DEFAULT_BROWSER), genome,
	    canonical, name, canonical);
if ((slCount(allIsoforms) > 1) || (!canonical))
    {
    dyStringPrintf(extraHeader, "Other loci: ");
    struct bed *el;
    for (el = allIsoforms; el; el = el->next)
	{
	if (isNotEmpty(canonical) && sameWord(canonical,el->name))
	    continue;
	dyStringPrintf(extraHeader,
	    "[http://%s/cgi-bin/hgGene?hgg_chrom=none&org=%s&db=0&hgg_gene=%s %s]",
		cfgOptionDefault(CFG_WIKI_BROWSER, DEFAULT_BROWSER),
		    genome, el->name, el->name);
	if (el->next)
	    dyStringPrintf(extraHeader,", ");
	}
    dyStringPrintf(extraHeader, "<BR>\n");
    }
dyStringPrintf(extraHeader, "%s", description);
/* add a date/time stamp to the description comments */
dyStringPrintf(extraHeader, "\n(description snapshot ~~~~~)<BR>\n");
if (aliases)
    {
    dyStringPrintf(extraHeader, "%s\n", aliases);
    freeMem(aliases);
    }

dyStringPrintf(extraHeader, "\n<HR>\n");

if (! wikiTrackEnabled(database, &userName))
    errAbort("create new wiki item: wiki track not enabled");
if (NULL == userName)
    errAbort("create new wiki item: user not logged in ?");

id = addWikiTrackItem(database, chrom, itemStart, itemEnd, name,
    score, strand, userName, GENE_CLASS, "#000000",
	"UCSCGeneAnnotation", name, NULL);

char wikiItemId[64];
safef(wikiItemId,ArraySize(wikiItemId),"%d", id);
struct wikiTrack *item = findWikiItemId(wikiItemId);

addDescription(item, userName, chrom, itemStart, itemEnd, cart, database,
	extraHeader->string, transcriptTag);
dyStringFree(&extraHeader);
return(item);
}

static void addComments(struct sqlConnection *conn, struct wikiTrack **item,
    char *userName, struct bed *clusterList, struct bed *allIsoforms,
	struct bed *allProteins)
{
if (*item)
    {
    char transcriptTag[1024];
    safef(transcriptTag, ArraySize(transcriptTag), "%s", curGeneId);
    addDescription(*item, userName, curGeneChrom,
	curGeneStart, curGeneEnd, cart, database, NULL, transcriptTag);
    }
else
    {
    struct bed *el = clusterList;
    *item = startNewItem(conn, el->chrom, el->chromStart, el->chromEnd,
	el->name, el->strand, clusterList, allIsoforms, allProteins);
    el = el->next;
    for ( ; el; el = el->next)
	{
	(void) addWikiTrackItem(database, el->chrom, el->chromStart,
	    el->chromEnd, el->name, 0, el->strand, userName, GENE_CLASS, "#000000",
	    "UCSCGeneAnnotation", el->name, (*item)->descriptionKey);
	}
    }
}

void doWikiTrack(struct sqlConnection *conn)
/* display wiki track business */
{
char *userName = NULL;
struct wikiTrack *item = findWikiItemByGeneSymbol(database, curGeneName);
char title[1024];
struct bed *allIsoforms = NULL;
struct bed *allProteins = NULL;
struct bed *clusterList = geneCluster(conn, curGeneName, &allIsoforms,
	&allProteins);
boolean editOK = FALSE;

safef(title,ArraySize(title), "UCSC gene annotations: %s", curGeneName);

/* we already know the wiki track is enabled since we are here,
 *	now calling this just to see if user is logged into the wiki
 */
if(!wikiTrackEnabled(database, &userName))
    {
    cartWebStart(cart, database, title);
    errAbort("hgGene.doWikiTrack: called when wiki track is not enabled ?");
    }
				/* FALSE == do not print message */
if (isNotEmpty(userName) && emailVerified(FALSE))
    editOK = TRUE;

if (editOK && cartVarExists(cart, hggDoWikiAddComment))
    addComments(conn, &item, userName, clusterList, allIsoforms, allProteins);
else
    cartRemove(cart, NEW_ITEM_COMMENT);

/* item exists, show wiki page */
if (item)
    {
    char *url = cfgOptionDefault(CFG_WIKI_URL, NULL);
    puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
    puts("<HTML>\n<HEAD>\n");
    hPrintf("<META HTTP-EQUIV=REFRESH CONTENT=\"0;"
	"url=%s/index.php?title=%s\">\n",
	url, item->descriptionKey);
    puts("</HEAD>\n");
    return;
    }

cartWebStart(cart, database, title);

/* safety check, both of these lists should be non-zero */
int locusLocationCount = slCount(clusterList);
int rawListCount = slCount(allIsoforms);
if ((0 == rawListCount) || (0 == locusLocationCount))
    {
    hPrintf("<EM>(Feature under development, not available for "
	"all genome browsers yet)</EM><BR>\n");
    hPrintf("hgGene.doWikiTrack: can not find any genes "
	"of gene symbol %s<BR>\n",curGeneName);
    cartWebEnd();
    return;
    }

if (isEmpty(userName))
    offerLogin();
else if (emailVerified(TRUE))  /* prints message when not verified */
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
    safef(label, ArraySize(label), "'%s' adding comments to gene %s\n",
	userName, curGeneName);
    webPrintWideLabelCell(label, 2);
    webPrintLinkTableNewRow();
    /* second row is initial comment/description text entry */
    webPrintWideCellStart(2, HG_COL_TABLE);
    cgiMakeTextArea(NEW_ITEM_COMMENT, ADD_ITEM_COMMENT_DEFAULT, 3, 70);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    hPrintf("<TD BGCOLOR=\"#%s\" COLSPAN=2 ALIGN=\"CENTER\" VALIGN=\"TOP\">",
	    HG_COL_TABLE);
    cgiMakeButton("submit", "add comments");
    hPrintf("\n</FORM>\n");
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
     hPrintf(
	"This entry form starts the comments for this gene annotation.<BR>\n"
            "Subsequent edits will be performed in the wiki editing system."
            "<BR>\n");
    }

if ((1 == locusLocationCount) && (1 == rawListCount))
    {
    hPrintf("<B>There is a single locus for gene symbol %s (%s)</B><BR>\n",
	    curGeneName, curGeneId);
    }
else
    {
    if (1 == locusLocationCount)
	hPrintf("<B>There is a single locus for gene symbol %s:</B><BR>\n",
	    curGeneName);
    else
	hPrintf("<B>There are %d loci for gene symbol %s:</B><BR>\n",
	    locusLocationCount, curGeneName);

    struct bed *el;
    for (el = clusterList; el; el = el->next)
	hPrintf("%s:%d-%d %s<BR>\n", el->chrom, el->chromStart,
	    el->chromEnd, el->strand);
    hPrintf("<B>From %d separate UCSC gene IDs:</B><BR>\n", rawListCount);
    for (el = allIsoforms; el; el = el->next)
	{
	hPrintf("%s", el->name);
	if (el->next)
	    hPrintf(", ");
	}
    hPrintf("<BR>\n");
    }

webIncludeHelpFile("wikiTrackGeneAnnotationHelp", TRUE);
webIncludeHelpFile("wikiTrack", TRUE);	/* generic description page */

cartWebEnd();

bedFreeList(&allIsoforms);
bedFreeList(&clusterList);
}
