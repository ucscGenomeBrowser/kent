/* hgCustom - Custom track management CGI. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "cart.h"
#include "cheapcgi.h"
#include "web.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "hCommon.h"
#include "customTrack.h"
#include "portable.h"
#include "errCatch.h"

static char const rcsid[] = "$Id: hgCustom.c,v 1.16 2006/06/22 05:23:27 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCustom - Custom track management CGI\n"
  "usage:\n"
  "   hgCustom <CGI settings>\n"
  );
}

/* CGI variables */
#define hgCtDataText     "hgCt_dataText"
#define hgCtDataFile     "hgCt_dataFile"
#define hgCtDataFileName  hgCtDataFile "__filename"
#define hgCtDocText      "hgCt_docText"
#define hgCtDocFile      "hgCt_docFile"
#define hgCtDoDelete     "hgCt_do_delete"
#define hgCtDeletePrefix "hgCt_del"
#define hgCtDoFileEntry  "hgCt_do_fileEntry"

/* Global variables */
struct cart *cart;
struct hash *oldCart = NULL;
char *excludeVars[] = {"Submit", "submit", "SubmitFile", 
                        hgCtDoDelete, NULL};
char *database;
char *organism;
struct customTrack *ctList = NULL;
struct hash *ctHash;
struct slName *browserLines = NULL;
char *ctFileName;

void addCustomTextEntry()
/* display UI for adding custom tracks by URL or pasting data */
{
cartRemovePrefix(cart, hgCtDataFile);
cgiParagraph("");

cgiSimpleTableStart();

/* first row - label, link for file upload, and submit button*/
cgiSimpleTableRowStart();
cgiTableField("Paste in URLs or data, or <A HREF=\"../cgi-bin/hgCustom?hgCt_do_fileEntry=1\">upload from a file\n");
cgiTableFieldEnd();
puts("<TD ALIGN='RIGHT'>");
cgiMakeSubmitButton();
cgiTableFieldEnd();
cgiTableRowEnd();

/* second row - text entry box for URL's or data  and clear button */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=2>");
cgiMakeTextArea(hgCtDataText, cartCgiUsualString(cart, hgCtDataText, ""), 8, 65);
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeClearButton("mainForm", hgCtDataText);
cgiTableFieldEnd();
cgiTableRowEnd();

/* third row - label for description text entry */
cgiSimpleTableRowStart();
cgiTableField("Optional <A TARGET=_BLANK HREF=\"/goldenPath/help/ct_description.txt\">description</A>");
cgiTableFieldEnd();
cgiTableRowEnd();

/* fourth row - text entry for description, and clear button */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=2>");
cgiMakeTextArea(hgCtDocText, cartCgiUsualString(cart, hgCtDocText, ""), 6, 65);
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeClearButton("mainForm", hgCtDocText);
cgiTableFieldEnd();
cgiTableRowEnd();


cgiTableEnd();
}

void addCustomFileEntry()
{
/* display UI for adding custom tracks by file upload 
 *    consisting of a label, entry box/browser, and button */

cartRemove(cart, hgCtDataText);

cgiParagraph("Upload file, or <A HREF=\"../cgi-bin/hgCustom?hgCt_do_fileEntry=0\">paste in URLs or data</A>");
cgiParagraph("");
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiTableField("Data file:");
cgiSimpleTableFieldStart();
cgiMakeFileEntry(hgCtDataFile);
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeButton("SubmitFile", "Submit Files");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("Optional <A TARGET=_BLANK HREF=\"/goldenPath/help/ct_description.txt\">description</A>:");
puts("<TD>");
cgiMakeFileEntry(hgCtDocFile);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
}

void showCustom()
/* list custom tracks and display checkboxes so user can select for delete */
{
//int i;
struct customTrack *ct;
char option[64];

webNewSection("Remove Custom Tracks");
hTableStart();
puts("<TR><TH ALIGN=LEFT COLSPAN=2 BGCOLOR=#536ED3>");
printf("<B>&nbsp;%s</B> ", wrapWhiteFont("Custom Tracks"));
puts("<TD BGCOLOR=#536ED3>");
cgiMakeButton(hgCtDoDelete, "Delete");
cgiTableFieldEnd();
puts("</TH></TR>");
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    printf("<TR><TD>&nbsp; %s &nbsp; </TD><TD>&nbsp; %s &nbsp; </TD><TD ALIGN=CENTER>", 
            ct->tdb->shortLabel, ct->tdb->longLabel);
    safef(option, sizeof(option), "%s_%s", hgCtDeletePrefix, 
            ct->tdb->tableName);
    cgiMakeCheckBox(option, FALSE);
    puts("</TD></TR>");
    }
