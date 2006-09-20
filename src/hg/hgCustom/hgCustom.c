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

static char const rcsid[] = "$Id: hgCustom.c,v 1.50 2006/09/20 01:30:30 kate Exp $";

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

/* CGI variables */
#define hgCt		 "hgct_"  /* prefix for all control variables */

#define hgCtDataText      CT_CUSTOM_TEXT_ALT_VAR
#define hgCtDataFile      CT_CUSTOM_FILE_VAR
#define hgCtDataFileName  CT_CUSTOM_FILE_NAME_VAR
#define hgCtTable         CT_SELECTED_TABLE_VAR
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

void addCustomForm(char *err)
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
cgiMakeButton(hgCtDoCancel, "Cancel");
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
#ifdef OLD
char *selectedTable = cartUsualString(cart, hgCtTable, "");
if (selectedTable[0] != 0)
    {
    struct customTrack *ct;
    ctList = customTracksParseCart(cart, NULL, NULL);
    /* fill doc text box with HTML for selected table */
    for (ct = ctList; ct != NULL; ct = ct->next)
        if (sameString(ct->tdb->tableName, selectedTable) && 
                ct->tdb->html && ct->tdb->html[0])
		    docText = ct->tdb->html;
    }
#endif

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

cgiMakeButton(hgCtDoGenomeBrowser, "view in genome browser");
puts("&nbsp;");
cgiMakeButton(hgCtDoTableBrowser, "access in table browser");
puts("<BR>&nbsp;");

if (warn && warn[0])
    printf("<B>&nbsp; &nbsp; &nbsp; &nbsp; <I>Warning</I>&nbsp;<FONT COLOR='GREEN'> %s </FONT></B><BR>", warn);

puts("<TABLE><TR><TD>");

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
    //printf("<TR><TD>%s</A></TD>", ct->tdb->shortLabel);
    printf("<TR><TD><A HREF='/cgi-bin/hgCustom?%s=%s'>%s</A></TD>", 
                hgCtTable, ct->tdb->tableName, ct->tdb->shortLabel);
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
cgiTableFieldEnd(); 

cgiSimpleTableFieldStart();
cgiSimpleTableStart();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeButton(hgCtDoAdd, "add custom tracks");
cgiTableFieldEnd();
cgiTableRowEnd();

#ifdef OLD
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeButton(hgCtDoGenomeBrowser, "view in genome browser");
cgiTableFieldEnd();
cgiTableRowEnd();

cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeButton(hgCtDoTableBrowser, "access in table browser &nbsp; ");
cgiTableFieldEnd();
cgiTableRowEnd();
cgiTableEnd();
cgiTableFieldEnd();
cgiTableRowEnd();
#endif
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

void doTableBrowser()
/* Redirect to table browser */
{
char url[256];
safef(url, sizeof url, "%s?%s&hgta_group=user&hgta_doMainPage=1", 
			hgTablesName(), cartSidUrlString(cart));
puts("<HTML>");
printf("<BODY onload=\"try {self.location.href='%s' } catch(e) {}\"><a href=\"%s\">Redirect </a></BODY>", url, url);
puts("</HTML>");
}

