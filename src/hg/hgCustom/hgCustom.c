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

static char const rcsid[] = "$Id: hgCustom.c,v 1.47.2.1 2006/08/31 19:45:25 kate Exp $";

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

#define SECTION_MANAGE_MSG      "Manage Custom Tracks"
#define SECTION_ADD_MSG         "Add Custom Tracks"

/* CGI variables */
#define hgCtDataText      CT_CUSTOM_TEXT_ALT_VAR
#define hgCtDataFile      CT_CUSTOM_FILE_VAR
#define hgCtDataFileName  CT_CUSTOM_FILE_NAME_VAR
#define hgCtTable         CT_SELECTED_TABLE_VAR
#define hgCtDocText      "hgct_docText"
#define hgCtDocFile      "hgct_docFile"
#define hgCtDocTrackName "hgct_docTrackName"
#define hgCtDoDelete     "hgct_do_delete"
#define hgCtDeletePrefix "hgct_del"
#define hgCtDoRefresh    "hgct_do_refresh"
#define hgCtRefreshPrefix "hgct_refresh"

#ifndef NO_EDIT
#define hgCtDoEdit       "hgct_do_edit"
#define hgCtName         "hgct_name"
#define hgCtDescription  "hgct_description"
#define hgCtItemUrl      "hgct_itemUrl"
#define hgCtInitialPos   "hgct_initialPos"
#define hgCtTrackLine    "hgct_trackLine"
#define hgCtColorChoice  "hgct_colorChoice"
#define hgCtDocChoice    "hgct_docChoice"
#define hgCtColorRed     "hgct_colorRed"
#define hgCtColorGreen   "hgct_colorGreen"
#define hgCtColorBlue    "hgct_colorBlue"
#define hgCtUpdateColor  "hgct_updateColor"
#define hgCtEditDoc      "hgct_do_edit_doc"
#endif

/* Global variables */
struct cart *cart;
struct hash *oldCart = NULL;
char *excludeVars[] = {"Submit", "submit", "SubmitFile", 
                        hgCtDoDelete, hgCtDoRefresh, NULL};
char *database;
char *organism;
struct customTrack *ctList = NULL;
struct slName *browserLines = NULL;
char *ctFileName;


void makeInsertButton(char *label, char *text, char *field)
/* UI button that adds to  a text field */
{
char javascript[1024];
safef(javascript, sizeof javascript, 
  "document.mainForm.%s.value = '%s' + '\\n' + document.mainForm.%s.value;", 
        field, text, field);
cgiMakeOnClickButton(javascript, label);
}

void makeClearButton(char *field)
/* UI button that clears a text field */
{
char javascript[1024];
safef(javascript, sizeof javascript, 
        "document.mainForm.%s.value = '';", field);
cgiMakeOnClickButton(javascript, "Clear");
}

void addCustom(char *err, char *warn)
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
  " <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html'>user's guide</A>."
  " Publicly available custom tracks are listed"
  " <A HREF='/goldenPath/customTracks/custTracks.html'>here</A>."
);

if (err)
    printf("<BR><B>&nbsp; &nbsp; &nbsp; &nbsp; <I>Error</I>&nbsp;<FONT COLOR='RED'> %s </FONT></B>", err);
if (warn)
    printf("<BR><B>&nbsp; &nbsp; &nbsp; &nbsp; <I>Warning</I>&nbsp;<FONT COLOR='GREEN'> %s </FONT></B>", warn);

cgiParagraph("&nbsp;");
cgiSimpleTableStart();

/* first row - label entry for file upload */
cgiSimpleTableRowStart();
cgiTableField("Paste URLs or data:");
puts("<TD ALIGN='RIGHT'>");
puts("Or upload: ");
cgiMakeFileEntry(hgCtDataFile);
cgiTableFieldEnd();
puts("<TD ALIGN='RIGHT'>");
cgiMakeSubmitButton();
cgiTableFieldEnd();
cgiTableRowEnd();

/* second row - text entry box for  data, and clear button */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=2>");
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

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
makeInsertButton("&nbsp; Set &nbsp;", 
        "track name=\\'\\' description=\\'\\' color=0,0,255", 
                        hgCtDataText);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();
cgiTableRowEnd();

