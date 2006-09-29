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
#include "customFactory.h"
#include "portable.h"
#include "errCatch.h"

static char const rcsid[] = "$Id: hgCustom.c,v 1.68 2006/09/29 00:57:32 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCustom - Custom track management CGI\n"
  "usage:\n"
  "   hgCustom <CGI settings>\n"
  );
}

#define TEXT_ENTRY_ROWS 7
#define TEXT_ENTRY_COLS 73
#define SAVED_LINE_COUNT  50

/* CGI variables */
#define hgCt		 "hgct_"  /* prefix for all control variables */

#define hgCtDataText      CT_CUSTOM_TEXT_ALT_VAR
#define hgCtDataFile      CT_CUSTOM_FILE_VAR
#define hgCtDataFileName  CT_CUSTOM_FILE_NAME_VAR
#define hgCtTable         CT_SELECTED_TABLE_VAR
#define hgCtUpdatedTable "hgct_updatedTable"
#define hgCtUpdatedTrack "hgct_updatedTrack"
#define hgCtDocText      "hgct_docText"
#define hgCtDocFile      "hgct_docFile"
#define hgCtDocTrackName "hgct_docTrackName"
#define hgCtDeletePrefix "hgct_del"
#define hgCtRefreshPrefix "hgct_refresh"

#define hgCtDo		  hgCt   "do_"	  /* prefix for all commands */
#define hgCtDoAdd	  hgCtDo "add"
#define hgCtDoCancel	  hgCtDo "cancel"
#define hgCtDoDelete	  hgCtDo "delete"
#define hgCtDoRefresh     hgCtDo "refresh"
#define hgCtDoGenomeBrowser	  hgCtDo "gb"
#define hgCtDoTableBrowser	  hgCtDo "tb"

/* Global variables */
struct cart *cart;
struct hash *oldCart = NULL;
char *excludeVars[] = {"Submit", "submit", "SubmitFile", NULL};
char *database;
char *organism;
struct customTrack *ctList = NULL;

void makeClearButton(char *field)
/* UI button that clears a text field */
{
char javascript[1024];
safef(javascript, sizeof javascript, 
        "document.mainForm.%s.value = '';", field);
cgiMakeOnClickButton(javascript, "&nbsp; Clear &nbsp;");
}