void helpCustom()
/* display documentation */
{
webNewSection("Loading Custom Tracks");
webIncludeFile("/goldenPath/help/loadingCustomTracks.html");
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
/* linefile routines require terminal newline */
dyStringPrintf(ds, "%s\n", text);
lf = lineFileOnString("custom HTML", TRUE, dyStringCannibalize(&ds));
ds = dyStringNew(1000);
while (lineFileNextReal(lf, &line))
    {
    fprintf(stderr, "<BR>line=%s</BR>", line);
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

void addCustomDocs()
/* get HTML docs */
{
char *html = NULL;
struct hash *docHash = NULL;
char *defaultTrackName;
struct customTrack *ct;

if (cartNonemptyString(cart, hgCtDocText))
    html = cartString(cart, hgCtDocText);
else if (cartNonemptyString(cart, hgCtDocFile))
    html = cartString(cart, hgCtDocFile);
defaultTrackName = ctList->tdb->shortLabel;
docHash = getCustomTrackDocs(html, defaultTrackName);
if (docHash)
    {
    for (ct = ctList; ct != NULL; ct = ct->next)
	{
	if ((html = hashFindVal(docHash, ct->tdb->shortLabel)) != NULL)
	    ct->tdb->html = cloneString(html);
	}
    }
//cartRemove(cart, hgCtDocText);
}

void startCustomForm()
{
/* create form for adding/managing custom tracks */
puts("<FORM ACTION=\"/cgi-bin/hgCustom\" METHOD=\"POST\" "
               " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
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
addCustomForm(err);
endCustomForm();
helpCustom();
cartWebEnd(cart);
}

void doUpdateCustom(struct customTrack *ct)
/* display form for adding custom tracks.
 * Include error message, if any */
{

cartWebStart(cart, "Update Custom Track: %s", ct->tdb->shortLabel);
cartSaveSession(cart);
startCustomForm();
cartSetString(cart, hgCtDocText, ct->tdb->html);
addCustomForm(NULL);
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
endCustomForm();
cartWebEnd(cart);
}

void fixNewData(struct cart *cart)
/* append a newline to incoming data, to keep custom preprocessor happy */
{
char *customText = cartUsualString(cart, hgCtDataText, "");
if (customText[0])
    {
    struct dyString *ds = dyStringNew(0);
    dyStringPrintf(ds, "%s\n", customText);
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
ctList = customTrackAddToList(ctList, refreshCts, &replacedCts);
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

void doMiddle(struct cart *theCart)
/* create web page */
{
char *ctFileName = NULL;
struct slName *browserLines = NULL;
struct customTrack *replacedCts = NULL;
char *err = NULL, *warn = NULL;

cart = theCart;
getDbAndGenome(cart, &database, &organism);
saveDbAndGenome(cart, database, organism);
hSetDb(database);

if (cartVarExists(cart, hgCtDoGenomeBrowser))
    {
    doGenomeBrowser();
    }
else if (cartVarExists(cart, hgCtDoTableBrowser))
    {
    doTableBrowser();
    }
else if (cartVarExists(cart, hgCtDoAdd))
    {
    doAddCustom(NULL);
    }
else if (cartVarExists(cart, hgCtTable))
    {
    struct customTrack *ct = NULL;
    char *selectedTable = cartString(cart, hgCtTable);
    if (selectedTable[0] != 0)
        {
        ctList = customTracksParseCart(cart, NULL, NULL);
        /* fill doc text box with HTML for selected table */
        for (ct = ctList; ct != NULL; ct = ct->next)
            if (sameString(ct->tdb->tableName, selectedTable))
                    break;
        }
    if (ct)
        doUpdateCustom(ct);
    else
        doAddCustom(NULL);
    }
else
    {
    struct dyString *dsWarn = dyStringNew(0);

    /* get new and existing custom tracks from cart */
    fixNewData(cart);
    ctList = customTracksParseCartDetailed(cart, &browserLines, &ctFileName,
					    &replacedCts, &err);
    addWarning(dsWarn, replacedTracksMsg(replacedCts));
    doBrowserLines(browserLines, &warn);
    addWarning(dsWarn, warn);
    if (err)
	{
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
	addCustomDocs();
	/* remove doc and data vars */
	saveCustom(ctFileName);
	doManageCustom(warn);
	}
    else
	{
	cartRemove(cart, "ct");
	if (cartVarExists(cart, hgCtDoCancel))
	    doGenomeBrowser();
	else
	    doAddCustom(NULL);
	}
    }
cartRemovePrefix(cart, hgCt);
}

#ifdef OLD
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
        dyStringAppend(dsWarn, "Replaced: &nbsp;");
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
    //cartRemove(cart, hgCtDocText);
    }

/* display header and first section header */
cartWebStart(cart, firstSectionMsg);

/* create form for input */
startCustomForm();

if (ctList != NULL)
    {
    /* display list of custom tracks, with checkboxes to select for delete */
    manageCustom();
    endCustomForm();

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
    }
else
    {
    /* clean up cart.  File cleanup handled by trash cleaner */
    cartRemove(cart, "ct");
    /* display form to add custom tracks -- either URL-based or file-based */
    addCustom(err, warn);
    endCustomForm();
    helpCustom();
    }
cartWebEnd(cart);
}
#endif

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers();
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}
