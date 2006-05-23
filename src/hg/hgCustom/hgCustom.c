/* hgCustom - Custom track management CGI. */
#include "common.h"
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

static char const rcsid[] = "$Id: hgCustom.c,v 1.7 2006/05/23 00:11:43 kate Exp $";

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
#define hgCtAddText     "hgCt_addText"
#define hgCtAddFile     "hgCt_addFile"
#define hgCtAddFileName  hgCtAddFile "__filename"
#define hgCtHtmlFile    "hgCt_htmlFile"
#define hgCtDoDelete    "hgCt_do_Delete"
#define hgCtDeletePrefix "hgCt_del"

/* Global variables */
struct cart *cart;
struct hash *oldCart = NULL;
char *excludeVars[] = {"Submit", "submit", "SubmitFile", hgCtDoDelete, NULL};
char *database;
char *organism;
struct customTrack *ctList = NULL;
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
cgiParagraph("&nbsp;Paste in URL or data:\n");
cgiSimpleTableStart();
cgiSimpleTableRowStart();

cgiSimpleTableFieldStart();
cgiMakeTextArea(hgCtAddText, cartCgiUsualString(cart, hgCtAddText, ""), 8, 60);
cgiTableFieldEnd();

/* right element of table is a nested table
       * with two buttons stacked on top of each other */
cgiSimpleTableFieldStart();
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeSubmitButton();
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeClearButton("mainForm", hgCtAddText);
//cgiMakeResetButton();
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();

cgiTableRowEnd();
cgiTableEnd();

/* file upload controls, formatted into a table */
    
cgiSimpleTableStart();

/* first  row -- file upload controls,
 *    consisting of a label, entry box/browser, and button */
cgiSimpleTableRowStart();

cgiTableField("&nbsp;Or upload data from a file:");

cgiSimpleTableFieldStart();
cgiMakeFileEntry(hgCtAddFile, cartCgiUsualString(cart, hgCtAddFileName, ""));
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
cgiMakeButton("SubmitFile", "Submit File");
cgiTableFieldEnd();

cgiTableRowEnd();

/* row for HTML file upload */
cgiSimpleTableRowStart();

cgiTableField("&nbsp;Optional description file (HTML):");

cgiSimpleTableFieldStart();
cgiMakeFileEntry(hgCtHtmlFile, "");
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
//puts("<OBJECT SCROLLING=NO DATA=\"/goldenPath/help/loadingCustomTracks.html\">Instructions for loading custom tracks are <A HREF=\"/goldenPath/help/loadingCustomTracks.html\">here</A>.</OBJECT>");
//puts("<IFRAME WIDTH=100% HEIGHT=350 SCROLLING=NO FRAMEBORDER=0 SRC=\"/goldenPath/help/loadingCustomTracks.html\">Instructions for loading custom tracks are <A HREF=\"/goldenPath/help/loadingCustomTracks.html\">here</A>.</IFRAME>");
htmlIncludeWebFile("/goldenPath/help/loadingCustomTracks.html");
webEndSection();
}

void doMiddle(struct cart *theCart)
/* create web page */
{
struct tempName tn;
struct customTrack *addCts = NULL;
struct customTrack *ct;
struct errCatch *errCatch;

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
//uglyf("<BR><FONT COLOR='GRAY'>Starting with %d cts<BR>", slCount(ctList));

/* process submit buttons */
if (cartVarExists(cart,"SubmitFile"))
    {
    /* add from file */
    //uglyf("<BR><FONT COLOR=GRAY>file input</FONT><BR>");
    errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        addCts = customTracksParse(cartString(cart, hgCtAddFile), 
                TRUE, &browserLines);
        slAddTail(&ctList, addCts);
        cartRemovePrefix(cart, hgCtAddFile);
        }
    else {}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        warn("<FONT COLOR='RED'>%s</FONT>", errCatch->message->string);
    errCatchFree(&errCatch);
    }
else if (cartVarExists(cart, hgCtDoDelete))
    {
    //uglyf("<BR><FONT COLOR='GRAY'>delete tracks<BR>");
    /* delete tracks */
    for (ct = ctList; ct != NULL; ct = ct->next)
        {
        char var[64];
        safef(var, sizeof var, "%s_%s", hgCtDeletePrefix, ct->tdb->tableName);
        if (cartVarExists(cart, var))
            slRemoveEl(&ctList, ct);
        }
    }
else if (cartNonemptyString(cart, hgCtAddText))
    {
    /* add from textbox */
    //uglyf("<BR><FONT COLOR='GREY'>textarea input<BR>");
    //uglyf("<BR><FONT COLOR='GREY'>input=%s<BR></FONT>", cartString(cart, hgCtAddText));
    errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        addCts = customTracksParse(cartString(cart, hgCtAddText), 
                FALSE, &browserLines);
        //uglyf("<BR>adding %d tracks<BR>", slCount(addCts));
        slAddTail(&ctList, addCts);
        cartRemove(cart, hgCtAddText);
        }
    else {}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        warn("<FONT COLOR='RED'>%s", errCatch->message->string);
    errCatchFree(&errCatch);
    }

if (ctList != NULL)
    {
    /* display list of custom tracks, with checkboxes to select for delete */
    showCustom();

    //uglyf("<BR><FONT COLOR='GRAY'>saving %d cts<BR>\n", slCount(ctList));

    /* create custom track file in trash dir, if needed */
    if (ctFileName == NULL)
        {
        makeTempName(&tn, "ct", ".bed");
        ctFileName = tn.forCgi;
        cartSetString(cart, "ct", ctFileName);
        }

    /* save custom tracks to file */
    customTrackSave(ctList, ctFileName);
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