void addCustomForm(struct customTrack *ct, char *err)
/* display UI for adding custom tracks by URL or pasting data */
{
puts("Display your own data as custom annotation tracks in the browser." 
     " Data must be formatted in"
  " <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html#BED'>BED</A>,"
  " <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html#GFF'>GFF</A>,"
  " <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html#GTF'>GTF</A>,"
  " <A TARGET=_BLANK HREF='/goldenPath/help/wiggle.html'>WIG</A>"
  " or <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html#PSL'>PSL</A>"
  " formats. To configure the display, set"
  " <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html#TRACK'>track</A>"
  " and"
  " <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html#BROWSER'>browser</A>"
  " line attributes as described in the "
  " <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html'>User's Guide</A>."
  " Publicly available custom tracks are listed"
  " <A HREF='/goldenPath/customTracks/custTracks.html'>here</A>."
  " Examples are"
  " <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html#EXAMPLE1'>here</A>."
);

char *url = NULL;
    char buf[1024];

if (err)
    printf("<P><B>&nbsp; &nbsp; &nbsp; &nbsp; <I><FONT COLOR='RED'>Error</I></FONT>&nbsp; %s </B>", err);
cgiParagraph("&nbsp;");
cgiSimpleTableStart();

/* first row - label entry for file upload */
cgiSimpleTableRowStart();
if (ct)
    /* update existing */
    cgiTableField("Paste in new data:");
else
    cgiTableField("Paste URLs or data:");
puts("<TD ALIGN='RIGHT'>");
puts("Or upload: ");
cgiMakeFileEntry(hgCtDataFile);
cgiTableFieldEnd();
puts("<TD ALIGN='RIGHT'>");
cgiMakeSubmitButton();
puts("&nbsp;");

cgiMakeButton(hgCtDoCancel, "Cancel");
cgiTableFieldEnd();
cgiTableRowEnd();

/* second row - text entry box for  data, and clear button */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=2>");
if (ct && (url = ctDataUrl(ct)) != NULL)
    {
    /* can't update via pasting if loaded from URL */
    safef(buf, sizeof buf, "Replace data at URL: %s", url);
    cgiMakeTextAreaDisableable(hgCtDataText, buf,
                                TEXT_ENTRY_ROWS, TEXT_ENTRY_COLS, TRUE);
    }
else
    cgiMakeTextArea(hgCtDataText, cartUsualString(cart, hgCtDataText, ""),
                                    TEXT_ENTRY_ROWS, TEXT_ENTRY_COLS);
cgiTableFieldEnd();

cgiSimpleTableFieldStart();
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
makeClearButton(hgCtDataText);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();
cgiTableRowEnd();

/* third row - label for description text entry */
cgiSimpleTableRowStart();
cgiTableField("Optional track documentation: ");
puts("<TD ALIGN='RIGHT'>");
puts("Or upload: ");
cgiMakeFileEntry(hgCtDocFile);
cgiTableFieldEnd();
cgiTableRowEnd();

/* fourth row - text entry for description, and clear button(s) */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=2>");

if (ct && (url = ctHtmlUrl(ct)) != NULL)
    {
    safef(buf, sizeof buf, "Replace doc at URL: %s", url);
    cgiMakeTextAreaDisableable(hgCtDocText, buf,
                                    TEXT_ENTRY_ROWS, TEXT_ENTRY_COLS, TRUE);
    }
else
    cgiMakeTextArea(hgCtDocText, cartUsualString(cart, hgCtDocText, ""),
                                    TEXT_ENTRY_ROWS, TEXT_ENTRY_COLS);

cgiTableFieldEnd();

cgiSimpleTableFieldStart();
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
makeClearButton(hgCtDocText);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();
cgiTableRowEnd();

/* fifth row - link for HTML description template */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=3>");
puts("Click <A HREF=\"../goldenPath/help/ct_description.txt\" TARGET=_blank>here</A> for an HTML document template that may be used for Genome Browser track descriptions.</TD>");
cgiTableFieldEnd();
cgiTableRowEnd();
cgiTableEnd();
}

void tableHeaderFieldStart()
{
/* print table column header with white text on black background */
printf("<TD ALIGN='CENTER' BGCOLOR=#536ED3>");
}

void tableHeaderField(char *label, char *description)
{
/* print table column header with white text on black background */
puts("<TD ALIGN='CENTER' BGCOLOR=#536ED3 ");
if (description)
    printf("TITLE='%s'", description);
printf("><B>%s</B></TD> ", wrapWhiteFont(label));
}

void manageCustomForm(char *warn)
/* list custom tracks and display checkboxes so user can select for delete */
{
struct customTrack *ct;
char buf[64];
char *pos = NULL;
char *dataUrl;

/* determine which columns to display (avoid empty columns) */
int updateCt = 0, itemCt = 0, posCt = 0;
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    if (ctDataUrl(ct))
        updateCt++;
    if (ctItemCount(ct) > 0)
        itemCt++;
    if (ctInitialPosition(ct) || ctFirstItemPos(ct))
        posCt++;
    }

puts("<TABLE BORDER=0>");
cgiSimpleTableRowStart();
puts("<TD VALIGN='TOP'>");
puts("</FORM>");
printf("<FORM style=\"margin-bottom:0;\" ACTION=\"%s?%s\" METHOD=\"GET\" NAME=\"tracksForm\">\n",
           hgTracksName(), cartSidUrlString(cart));
cgiMakeButton("Submit", "view in genome browser");
puts("</FORM></TD>");
puts("<TD VALIGN='TOP'>");
printf("<FORM style=\"margin-bottom:0;\" ACTION=\"%s?%s\" METHOD=\"GET\" NAME=\"tablesForm\">\n",
           hgTablesName(), cartSidUrlString(cart));