hTableEnd();
webEndSection();
}

void helpCustom()
/* display documentation */
{
webNewSection("Loading Custom Tracks");
htmlIncludeWebFile("/goldenPath/help/loadingCustomTracks.html");
webEndSection();
}

void doBrowserLines(struct slName *browserLines)
/*  parse variables from browser lines into the cart */
{
struct slName *bl;
for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    char *words[96];
    int wordCount;

    wordCount = chopLine(bl->name, words);
    if (wordCount > 1)
        {
	char *command = words[1];
	if (sameString(command, "hide") 
            || sameString(command, "dense") 
            || sameString(command, "pack") 
            || sameString(command, "squish") 
            || sameString(command, "full"))
	    {
	    if (wordCount > 2)
	        {
		int i;
		for (i=2; i<wordCount; ++i)
		    {
		    char *s = words[i];
		    if (sameWord(s, "all"))
                        {
                        if (sameString(command, "hide"))
                            cartSetBoolean(cart, "hgt.hideAllNotCt", TRUE);
                        else
                            cartSetString(cart, "hgt.visAll", command);
                        }
                    else
                        cartSetString(cart, s, command);
		    }
		}
	    }
	else if (sameString(command, "position"))
	    {
	    if (wordCount < 3)
	        errAbort("Expecting 3 words in browser position line");
	    if (!hgIsChromRange(words[2])) 
	        errAbort("browser position needs to be in chrN:123-456 format");
            cartSetString(cart, "position", words[2]);
	    }
	}
    }
}

static struct customTrack *parseTracks(char *var)
/* get tracks from CGI/cart variable and add to custom track list */
{
struct customTrack *addCts = NULL;
struct customTrack *ct, *oldCt;
struct errCatch *errCatch = errCatchNew();

if (errCatchStart(errCatch))
    {
    addCts = customTracksParse(cartString(cart, var), FALSE, &browserLines);
    doBrowserLines(browserLines);
    for (ct = addCts; ct != NULL; ct = ct->next)
        {
        if ((oldCt = hashFindVal(ctHash, ct->tdb->tableName)) != NULL)
            {
            printf("<BR>&nbsp; &nbsp; <FONT COLOR='GREEN'>Replacing track: %s <BR>", ct->tdb->tableName);
            slRemoveEl(&ctList, oldCt);
            }
        slAddTail(&ctList, ct);
        }
    cartRemovePrefix(cart, var);
    }
else {}
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf("<BR><FONT COLOR='RED'>%s</FONT>", errCatch->message->string);
errCatchFree(&errCatch);
return addCts;
}

#define DOC_TRACK_PREFIX  "<!-- UCSC_GB_TRACK "
#define DOC_TRACK_SUFFIX  "-->"
#define DOC_NAME_TAG    "NAME="

static struct hash *getCustomTrackDocs(char *text)
/* get HTML descriptions from text with special comments to name 
 * and delimit tracks */
{
struct hash *docHash = NULL;

if (!text)
    return NULL;
docHash = hashNew(6);
char *p, *q;
char *html = text;
char *trackName = NULL;
while ((p = stringIn(DOC_TRACK_PREFIX, html)) != NULL)
    {
    if (trackName)
        {
        *--p = 0;
        hashAdd(docHash, trackName, html);
        p++;
        }
    q = stringIn(DOC_TRACK_SUFFIX, p);
    if (!q)
        errAbort("Badly formatted HTML file: missing TRACK comment end");
    p = stringIn(DOC_NAME_TAG, p);
    if (!p)
        errAbort("Badly formatted HTML file: missing TRACK comment NAME tag");
    trackName = p + strlen(DOC_NAME_TAG);
    for (p = trackName; p != q && !isspace(*p); p++);
    *p = 0;
    uglyf("<BR>trackName=%s\n", trackName);
    html = q + strlen(DOC_TRACK_SUFFIX);
    }
if (trackName)
    {
    uglyf("<BR>adding to hash: %s (%d)", trackName, (int)strlen(html));
    hashAdd(docHash, trackName, html);
    }
return docHash;
}

