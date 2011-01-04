/* Do clicks for the wiki track of variation for microattribution reviews */

#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "web.h"
#include "hPrint.h"
#include "obscure.h"
#include "hgConfig.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "binRange.h"
#include "web.h"
#include "net.h"
#include "grp.h"
#include "hui.h"
#include "htmlPage.h"
#include "htmshell.h"
#include "bedDetail.h"
#include "wikiLink.h"
#include "wikiTrack.h"
#include "variome.h"

void displayVariomeItem (struct variome *item, char *userName);

static void startForm(char *name, char *actionType)
{
hPrintf("<FORM ID=\"%s\" NAME=\"%s\" ACTION=\"%s\">\n\n", name, name, hgcName());
cartSaveSession(cart);
/* put actionType in place of table name */
cgiMakeHiddenVar("g", actionType);
cgiContinueHiddenVar("c");
cgiContinueHiddenVar("o");
hPrintf("\n");
cgiContinueHiddenVar("l");
cgiContinueHiddenVar("r");
hPrintf("\n");
}

void doVariome (char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle item clicks on variome - may create new items */
{
char *userName = NULL;
if (wikiTrackEnabled(database, &userName) && sameWord("0", wikiItemId))
    {
    cartWebStart(cart, database, "%s", "Variome track: Create new item");
    if (NULL == userName)
        {
        offerLogin(0, "add new items to", "variome");
        cartHtmlEnd();
        return;
        }

    if (emailVerified(TRUE)) /* prints message when not verified */
        {
        // outputJavaScript();
        startForm("createItem", "variome.create"); /* passes hidden params */

        webPrintLinkTableStart();
        /* first row is a title line */
        char label[256];
        safef(label, ArraySize(label), "Create new item, owner: '%s'\n",
            userName);
        webPrintWideLabelCell(label, 2);
        webPrintLinkTableNewRow();
        /* second row is classification pull-down menu */
        //webPrintWideCellStart(2, HG_COL_TABLE);
        //puts("<B>classification:&nbsp;</B>");
        //cgiMakeDropList(NEW_ITEM_CLASS, variomeClassList, variomeClassCnt,
                //cartUsualString(cart,NEW_ITEM_CLASS,ITEM_NOT_CLASSIFIED));
        //cgiMakeDropList("variome.loc", gvLocationDbValue, gvLocationSize,
                //cartUsualString(cart, "variome.loc",
                                //"not within known transcription unit"));
        //cgiMakeDropList("variome.coor", variomeCoorList, 2,
                //cartUsualString(cart, "variome.coor", "exact coordinates"));
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
        hPrintf("<B>item name (<A HREF=\"http://www.hgvs.org/mutnomen/\" TARGET=\"_BLANK\">HGVS mutation nomenclature</A>):&nbsp;</B>");
        cgiMakeTextVar("i", "required", 25);
        webPrintLinkCellEnd();

        /* add more fields to encourage consistancy but put together in 
           comment before adding to wiki */
        webPrintLinkTableNewRow();
        webPrintWideCellStart(2, HG_COL_TABLE);
        hPrintf("<B><A HREF=\"http://www.researcherid.com/\" TARGET=\"_BLANK\">Researcher ID</A></B> ");
        cgiMakeTextVar("variome.rid", "required", 18);
        webPrintLinkCellEnd();

        webPrintLinkTableNewRow();
        webPrintWideCellStart(2, HG_COL_TABLE);
        hPrintf("<B>Associated gene or locus</B> ");
        cgiMakeTextVar("variome.gene", "required", 18);
        webPrintLinkCellEnd();

        webPrintLinkTableNewRow();
        webPrintWideCellStart(2, HG_COL_TABLE);
        hPrintf("<B>dbSNP rs#</B> ");
        cgiMakeTextVar("variome.rs", "", 18);
        webPrintLinkCellEnd();

        webPrintLinkTableNewRow();
        webPrintWideCellStart(2, HG_COL_TABLE);
        hPrintf("<B>dbSNP ss#</B> ");
        cgiMakeTextVar("variome.ss", "", 18);
        webPrintLinkCellEnd();

        webPrintLinkTableNewRow();
        webPrintWideCellStart(2, HG_COL_TABLE);
        hPrintf("<B>Phenotype</B> ");
        cgiMakeTextVar("variome.pheno", "", 25);
        webPrintLinkCellEnd();

        webPrintLinkTableNewRow();
        webPrintWideCellStart(2, HG_COL_TABLE);
        hPrintf("<B>OMIM variant ID</B> ");
        cgiMakeTextVar("variome.omim", "", 18);
        webPrintLinkCellEnd();

        webPrintLinkTableNewRow();
        /* sixth is score which is not used */
        /* seventh row is item color pull-down menu, not used */
        /* eighth row is initial comment/description text entry */
        webPrintWideCellStart(2, HG_COL_TABLE);
        hPrintf("<B>Comments:</B><BR>");
        cgiMakeTextArea(NEW_ITEM_COMMENT, NEW_ITEM_COMMENT_DEFAULT, 5, 40);
        webPrintLinkCellEnd();
        webPrintLinkTableNewRow();
        /* ninth row is the submit and cancel buttons */
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
        }

    }
else
    {
    struct variome *item = findVariomeItemId(database, wikiItemId);
    cartWebStart(cart, database, "%s (%s)", "User Annotation Track", item->name);
    displayVariomeItem(item, userName);
    }

cartHtmlEnd();
}   /* void doVariome() */

void displayVariomeItem (struct variome *item, char *userName)
/* given an already fetched item, get the item description from
 *      the wiki.  Put up edit form(s) if userName is not NULL
 */
{
char *url = cfgOptionDefault(CFG_WIKI_URL, NULL);
//struct slName *class = slNameListFromString(item->class, ',');
if (isNotEmpty(item->geneSymbol) && differentWord(item->geneSymbol,"0"))
    {
    hPrintf("<B>Gene symbol:&nbsp;</B><A "
        "HREF=\"../cgi-bin/hgGene?hgg_gene=%s\" TARGET=_blank>%s</A><BR>\n",
            item->geneSymbol, item->geneSymbol);
    }

printPosOnChrom(item->chrom, item->chromStart, item->chromEnd,
    item->strand, FALSE, item->name);
hPrintf("<B>Created </B>%s<B> by:&nbsp;</B>", item->creationDate);
hPrintf("<A HREF=\"%s/index.php/User:%s\" TARGET=_blank>%s</A><BR>\n", url,
    item->owner, item->owner);
hPrintf("<B>Last update:&nbsp;</B>%s<BR>\n", item->lastModifiedDate);
char *editors = cfgOptionDefault(CFG_WIKI_EDITORS, NULL);
char *editor = NULL;
if ((NULL != userName) && editors)
    {
    int i;
    int wordCount = chopByChar(editors, ',', NULL, 0);
    char **words = (char **)needMem((size_t)(wordCount * sizeof(char *)));
    chopByChar(editors, ',', words, wordCount);
    for (i = 0; i < wordCount; ++i)
        {
        if (sameWord(userName, words[i]))
            {
            editor = words[i];
            break;
            }
        }
    }
if ((NULL != userName) &&
        (editor || (sameWord(userName, item->owner))))
    {
    startForm("deleteForm", "variome.delete");
    char idString[128];
    safef(idString, ArraySize(idString), "%d", item->id);
    cgiMakeHiddenVar("i", idString);
    hPrintf("\n");
    webPrintLinkTableStart();
    webPrintLinkCellStart();
    if (editor && (differentWord(userName, item->owner)))
        hPrintf("Editor '%s' has deletion rights&nbsp;&nbsp;", editor);
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

if (NULL != userName)
    hPrintf("<B>Mark this wiki article as <em>"
        "<A HREF=\"%s/index.php/%s?action=watch\" TARGET=_blank>watched</A>"
        "</em> to receive email notices of any comment additions.</B><BR>\n",
           url, item->descriptionKey);

hPrintf("<HR>\n");
displayComments((struct wikiTrack *)item); 
hPrintf("<HR>\n");

//DELETE this?
//if (NULL == userName)
    //{
    //offerLogin(item->id, "add comments to items on", "variome");
    //}
//else if (emailVerified(TRUE)) /* prints message when not verified */
    //{ 
    //startForm("addComments", "variome.addComments");
    //char idString[128];
    //safef(idString, ArraySize(idString), "%d", item->id);
    //cgiMakeHiddenVar("i", idString);
    //hPrintf("\n");
    //webPrintLinkTableStart();
    ///* first row is a title line */
    //char label[256];
    //safef(label, ArraySize(label),
        //"'%s' adding comments to item '%s'\n", userName, item->name);
    //webPrintWideLabelCell(label, 2);
    //webPrintLinkTableNewRow();
    ///* second row is initial comment/description text entry */
    //webPrintWideCellStart(2, HG_COL_TABLE);
    //cgiMakeTextArea(NEW_ITEM_COMMENT, ADD_ITEM_COMMENT_DEFAULT, 3, 40);
    //webPrintLinkCellEnd();
    //webPrintLinkTableNewRow();
    ///*webPrintLinkCellStart(); more careful explicit alignment */
    //hPrintf("<TD BGCOLOR=\"#%s\" ALIGN=\"CENTER\" VALIGN=\"TOP\">",
            //HG_COL_TABLE);
    //cgiMakeButton("submit", "add comments");
    //hPrintf("\n</FORM>\n");
    //webPrintLinkCellEnd();
    //hPrintf("<TD BGCOLOR=\"#%s\" ALIGN=\"CENTER\" VALIGN=\"TOP\">",
            //HG_COL_TABLE);
    //hPrintf("\n<FORM ID=\"cancel\" NAME=\"cancel\" ACTION=\"%s\">", hgTracksName());
    //cgiMakeButton("cancel", "return to tracks display");
    //hPrintf("\n</FORM>\n");
    //webPrintLinkCellEnd();
    //webPrintLinkTableEnd();

//Only allow edits through wiki?
    hPrintf("Editing should be done using the ");
    hPrintf("wiki article <A HREF=\"%s/index.php/%s\" TARGET=_blank>%s</A> "
       "for this item's description.<BR>", url, item->descriptionKey,
            item->descriptionKey);
    //}
}       /*      displayVariomeItem()   */

void makeCommentFromFields ()
/* works with cart variables, puts fields in form together as a html dl 
   and puts it into the NEW_ITEM_COMMENT variable for the wiki */
{
/* HGVS name is also the item name */
char *hgvs = cartCgiUsualString(cart, "i", NULL);
char *rid = cartCgiUsualString(cart, "variome.rid", NULL);
char *gene = cartCgiUsualString(cart, "variome.gene", NULL);
char *rs = cartCgiUsualString(cart, "variome.rs", NULL);
char *ss = cartCgiUsualString(cart, "variome.ss", NULL);
char *omim = cartCgiUsualString(cart, "variome.omim", NULL);
char *phen = cartCgiUsualString(cart, "variome.pheno", NULL);
char *comm = cartCgiUsualString(cart, NEW_ITEM_COMMENT, NULL);
char *pos = cartCgiUsualString(cart, "getDnaPos", NULL);
char *strand = cartCgiUsualString(cart, NEW_ITEM_STRAND, NULL);
struct dyString *dy = newDyString(512);
/* make sure defined & defaults have changed for required fields */
if (hgvs == NULL || sameString(hgvs, "required"))
    errAbort("The HGVS name is a required field\n");
if (rid == NULL || sameString(rid, "required"))
    errAbort("The Researcher ID is a required field\n");
if (gene == NULL || sameString(gene, "required"))
    errAbort("The associated gene or locus is required, it can be a HUGO gene name, LRG ID, or GenBank accession.\n");
dyStringPrintf(dy, "{{Variome_entry\n|hgvs=%s\n|rid=%s|position=%s\n", 
    hgvs, rid, pos);
dyStringPrintf(dy, "|gene=%s\n", gene); /* required */
//dyStringPrintf(dy, "<dl><dt>HGVS name<dd>%s<dt>Researcher ID<dd>%s",
    //hgvs, rid);
if (strand != NULL && differentString(strand, ""))
    {
    if (sameString(strand, "+"))
        dyStringPrintf(dy, "|strand=+\n");
    else 
        dyStringPrintf(dy, "|strand=-\n");
    }
if (rs != NULL && differentString(rs, "")) 
    dyStringPrintf(dy, "|rs=%s\n", rs);
    //dyStringPrintf(dy, "<dt>dbSNP rs#<dd>%s", rs);
if (ss != NULL && differentString(ss, ""))
    dyStringPrintf(dy, "|ss=%s\n", ss);
    //dyStringPrintf(dy, "<dt>dbSNP ss#<dd>%s", ss);
if (omim != NULL && differentString(omim, ""))
    dyStringPrintf(dy, "|omim=%s\n", omim);
    //dyStringPrintf(dy, "<dt>OMIM variant ID<dd>%s", omim);
if (phen != NULL && differentString(phen, ""))
    dyStringPrintf(dy, "|pheno=%s\n", phen);
    //dyStringPrintf(dy, "<dt>Phenotype<dd>%s", phen);
if (comm != NULL && differentString(comm, NEW_ITEM_COMMENT_DEFAULT)) 
    dyStringPrintf(dy, "|comment=%s\n", comm);
    //dyStringPrintf(dy, "<dt>Comment<dd>%s", comm);
dyStringPrintf(dy, "}}\n");
//dyStringPrintf(dy, "</dl>");
cartSetString(cart, NEW_ITEM_COMMENT, dy->string);
}

void doCreateVariomeItem (char *itemName, char *chrom, int winStart, int winEnd)
/* handle create item clicks for variome */
{
int id = 0, itemStart = 0, itemEnd = 0;
char *chrName = NULL;
char *pos = NULL;
char *strand = cartUsualString(cart, NEW_ITEM_STRAND, "plus");
char *class = cloneString("varRep"); /* section of browser belongs in */
char *color = cartUsualString(cart, NEW_ITEM_COLOR, "#000000");
char *userName = NULL;

if (! wikiTrackEnabled(database, &userName))
    errAbort("create new wiki item: wiki track not enabled");
if (NULL == userName)
    errAbort("create new wiki item: user not logged in ?");

pos = stripCommas(cartOptionalString(cart, "getDnaPos"));
if (NULL == pos)
    errAbort("create new wiki item: called incorrectly, without getDnaPos");

hgParseChromRange(database, pos, &chrName, &itemStart, &itemEnd);

id = addVariomeItem(database, chrName, itemStart, itemEnd,
    itemName, 1000, strand, userName, class, color, "Variome", "0", NULL);
/* char *db, char *chrom, int start, int end,
   char *name, int score, char *strand, char *owner, char *class,
   char *color, char *category, char *geneSymbol, char *wikiKey */

cartWebStart(cart, database, "%s %s", "Variome Track, created new item: ", itemName);

char wikiItemId[64];
safef(wikiItemId,ArraySize(wikiItemId),"%d", id);
struct variome *item = findVariomeItemId(database, wikiItemId);

makeCommentFromFields();
/* gets comment from cart, NEW_ITEM_COMMENT */
addDescription((struct wikiTrack *)item, userName, seqName, winStart, winEnd, cart, database, NULL, NULL, "[[Category:Variome]]");
displayVariomeItem(item, userName);

cartHtmlEnd();
}

//DELETE this? force to use wiki to edit?
void doAddVariomeComments(char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle add comment item clicks for Variome Track */
{
char *userName = NULL;
struct variome *item = findVariomeItemId(database, wikiItemId);

cartWebStart(cart, database, "%s (%s)", "Variome Track", item->name);
if (NULL == wikiItemId)
    errAbort("add wiki comments: NULL wikiItemId");
if (! wikiTrackEnabled(database, &userName))
    errAbort("add wiki comments: wiki track not enabled");
if (NULL == userName)
    errAbort("add wiki comments: user not logged in ?");

/* gets comment from cart, NEW_ITEM_COMMENT */
addDescription((struct wikiTrack *)item, userName, seqName, winStart, winEnd,
    cart, database, NULL, NULL, "[[Category:Variome]]");
updateVariomeLastModifiedDate(database, sqlSigned(wikiItemId));
displayVariomeItem(item, userName);
cartHtmlEnd();
}

void doDeleteVariomeItem(char *wikiItemId, char *chrom, int winStart, int winEnd)
/* handle delete item clicks for Variome Track */
{
char *userName = NULL;
struct variome *item = findVariomeItemId(database, wikiItemId);

cartWebStart(cart, database, "%s (%s)", "Variome Track, deleted item: ",
        item->name);
if (NULL == wikiItemId)
    errAbort("delete wiki item: NULL wikiItemId");
if (! wikiTrackEnabled(database, &userName))
    errAbort("delete wiki item: wiki track not enabled");
char comments[1024];
safef(comments,ArraySize(comments), "This item '''%s''' on assembly %s "
    "at %s:%d-%d has been deleted from the wiki track\n\n", item->db,
        item->name, item->chrom, item->chromStart, item->chromEnd);
prefixComments((struct wikiTrack *)item, comments, userName,
    seqName, winStart, winEnd, database, NULL, "(deleted item)", "[[Category:Variome]]");
deleteVariomeItem(database, sqlSigned(wikiItemId));
hPrintf("<BR>\n");
hPrintf("<FORM ID=\"delete\" NAME=\"delete\" ACTION=\"%s\">", hgTracksName());
cgiMakeButton("submit", "return to tracks display");
hPrintf("\n</FORM>\n");
hPrintf("<BR>\n");
cartHtmlEnd();
}