cgiMakeButton("Submit", "access in table browser");
puts("</FORM></TD>");
cgiTableRowEnd();
puts("</TABLE>");

if (warn && warn[0])
    printf("<B>&nbsp; &nbsp; &nbsp; &nbsp; %s", warn);

printf("<FORM ACTION=\"%s?%s\" METHOD=\"GET\" NAME=\"customForm\">\n",
           hgCustomName(),  cartSidUrlString(cart));
cgiSimpleTableStart();
cgiSimpleTableRowStart();
puts("<TD VALIGN=\"TOP\">");
hTableStart();
cgiSimpleTableRowStart();
tableHeaderField("Name", "Short track identifier");
tableHeaderField("Description", "Long track identifier");
tableHeaderField("Type", "Data format of track");
tableHeaderField("Doc", "HTML track description");
if (itemCt)
    tableHeaderField("Items", "Count of discrete items in track");
if (posCt)
    tableHeaderField("Pos"," Go to genome browser at default track position or first item");
tableHeaderFieldStart();
cgiMakeButtonWithMsg(hgCtDoDelete, "delete", "Remove custom track");
cgiTableFieldEnd();

/* add column with Update button if any custom tracks are updateable */
if (updateCt)
    {
    tableHeaderFieldStart();
    cgiMakeButtonWithMsg(hgCtDoRefresh, "update", "Refresh from data URL");
    cgiTableFieldEnd();
    }

cgiTableRowEnd();
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    /* Name  field */
    if (ctDataUrl(ct) && ctHtmlUrl(ct))
        printf("<TR><TD>%s</A></TD>", ct->tdb->shortLabel);
    else
        printf("<TR><TD><A TITLE='Update custom track: %s' HREF='%s?%s=%s'>%s</A></TD>", 
            ct->tdb->shortLabel, hgCustomName(), hgCtTable, ct->tdb->tableName, 
            ct->tdb->shortLabel);
    /* Description field */
    printf("<TD>%s</TD>", ct->tdb->longLabel);
    /* Type field */
    printf("<TD>%s</TD>", ctInputType(ct));
    /* Doc field */
    printf("<TD ALIGN='CENTER'>%s</TD>", ct->tdb->html &&
                                    ct->tdb->html[0] != 0 ? "Y" : "&nbsp;");
    /* Items field */
    if (itemCt)
        {
        int count = ctItemCount(ct);
        if (count > 0)
            printf("<TD ALIGN='CENTER'>%d</TD>", count);

        else
            puts("<TD>&nbsp;</TD>");
        }
    /* Pos field; indicates initial position for the track, 
     * or first element */
    if (posCt)
        {
        pos = ctInitialPosition(ct);
        if (!pos)
            pos = ctFirstItemPos(ct);
        if (pos)
            {
            char *chrom = cloneString(pos);
            chopSuffixAt(chrom, ':');
            if (hgOfficialChromName(chrom))
                printf("<TD><A HREF='%s?%s&position=%s' TITLE=%s>%s:</A></TD>", 
                    hgTracksName(), cartSidUrlString(cart), pos, pos, chrom);
            else
                puts("<TD>&nbsp;</TD>");
            }
        else
            puts("<TD>&nbsp;</TD>");
        }
    /* Delete checkboxes */
    puts("</TD><TD ALIGN=CENTER>");
    safef(buf, sizeof(buf), "%s_%s", hgCtDeletePrefix, 
            ct->tdb->tableName);
    cgiMakeCheckBox(buf, FALSE);

    /* Update checkboxes */
    if (updateCt)
        {
        puts("</TD><TD ALIGN=CENTER>");
        safef(buf, sizeof(buf), "%s_%s", hgCtRefreshPrefix, 
                ct->tdb->tableName);
        if ((dataUrl = ctDataUrl(ct)) != NULL)
            cgiMakeCheckBoxWithMsg(buf, FALSE, dataUrl);
        else
            puts("&nbsp;");
        }
    puts("</TD></TR>");
    }