void startCustomForm()
/* create form for adding new custom tracks */
{
puts("Display your own custom annotation tracks in the browser"
     " using the procedure described in the custom tracks"
"<A TARGET=_BLANK HREF=\"/goldenPath/help/customTrack.html\"> user's guide </A>."
     " For information on upload procedures and supported formats, see "
     "the \"Loading Custom Annotation Tracks\" section, below.");
/* create HMTL form */
puts("<FORM ACTION=\"/cgi-bin/hgCustom\" METHOD=\"POST\" "
               " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
}

void endCustomForm()
/* end form for adding new custom tracks */
{
puts("</FORM>\n");
}

void doMiddle(struct cart *theCart)
/* create web page */
{
struct tempName tn;
struct customTrack *ct;
struct customTrack *addCts = NULL;

cart = theCart;
/* needed ? */
getDbAndGenome(cart, &database, &organism);
saveDbAndGenome(cart, database, organism);
hSetDb(database);

/* display header and panel to add custom tracks */
cartWebStart(cart, "Add Custom Track");
startCustomForm();
cartSaveSession(cart);

/* get existing custom tracks from cart */
ctList = customTracksParseCart(cart, &browserLines, &ctFileName);
doBrowserLines(browserLines);
ctHash = hashNew(5);
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    hashAdd(ctHash, ct->tdb->tableName, ct);
    }

/* display form to add custom tracks -- either URL-based or file-based */
if (cartVarExists(cart, hgCtDoFileEntry) && cartBoolean(cart, hgCtDoFileEntry))
    addCustomFileEntry();
else
    addCustomTextEntry();

/* process submit buttons */
if (cartVarExists(cart,hgCtDataFile))
    {
    /* add from file */
    if (cartNonemptyString(cart, hgCtDataFile))
        addCts = parseTracks(hgCtDataFile);
    else
        {
        char *file = cartString(cart, hgCtDataFileName);
        if (sameString(file, ""))
            printf("<BR><FONT COLOR='RED'>No file specified</FONT>");
        else
            printf("<BR><FONT COLOR='RED'>Error reading file:  %s</FONT>", 
                        cartString(cart, hgCtDataFileName));
        }
    }
else if (cartVarExists(cart, hgCtDoDelete))
    {
    /* delete tracks */
    for (ct = ctList; ct != NULL; ct = ct->next)
        {
        char var[64];
        safef(var, sizeof var, "%s_%s", hgCtDeletePrefix, ct->tdb->tableName);
        if (cartBoolean(cart, var))
            slRemoveEl(&ctList, ct);
        }
    }
else if (cartNonemptyString(cart, hgCtDataText))
    {
    addCts = parseTracks(hgCtDataText);
    }

if (ctList != NULL)
    {
    /* get HTML docs */
    char *html = NULL;
    struct hash *docHash;
    if (cartNonemptyString(cart, hgCtDocText))
        {
        html = cartString(cart, hgCtDocText);
        }
    else if (cartNonemptyString(cart, hgCtDocFile))
        {
        html = cartString(cart, hgCtDocFile);
        }
    docHash = getCustomTrackDocs(html);
    if (docHash)
        {
        for (ct = ctList; ct != NULL; ct = ct->next)
            {
            if ((html = hashFindVal(docHash, ct->tdb->shortLabel)) != NULL)
                {
                uglyf("<BR>found html for %s (%d)", ct->tdb->shortLabel,
                                        (int)strlen(html));
                ct->tdb->html = cloneString(html);
                }
            }
        }
    }

#ifdef MAYBE
/* links to navigate to genome or table browser */
cgiParagraph(" ");
puts("&nbsp; <A HREF=/cgi-bin/hgTracks>Genome Browser</A>");
puts("&nbsp; &nbsp; <A HREF=/cgi-bin/hgTables?hgta_group=user>Table Browser</A>");
#endif

if (ctList != NULL)
    {
    /* display list of custom tracks, with checkboxes to select for delete */
    showCustom();

    /* create custom track file in trash dir, if needed */
    if (ctFileName == NULL)
        {
	customTrackTrashFile(&tn, ".bed");
        ctFileName = tn.forCgi;
        cartSetString(cart, "ct", ctFileName);
        }

    /* save custom tracks to file */
    customTrackSave(ctList, ctFileName);
    cartSetString(cart, "hgta_group", "user");

    //cartRemovePrefix(cart, "hgCt_");
    }
else
    {
    /* clean up cart.  File cleanup handled by trash cleaner */
    cartRemove(cart, "ct");
    }
endCustomForm();
helpCustom();
cartWebEnd(cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers();
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}