/* third row - label for description text entry */
cgiSimpleTableRowStart();
cgiTableField("Optional HTML doc: ");
puts("<TD ALIGN='RIGHT'>");
puts("Or upload: ");
cgiMakeFileEntry(hgCtDocFile);
cgiTableFieldEnd();
cgiTableRowEnd();

/* fourth row - text entry for description, and clear button(s) */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=2>");

char *docText = "";
char *selectedTable = cartUsualString(cart, hgCtTable, "");
struct customTrack *ct;
if (selectedTable[0] != 0)
    /* fill doc text box with HTML for selected table */
    for (ct = ctList; ct != NULL; ct = ct->next)
        if (sameString(ct->tdb->tableName, selectedTable) && 
                ct->tdb->html && ct->tdb->html[0])
                    {
                    struct dyString *ds = dyStringNew(0);
                    dyStringPrintf(ds, "%s\'%s\'%s\n%s", CT_DOC_HEADER_PREFIX, 
                            ct->tdb->shortLabel,
                            CT_DOC_HEADER_SUFFIX, ct->tdb->html);
                    docText = dyStringCannibalize(&ds);
                    }
cgiMakeTextArea(hgCtDocText, cartUsualString(cart, hgCtDocText, docText), 
        TEXT_ENTRY_ROWS, TEXT_ENTRY_COLS);

cgiTableFieldEnd();

cgiSimpleTableFieldStart();
cgiSimpleTableStart();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
makeClearButton(hgCtDocText);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();

char buf[128];
safef(buf, sizeof buf, "%s\\'\\'%s", CT_DOC_HEADER_PREFIX, 
                                        CT_DOC_HEADER_SUFFIX);
makeInsertButton("&nbsp; Set &nbsp;", buf, hgCtDocText);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();
cgiTableRowEnd();

/* fifth row - link for HTML description template */
cgiSimpleTableRowStart();
printf("<TD><A TARGET=_BLANK HREF='../goldenPath/help/ct_description.txt'>HTML doc template</A></TD>");
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

void manageCustom()
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

hTableStart();
cgiSimpleTableRowStart();
tableHeaderField("Name", "Short track identifier");
tableHeaderField("Description", "Long track identifier");
tableHeaderField("Type", "Data format of track");
tableHeaderField("Doc", "HTML track description");
if (itemCt)
    tableHeaderField("Items", "Count of discrete items in track");
if (posCt)
    tableHeaderField("Pos"," Default track position or first item");
tableHeaderFieldStart();
cgiMakeButtonWithMsg(hgCtDoDelete, "Delete", "Remove custom track");
cgiTableFieldEnd();

/* add column wiht Update button if any custom tracks are updateable */
if (updateCt)
    {
    tableHeaderFieldStart();
    cgiMakeButtonWithMsg(hgCtDoRefresh, "Update", "Refresh from data URL");
    cgiTableFieldEnd();
    }

cgiTableRowEnd();
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    /* Name  field */
#ifndef NO_EDIT
    printf("<TR><TD><A HREF=\"/cgi-bin/hgCustom?%s=%s\">%s</A></TD>", 
            hgCtDoEdit, ct->tdb->tableName, ct->tdb->shortLabel);
#else
    printf("<TR><TD>%s</A></TD>", ct->tdb->shortLabel);
#endif
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
            printf("<TD><A HREF='%s?%s&position=%s' TITLE=%s>%s:</A></TD>", 
                hgTracksName(), cartSidUrlString(cart), pos, pos, chrom);
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
}

void doBrowserLines(struct slName *browserLines, char **retErr)
/*  parse variables from browser lines into the cart.
    Return browser initial position, if specified */
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