hTableEnd();

/* add button */
puts("<TD VALIGN=\"TOP\">");
cgiMakeButton(hgCtDoAdd, "add custom tracks");
puts("</TD>");

cgiTableRowEnd();
cgiTableEnd();
puts("</FORM>");
cartSetString(cart, "hgta_group", "user");
}

void doGenomeBrowser()
{
/* Redirect to table browser */
char url[256];
safef(url, sizeof url, "%s?%s", 
			hgTracksName(), cartSidUrlString(cart));
puts("<HTML>");
printf("<BODY onload=\"try {self.location.href='%s' } catch(e) {}\"><a href=\"%s\">Redirect </a></BODY>", url, url);
puts("</HTML>");
}

void helpCustom()
/* display documentation */
{
webNewSection("Loading Custom Tracks");
webIncludeFile("/goldenPath/help/customTrackLoad.html");
webEndSection();
}

void doBrowserLines(struct slName *browserLines, char **retErr)
/*  parse variables from browser lines into the cart */
{
char *err = NULL;
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
                {
	        err = "Expecting 3 words in browser position line";
                break;
                }
	    if (!hgIsChromRange(words[2])) 
                {
	        err ="Invalid browser position (use chrN:123-456 format)";
                break;
                }
            cartSetString(cart, "position", words[2]);
	    }
	}
    }
if (retErr)
    *retErr = err;
}

void addCustomDoc(char *selectedTrack)
/* get HTML doc from cgi/cart variables */
{
char *html = NULL;
struct customTrack *ct = NULL;

if (cartNonemptyString(cart, hgCtDocText))
    html = cartString(cart, hgCtDocText);
else if (cartNonemptyString(cart, hgCtDocFile))
    html = cartString(cart, hgCtDocFile);
if (selectedTrack)
    {
    for (ct = ctList; ct != NULL; ct = ct->next)
        {
        if (sameString(selectedTrack, ct->tdb->tableName))
            break;
        }
    }
else if (!ctHtmlUrl(ctList))
    {
    /* attach doc to newest added (first ct in the list)
     * unless it's got it's own doc assigned via URL */
    ct = ctList;
    }
if (ct)
    ct->tdb->html = cloneString(html);
}

void startCustomForm()
{
/* create form for adding/managing custom tracks */
printf("<FORM ACTION=\"%s\" METHOD=\"POST\" "
               " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n",
               hgCustomName());
}

void endCustomForm()
/* end form for adding new custom tracks */
{
puts("</FORM>\n");
}

void doAddCustom(char *err)
/* display form for adding custom tracks.
 * Include error message, if any */
{
cartWebStart(cart, "Add Custom Tracks");
cartSaveSession(cart);
startCustomForm();
addCustomForm(NULL, err);
endCustomForm();
helpCustom();
cartWebEnd(cart);
}

void doUpdateCustom(struct customTrack *ct, char *err)
/* display form for adding custom tracks.
 * Include error message, if any */
{
cartWebStart(cart, "Update Custom Track: %s", ct->tdb->longLabel);
cartSaveSession(cart);
startCustomForm();
cartSetString(cart, hgCtDocText, ct->tdb->html);
addCustomForm(ct, err);
cgiMakeHiddenVar(hgCtUpdatedTable, ct->tdb->tableName);
char buf[256];
safef(buf, sizeof buf, "track name='%s' description='%s'",
                        ct->tdb->shortLabel, ct->tdb->longLabel);
cgiMakeHiddenVar(hgCtUpdatedTrack, buf);
endCustomForm();
helpCustom();
cartWebEnd(cart);
}

void doManageCustom(char *warn)
/* display form for deleting & updating custom tracks.
 * Include warning message, if any */
{
cartWebStart(cart, "Manage Custom Tracks");
cartSaveSession(cart);
startCustomForm();
manageCustomForm(warn);
cartWebEnd(cart);
}

