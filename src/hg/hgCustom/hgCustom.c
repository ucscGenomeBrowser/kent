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

static char const rcsid[] = "$Id: hgCustom.c,v 1.13 2006/06/19 21:34:08 hiram Exp $";

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
#define hgCtDataFile     "hgCt_datafile"
#define hgCtDataFileName  hgCtDataFile "__filename"
#define hgCtDocText      "hgCt_docText"
#define hgCtDocFile      "hgCt_docFile"
#define hgCtDoDelete     "hgCt_do_Delete"
#define hgCtDeletePrefix "hgCt_del"

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

void addCustom()
/* display UI for adding and deleting custom tracks */
{
puts("Display your own custom annotation tracks in the browser"
     " using the procedure described in the custom tracks"
"<A TARGET=_BLANK HREF=\"../goldenPath/help/customTrack.html\"> user's guide </A>."
     " For information on upload procedures and supported formats, see "
     "the \"Loading Custom Annotation Tracks\" section, below.");

/* create HMTL form */
puts("<FORM ACTION=\"/cgi-bin/hgCustom\" METHOD=\"POST\" "
               " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

/* add your own: 
 *  text box and two buttons (submit, reset), 
 *  formatted in a table */
cgiSimpleTableStart();
cgiSimpleTableRowStart();
cgiTableField("Paste in URLs or data:\n");
puts("<TD ALIGN='RIGHT'>");
cgiMakeClearButton("mainForm", hgCtDataText);
cgiTableFieldEnd();

cgiTableField("&nbsp; Optional <A TARGET=_BLANK HREF=\"/goldenPath/help/ct_description.txt\">descriptions</A>:");

puts("<TD ALIGN='RIGHT'>");
cgiMakeClearButton("mainForm", hgCtDocText);
cgiTableFieldEnd();
cgiTableRowEnd();
cgiSimpleTableFieldStart();
cgiTableFieldEnd();

/* second row - text entry boxes for URL's or data  and submit button */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=2>");
cgiMakeTextArea(hgCtDataText, cartCgiUsualString(cart, hgCtDataText, ""), 8, 55);
cgiTableFieldEnd();
puts("<TD COLSPAN=2>");
cgiMakeTextArea(hgCtDocText, cartCgiUsualString(cart, hgCtDocText, ""), 8, 25);
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeSubmitButton();
cgiTableFieldEnd();
cgiTableRowEnd();

/* file upload controls, 
 *    consisting of a label, entry box/browser, and button */
cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiTableField("&nbsp; Or upload from file:");
cgiSimpleTableFieldStart();
cgiMakeFileEntry(hgCtDataFile);
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeButton("SubmitFile", "Submit Files");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableField("&nbsp; Optional <A TARGET=_BLANK HREF=\"/goldenPath/help/ct_description.txt\">description file</A>:");
puts("<TD>");
cgiMakeFileEntry(hgCtDocFile);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
}

void showCustom()
/* list custom tracks and display checkboxes so user can select for delete */
{
int i;
struct customTrack *ct;
char option[64];

webNewSection("Remove Custom Tracks");
hTableStart();
puts("<TR><TH align=LEFT colspan=3 BGCOLOR=#536ED3>");
printf("<B>&nbsp;%s</B> ", wrapWhiteFont("Custom Tracks"));
for (i = 0; i < 50; i++)
    puts("&nbsp;");
cgiMakeButton(hgCtDoDelete, "Delete");
puts("</TH></TR>");
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    printf("<TR><TD> %s </TD><TD> %s </TD><TD>", 
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

#ifdef NEW
#define DOC_TRACK_PREFIX  "<!-- UCSC_GB_TRACK "
#define DOC_TRACK_SUFFIX  "-->"

static struct hash *getCustomTrackDocs(char *text)
/* get HTML descriptions from text with special comments to name 
 * and delimit tracks */
{
struct hash *docHash = hashNew(6);
char *p, *q;
char *html = text;
char *trackName = NULL;
while ((p = stringIn(DOC_TRACK_PREFIX, html)) != NULL)
    {
    if (trackName)
        {
        *p = 0;
        hashAdd(docHash, cloneString(trackName), cloneString(html));
        }
    q = stringIn(DOC_TRACK_SUFFIX, p);
    p += strlen(DOC_TRACK_PREFIX);
    if (q)
        {
        html = q + strlen(DOC_TRACK_SUFFIX);
        *html++ = 0;
        trackName = skipLeadingSpaces(p);
        }
    else
        html = p;
    }
return docHash;
}

#endif

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
addCustom();

/* get existing custom tracks from cart */
ctList = customTracksParseCart(cart, &browserLines, &ctFileName);
doBrowserLines(browserLines);
//uglyf("<BR><FONT COLOR='GRAY'>Starting with %d cts<BR>", slCount(ctList));
ctHash = hashNew(5);
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    //uglyf("<BR>Existing ct: %s<BR>", ct->tdb->tableName);
    hashAdd(ctHash, ct->tdb->tableName, ct);
    }

/* process submit buttons */
if (cartVarExists(cart,"SubmitFile"))
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
    //uglyf("<BR><FONT COLOR='GRAY'>delete tracks<BR>");
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

#ifdef NEW
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
    for (ct = ctList; ct != NULL; ct = ct->next)
        {
        if ((html = hashFindVal(docHash, ct->tdb->tableName)) != NULL)
            ct->tdb->html = cloneString(html);
        }
    }

/* links to navigate to genome or table browser */
cgiParagraph(" ");
puts("&nbsp; <A HREF=/cgi-bin/hgTracks>Genome Browser</A>");
puts("&nbsp; &nbsp; <A HREF=/cgi-bin/hgTables?hgta_group=user>Table Browser</A>");
#endif

if (ctList != NULL)
    {
    /* display list of custom tracks, with checkboxes to select for delete */
    showCustom();

    //uglyf("<BR><FONT COLOR='GRAY'>saving %d cts<BR>\n", slCount(ctList));

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
puts("</FORM>\n");
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