struct hash *getCustomTrackDocs(char *text, char *defaultTrackName)
/* get HTML descriptions from text with special comments to name 
 * and delimit tracks */
{
char *line;
struct hash *docHash = hashNew(6);
char *trackName = defaultTrackName;
struct lineFile *lf = NULL;
struct dyString *ds = dyStringNew(1000);

if (!text)
    return NULL;
lf = lineFileOnString("custom HTML", TRUE, text);
while (lineFileNextReal(lf, &line))
    {
    if (startsWithWord("<!--", line) && stringIn("-->", line) &&
            containsStringNoCase(line, "UCSC_GB_TRACK"))
        {
        /* allow double quotes in doc header comment */
        subChar(line, '"', '\'');
        line = replaceChars(line, "name=", "NAME=");
        trackName = stringBetween("NAME='", "'", line);
        if (!trackName)
            trackName = defaultTrackName;
        if (strlen(ds->string))
            {
            /* starting a new track -- save doc from previous */
            hashAdd(docHash, trackName, dyStringCannibalize(&ds));
            }
        /* remove quotes and surrounding whitespace from track identifier*/
        trackName = skipLeadingSpaces(trackName);
        eraseTrailingSpaces(trackName);
        stripChar(trackName, '\'');
        stripChar(trackName, '\"');
        ds = dyStringNew(1000);
        continue;
        }
    dyStringAppend(ds, line);
    dyStringAppend(ds, "\n");
    }
if (strlen(ds->string))
    {
    hashAdd(docHash, trackName, dyStringCannibalize(&ds));
    }
return docHash;
}