void fixNewData(struct cart *cart)
/* append a newline to incoming data, to keep custom preprocessor happy */
{
char *customText = cartUsualString(cart, hgCtDataText, "");
char *updatedTrackLine = NULL;
if (customText[0])
    {
    struct dyString *ds = dyStringNew(0);
    if ((updatedTrackLine = cartOptionalString(cart, hgCtUpdatedTrack)) != NULL)
        if (!startsWith("track", customText))
            dyStringPrintf(ds, "%s\n", updatedTrackLine);
    dyStringAppend(ds, customText);
    dyStringPrintf(ds, "\n");
    customText = dyStringCannibalize(&ds);
    cartSetString(cart, hgCtDataText, customText);
    }
}

char *replacedTracksMsg(struct customTrack *replacedCts)
/* make warning message listing replaced tracks */
{
struct customTrack *ct;

if (!slCount(replacedCts))
    return NULL;
struct dyString *dsWarn = dyStringNew(0);
dyStringAppend(dsWarn, "Replaced: &nbsp;");
for (ct = replacedCts; ct != NULL; ct = ct->next)
    {
    if (ct != replacedCts)
	/* not the first */
	dyStringAppend(dsWarn, ",&nbsp;");
    dyStringAppend(dsWarn, ct->tdb->shortLabel);
    }
return dyStringCannibalize(&dsWarn);
}

void doDeleteCustom()
/* remove custom tracks from list based on cart variables */
{
struct customTrack *ct;
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    char var[128];
    safef(var, sizeof var, "%s_%s", hgCtDeletePrefix, ct->tdb->tableName);
    if (cartBoolean(cart, var))
	slRemoveEl(&ctList, ct);
    }
}

void doRefreshCustom(char **warn)
/* reparse custom tracks from URLs based on cart variables */
{
struct customTrack *ct;
struct customTrack *replacedCts = NULL;
struct customTrack *refreshCts = NULL;

for (ct = ctList; ct != NULL; ct = ct->next)
    {
    char var[128];
    safef(var, sizeof var, "%s_%s", hgCtRefreshPrefix, ct->tdb->tableName);
    if (cartUsualBoolean(cart, var, FALSE))
	{
	struct customTrack *nextCt = NULL, *urlCt = NULL;
	struct customTrack *urlCts = 
	    customFactoryParse(ctDataUrl(ct), FALSE, NULL);
	for (urlCt = urlCts; urlCt != NULL; urlCt = nextCt)
	    {
	    nextCt = urlCt->next;
	    if (sameString(ct->tdb->tableName, urlCt->tdb->tableName))
		slAddHead(&refreshCts, urlCt);
	    }
	}
    }
ctList = customTrackAddToList(ctList, refreshCts, &replacedCts, FALSE);
if (warn)
    *warn = replacedTracksMsg(replacedCts);
customTrackHandleLift(ctList);
}

void saveCustom(char *ctFileName)
/* save custom tracks to file */
{
/* create custom track file in trash dir, if needed */
if (ctFileName == NULL)
    {
    static struct tempName tn;
    customTrackTrashFile(&tn, ".bed");
    ctFileName = tn.forCgi;
    cartSetString(cart, "ct", ctFileName);
    }
customTrackSave(ctList, ctFileName);
}

void addWarning(struct dyString *ds, char *msg)
/* build up a warning message from parts */
{
if (!msg)
    return;
if (ds->string[0])
    dyStringAppend(ds, ". ");
dyStringAppend(ds, msg);
}

char *saveLines(char *text, int max)
/* save lines from input, up to 'max'.
 * Prepend with comment, if truncated */
{
if (!text)
    return NULL;

char buf[128];
int count = 0;
char *line;
boolean truncated = FALSE;
struct dyString *ds = dyStringNew(0);

safef(buf, sizeof buf, "# Displaying first %d lines of data", max);
struct lineFile *lf = lineFileOnString("saved custom text", TRUE, text);
while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith(buf, line))
        continue;
    if (++count > max)
        {
        truncated = TRUE;
        break;
        }
    dyStringAppend(ds, line);
    dyStringAppend(ds, "\n");
    }
if (truncated)
    {
    struct dyString *dsNew = dyStringNew(0);
    dyStringPrintf(dsNew, "%s\n%s", buf, dyStringCannibalize(&ds));
    return dyStringCannibalize(&dsNew);
    }
return (dyStringCannibalize(&ds));
}

void doMiddle(struct cart *theCart)
/* create web page */
{
char *ctFileName = NULL;
struct slName *browserLines = NULL;
struct customTrack *replacedCts = NULL;
int numAdded = 0;
char *err = NULL, *warn = NULL;

cart = theCart;
getDbAndGenome(cart, &database, &organism);
saveDbAndGenome(cart, database, organism);
hSetDb(database);

if (cartVarExists(cart, hgCtDoAdd))
    {
    doAddCustom(NULL);
    }
else if (cartVarExists(cart, hgCtDoCancel))
    {
    /* remove cart variables now so text in input
     * boxes isn't parsed */
    cartRemovePrefix(cart, hgCt);
    cartRemove(cart, CT_CUSTOM_TEXT_VAR);
    ctList = customTracksParseCart(cart, NULL, NULL);
    if (ctList)
        doManageCustom(NULL);
    else
        doGenomeBrowser();
    }
else if (cartVarExists(cart, hgCtTable))
    {
    /* update track */
    struct customTrack *ct = NULL;
    /* need to clone the hgCtTable value, as the ParseCart will remove
       the variable */
    char *selectedTable = cloneString(cartString(cart, hgCtTable));
    if (selectedTable[0] != 0)
        {
        ctList = customTracksParseCart(cart, NULL, NULL);
        for (ct = ctList; ct != NULL; ct = ct->next)
            if (sameString(ct->tdb->tableName, selectedTable))
                break;
        }
    if (ct)
        doUpdateCustom(ct, NULL);
    else
        doAddCustom(NULL);
    }
else
    {
    /* get new and existing custom tracks from cart and decide what to do */
    fixNewData(cart);
    struct dyString *dsWarn = dyStringNew(0);
    char *savedCustomText = cartOptionalString(cart, hgCtDataText);
    savedCustomText = saveLines(cloneString(savedCustomText), SAVED_LINE_COUNT);

    ctList = customTracksParseCartDetailed(cart, &browserLines, &ctFileName,
					    &replacedCts, &numAdded, &err);
    addWarning(dsWarn, replacedTracksMsg(replacedCts));
    doBrowserLines(browserLines, &warn);
    addWarning(dsWarn, warn);
    if (err)
	{
        char *selectedTable = NULL;
        cartSetString(cart, hgCtDataText, savedCustomText);
        if ((selectedTable= cartOptionalString(cart, hgCtUpdatedTable)) != NULL)
            {
            struct customTrack *ct = NULL;
            for (ct = ctList; ct != NULL; ct = ct->next)
                if (sameString(selectedTable, ct->tdb->tableName))
                        break;
            doUpdateCustom(ct, err);
            }
        else
            doAddCustom(err);
       	cartRemovePrefix(cart, hgCt);
	return;
	}
    if (cartVarExists(cart, hgCtDoDelete))
	doDeleteCustom();
    if (cartVarExists(cart, hgCtDoRefresh))
	{
	doRefreshCustom(&warn);
	addWarning(dsWarn, warn);
	}
    warn = dyStringCannibalize(&dsWarn);
    if (ctList)
	{
        char *updatedTable = cartOptionalString(cart, hgCtUpdatedTable);
        if (updatedTable || numAdded)
            addCustomDoc(updatedTable);
        saveCustom(ctFileName);
        doManageCustom(warn);
	}
    else
	{
	cartRemove(cart, "ct");
        if (cartVarExists(cart, hgCtDoDelete))
                doGenomeBrowser();
        else
            doAddCustom(NULL);
	}
    }
cartRemovePrefix(cart, hgCt);
cartRemove(cart, CT_CUSTOM_TEXT_VAR);
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