void startCustomForm()
{
/* create form for adding new custom tracks */
puts("<FORM ACTION=\"/cgi-bin/hgCustom\" METHOD=\"POST\" "
               " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
}

void endCustomForm()
/* end form for adding new custom tracks */
{
puts("</FORM>\n");
}

#ifndef NO_EDIT

void changeCustomTrack(char *track)
/* modify custom track with values from edit form */
{
struct customTrack *ct = NULL;
for (ct = ctList; ct != NULL; ct = ct->next)
    if (sameString(ct->tdb->tableName, track))
        break;
if (!ct)
    errAbort("can't find custom track: %s", track);
ct->tdb->shortLabel = cloneString(cartUsualString(cart, hgCtName,
                                        ct->tdb->shortLabel));
ct->tdb->tableName = customTrackTableFromLabel(ct->tdb->shortLabel);
ct->tdb->longLabel = cloneString(cartUsualString(cart, hgCtDescription,
                                        ct->tdb->longLabel));
//cartRemove(cart, hgCtName);
cartRemove(cart, hgCtDescription);
}

void editCustomDoc(char *track)
/* put up form for editing custom track HTML doc */
{
struct customTrack *ct = NULL;
for (ct = ctList; ct != NULL; ct = ct->next)
    if (sameString(ct->tdb->tableName, track))
        break;
if (!ct)
    errAbort("can't find custom track: %s", track);
cartWebStart(cart, "Edit HTML track documentation:  %s ", 
                        ct->tdb->shortLabel);
puts("<FORM ACTION=\"/cgi-bin/hgCustom\" METHOD=\"POST\" "
               " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hgCtTable, ct->tdb->tableName);

cgiSimpleTableStart();

/* description */
cgiSimpleTableRowStart();
cgiTableField("Description");
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeTextArea(hgCtTrackLine, "", 3, 70);
cgiTableFieldEnd();
cgiTableRowEnd();

/* methods */
cgiSimpleTableRowStart();
cgiTableField("Methods");
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeTextArea(hgCtTrackLine, "", 3, 70);
cgiTableFieldEnd();
cgiTableRowEnd();

/* references */
cgiSimpleTableRowStart();
cgiTableField("References");
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeTextArea(hgCtTrackLine, "", 3, 70);
cgiTableFieldEnd();
cgiTableRowEnd();

/* credits */
cgiSimpleTableRowStart();
cgiTableField("Credits");
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeTextArea(hgCtTrackLine, "", 3, 70);
cgiTableFieldEnd();
cgiTableRowEnd();

#ifdef NEW
cgiSimpleTableRowStart();
puts("<TD ALIGN='RIGHT'>");
cgiMakeSubmitButton();
cgiTableFieldEnd();
cgiTableRowEnd();
cgiTableEnd();
#endif

puts("</FORM>\n");
cartWebEnd();
exit(0);
}

void editCustomTrack(char *track)
/* put up form for editing custom track */
{
struct customTrack *ct = NULL;
for (ct = ctList; ct != NULL; ct = ct->next)
    if (sameString(ct->tdb->tableName, track))
        break;
if (!ct)
    errAbort("can't find custom track: %s", track);
cartWebStart(cart, "Configure custom track:  %s ", 
                        ct->tdb->shortLabel);
puts("<FORM ACTION=\"/cgi-bin/hgCustom\" METHOD=\"POST\" "
               " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hgCtTable, ct->tdb->tableName);

puts("<TABLE CELLSPACING=4>");

cgiSimpleTableRowStart();
cgiTableFieldWithMsg("Name", "Short (left) label");

cgiSimpleTableFieldStart();
cgiMakeTextVar(hgCtName, ct->tdb->shortLabel, 16);
cgiTableFieldEnd();
puts("<TD ALIGN='RIGHT'>");
cgiMakeSubmitButton();
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableFieldWithMsg("Description", "Long (center) label");
puts("<TD COLSPAN=2>");
cgiMakeTextVar(hgCtDescription, ct->tdb->longLabel, 55);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiTableFieldWithMsg("Position", "Initial position displayed in browser");
puts("<TD COLSPAN=2>");
char *initialPos = ctInitialPosition(ct);
if (!initialPos)
    initialPos = "";
cgiMakeTextVar(hgCtInitialPos, initialPos, 25);
cgiTableFieldEnd();
cgiTableRowEnd();

/* color chooser */
cgiSimpleTableRowStart();
puts("<TD COLSPAN=10>");
puts("<TABLE CELLSPACING=4>");
cgiSimpleTableRowStart();
cgiTableField("Color:");

/* fixed color */
cgiTableField("Fixed");
cgiSimpleTableFieldStart();
cgiMakeRadioButton(hgCtColorChoice, "fixed", TRUE);
cgiTableFieldEnd();
puts("<TD WIDTH=30 VSPACE=20 BGCOLOR=#000000>&nbsp;</TD>");

/* red */
puts("<TD ALIGN='RIGHT'>R</B>");
cgiSimpleTableFieldStart();
cgiMakeTextVar(hgCtColorRed, "0", 3);
cgiTableFieldEnd();

/* green */
puts("<TD ALIGN='RIGHT'>G</B>");
cgiSimpleTableFieldStart();
cgiMakeTextVar(hgCtColorGreen, "0", 3);
cgiTableFieldEnd();

/* blue */
puts("<TD ALIGN='RIGHT'>B</B>");
cgiSimpleTableFieldStart();
cgiMakeTextVar(hgCtColorBlue, "0", 3);
cgiTableFieldEnd();

/* preview button */
cgiSimpleTableFieldStart();
cgiMakeOnClickButton("", "Update");
cgiTableFieldEnd();
cgiTableRowEnd();

/* by score */
cgiSimpleTableRowStart();
cgiTableField("");
cgiTableField("By score");

/* gray */
cgiSimpleTableFieldStart();
cgiMakeRadioButton(hgCtColorChoice, "grayScored", FALSE);
cgiTableFieldEnd();
puts("<TD WIDTH=30 VSPACE=20 BGCOLOR=#909090>&nbsp;</TD>");

/* blue */
cgiSimpleTableFieldStart();
cgiMakeRadioButton(hgCtColorChoice, "blueScored", FALSE);
cgiTableFieldEnd();
puts("<TD WIDTH=30 VSPACE=20 BGCOLOR=#003C78>&nbsp;</TD>");

/* brown */
cgiSimpleTableFieldStart();
cgiMakeRadioButton(hgCtColorChoice, "brownScored", FALSE);
cgiTableFieldEnd();
puts("<TD WIDTH=30 VSPACE=20 BGCOLOR=#643200>&nbsp;</TD>");

cgiTableRowEnd();

/* by item */
cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableField("By item");
cgiSimpleTableFieldStart();
cgiMakeRadioButton(hgCtColorChoice, "colorByItem", FALSE);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
cgiTableFieldEnd();
cgiTableRowEnd();

/* HTML doc */
cgiSimpleTableRowStart();
cgiTableFieldWithMsg("Doc:", "HTML documentation for track");
cgiTableRowEnd();

cgiSimpleTableRowStart();
puts("<TD ALIGN='RIGHT'>");
cgiMakeRadioButton(hgCtDocChoice, "url", FALSE);
puts("URL");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeTextVar(hgCtItemUrl, "", 55);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
puts("<TD ALIGN='RIGHT'>");
cgiMakeRadioButton(hgCtDocChoice, "local", TRUE);
puts("Local");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeButton(hgCtEditDoc, "Edit");
puts("&nbsp; Or upload:");
cgiMakeFileEntry(hgCtDocFile);
cgiTableFieldEnd();
cgiTableRowEnd();

/* item URL */
cgiSimpleTableRowStart();
cgiTableFieldWithMsg("Item URL", "External link for item details page");
puts("<TD COLSPAN=2>");
char *itemUrl = trackDbSetting(ct->tdb, "url");
if (!itemUrl)
    itemUrl = "";
cgiMakeTextVar(hgCtItemUrl, itemUrl, 55);
cgiTableFieldEnd();
cgiTableRowEnd();

/* track line display */
cgiSimpleTableRowStart();
cgiTableField("&nbsp;");
cgiTableRowEnd();
cgiSimpleTableRowStart();
cgiTableRowEnd();
cgiTableField("Track line:");
puts("<TD ALIGN='RIGHT'>");
//char *javascript = "document.mainForm.hgCtTrackLine = 'track name = document.mainForm.hgCtName description = document.mainForm.hgCtDescription color = document.mainForm.hgCtColorRed'";
char javascript[2048];
safef(javascript, sizeof javascript, 
"document.mainForm.%s.value = 'track name=' + document.mainForm.%s.value + ' description=' + document.mainForm.%s.value + ' color=' + document.mainForm.%s.value + ',' + document.mainForm.%s.value + ',' + document.mainForm.%s.value", 
        hgCtTrackLine, hgCtName, hgCtDescription,
        hgCtColorRed, hgCtColorGreen, hgCtColorBlue);
cgiMakeOnClickButton(javascript, "Update");
cgiTableFieldEnd();
puts("<TD ALIGN='LEFT'>");
cgiMakeOnClickButton("", "Apply");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
puts("<TD COLSPAN=10>");
char trackLine[256];
safef(trackLine, sizeof trackLine, "track name=\"%s\" description=\"%s\"",
        ct->tdb->shortLabel, ct->tdb->longLabel);
cgiMakeTextArea(hgCtTrackLine, trackLine, 3, 70);
cgiTableFieldEnd();
cgiTableRowEnd();

cgiTableEnd();
puts("</FORM>\n");
cartWebEnd();
exit(0);
}

#endif

void doMiddle(struct cart *theCart)
/* create web page */
{
struct customTrack *ct;
struct customTrack *addCts = NULL, *replacedCts = NULL;
char *firstSectionMsg;
char *err = NULL, *warn = NULL;

cart = theCart;
/* needed ? */
getDbAndGenome(cart, &database, &organism);
saveDbAndGenome(cart, database, organism);
hSetDb(database);
cartSaveSession(cart);

/* get new and existing custom tracks from cart */
/* TODO: remove ifdef when hgCustom is released.  It's just
 * used to preserve old behavior during testing */
#ifndef CT_APPEND_OK
cartSetBoolean(cart, CT_APPEND_OK_VAR, TRUE);
#endif

if (cartVarExists(cart, hgCtDoDelete) || cartVarExists(cart, hgCtDoRefresh))
    {
    /* suppress parsing of new tracks */
    cartRemove(cart, hgCtDataText);
    cartRemove(cart, hgCtDataFile);
    }

/* append a newline to incoming data, to keep custom preprocessor happy */
char *customText = cartUsualString(cart, hgCtDataText, "");
if (customText[0])
    {
    struct dyString *ds = dyStringNew(0);
    dyStringPrintf(ds, "%s\n", customText);
    customText = dyStringCannibalize(&ds);
    cartSetString(cart, hgCtDataText, customText);
    }

ctList = customTracksParseCartDetailed(cart, &browserLines, &ctFileName,
                                        &replacedCts, &err);
doBrowserLines(browserLines, &warn);
if (err || warn)
    /* restore customText data to fill text area */
    cartSetString(cart, hgCtDataText, customText);

struct dyString *dsWarn = NULL;
if (warn)
    {
    dsWarn = dyStringNew(0);
    dyStringPrintf(dsWarn, "%s. ", warn);
    }

if (slCount(replacedCts) != 0)
    {
    if (!dsWarn)
        dsWarn = dyStringNew(0);
    dyStringAppend(dsWarn, "Replacing custom tracks: &nbsp;");
    for (ct = replacedCts; ct != NULL; ct = ct->next)
        {
        if (ct != replacedCts)
            /* not the first */
            dyStringAppend(dsWarn, ",&nbsp;");
        dyStringAppend(dsWarn, ct->tdb->shortLabel);
        }
    }
if (dsWarn)
    warn = dyStringCannibalize(&dsWarn);

#ifndef NO_EDIT
if (cartVarExists(cart, hgCtDoEdit))
    {
    editCustomTrack(cartString(cart, hgCtDoEdit));
    cartRemove(cart, hgCtDoEdit);
    }
else if (cartVarExists(cart, hgCtEditDoc))
    {
    editCustomDoc(cartString(cart, hgCtTable));
    cartRemove(cart, hgCtEditDoc);
    cartRemove(cart, hgCtTable);
    }
/*
else if (cartVarExists(cart, hgCtTable))
    {
    changeCustomTrack(cartString(cart, hgCtTable));
    //cartRemove(cart, hgCtTable);
    }
    */
#endif
if (cartVarExists(cart, hgCtDoDelete))
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
else if (cartVarExists(cart, hgCtDoRefresh))
    {
    /* refresh tracks  from URL */
    struct customTrack *refreshCts = NULL;
    for (ct = ctList; ct != NULL; ct = ct->next)
        {
        char var[64];
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
    ctList = customTrackAddToList(ctList, refreshCts, &replacedCts);
    if (slCount(replacedCts) != 0)
        {
        struct dyString *dsWarn = dyStringNew(0);
        dyStringAppend(dsWarn, "Replacing: &nbsp;");
        for (ct = replacedCts; ct != NULL; ct = ct->next)
            {
            if (ct != replacedCts)
                /* not the first */
                dyStringAppend(dsWarn, ",&nbsp;");
            dyStringAppend(dsWarn, ct->tdb->shortLabel);
            }
        warn = dyStringCannibalize(&dsWarn);
    }
    customTrackHandleLift(ctList);
    }
if (ctList == NULL)
    firstSectionMsg = SECTION_ADD_MSG;
else
    {
    firstSectionMsg = SECTION_MANAGE_MSG;

    /* get HTML docs */
    char *html = NULL;
    struct hash *docHash = NULL;
    char *defaultTrackName;
    if (cartNonemptyString(cart, hgCtDocText))
        {
        html = cartString(cart, hgCtDocText);
        }
    else if (cartNonemptyString(cart, hgCtDocFile))
        {
        html = cartString(cart, hgCtDocFile);
        }
    defaultTrackName = (addCts != NULL ?
                        addCts->tdb->shortLabel: ctList->tdb->shortLabel);
    docHash = getCustomTrackDocs(html, defaultTrackName);
    if (docHash)
        {
        for (ct = ctList; ct != NULL; ct = ct->next)
            {
            if ((html = hashFindVal(docHash, ct->tdb->shortLabel)) != NULL)
                {
                ct->tdb->html = cloneString(html);
                }
            }
        }
    cartRemove(cart, hgCtDocText);
    }
/* display header and first section header */
cartWebStart(cart, firstSectionMsg);

/* create form for input */
startCustomForm();

if (ctList != NULL)
    {
    /* display list of custom tracks, with checkboxes to select for delete */
    manageCustom();

    /* create custom track file in trash dir, if needed */
    if (ctFileName == NULL)
        {
        static struct tempName tn;
	customTrackTrashFile(&tn, ".bed");
        ctFileName = tn.forCgi;
        cartSetString(cart, "ct", ctFileName);
        }

    /* save custom tracks to file */
    customTrackSave(ctList, ctFileName);
    cartSetString(cart, "hgta_group", "user");

    webNewSection("Add Custom Tracks");
    }
else
    {
    /* clean up cart.  File cleanup handled by trash cleaner */
    cartRemove(cart, "ct");
    }

/* display form to add custom tracks -- either URL-based or file-based */
addCustom(err, warn);
endCustomForm();

cartRemovePrefix(cart, "hgct_");
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
