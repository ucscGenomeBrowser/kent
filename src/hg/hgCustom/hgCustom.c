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

static char const rcsid[] = "$Id: hgCustom.c,v 1.36 2006/08/23 22:55:54 kate Exp $";

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
#define hgCtDocText      "hgct_docText"
#define hgCtDocFile      "hgct_docFile"
#define hgCtDocTrackName "hgct_docTrackName"
#define hgCtDoDelete     "hgct_do_delete"
#define hgCtDeletePrefix "hgct_del"
#define hgCtDoRefresh    "hgct_do_refresh"
#define hgCtRefreshPrefix "hgct_refresh"

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
  " <A TARGET=_BLANK HREF='/goldenPath/help/wiggle.html'>WIG</A>,"
  " or <A TARGET=_BLANK HREF='/goldenPath/help/customTrack.html#PSL'>PSL</A>,"
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
cgiMakeTextArea(hgCtDocText, cartCgiUsualString(cart, hgCtDocText, ""), 
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
makeInsertButton("&nbsp; Set &nbsp;", 
        "<!-- UCSC_GB_TRACK NAME=\\'\\' -->", hgCtDocText);
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
    if (ct->bedList)
        itemCt++;
    if (ctInitialPosition(ct))
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
    printf("<TR><TD>%s</A></TD>", ct->tdb->shortLabel);
    /* Description field */
    printf("<TD>%s</TD>", ct->tdb->longLabel);
    /* Type field */
    printf("<TD>%s</TD>", ctInputType(ct));
    /* Doc field */
    printf("<TD ALIGN='CENTER'>%s</TD>", ct->tdb->html &&
                                    ct->tdb->html[0] != 0 ? "X" : "&nbsp;");
    /* Items field */
    if (itemCt)
        {
        if (ct->bedList)
            {
            printf("<TD ALIGN='CENTER'>%d</TD>", slCount(ct->bedList));
            }
        else
            puts("<TD>&nbsp;</TD>");
        }
    /* Pos field; indicates initial position for the track, 
     * or first element */
    if (posCt)
        {
        pos = ctInitialPosition(ct);
        if (!pos)
            {
            if (ct->bedList)
                {
                safef(buf, sizeof(buf), "%s:%d-%d", ct->bedList->chrom,
                        ct->bedList->chromStart, ct->bedList->chromEnd);
                pos = buf;
                }
            }
        if (pos)
            {
            char *chrom = cloneString(pos);
            chopSuffixAt(chrom, ':');
            printf("<TD><A HREF='%s?%s&position=%s'>%s:</A></TD>", 
                    hgTracksName(), cartSidUrlString(cart), pos, chrom);
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

void helpCustom()
/* display documentation */
{
webNewSection("Loading Custom Tracks");
webIncludeFile("/goldenPath/help/loadingCustomTracks.html");
webEndSection();
}

void doBrowserLines(struct slName *browserLines)
/*  parse variables from browser lines into the cart.
    Return browser initial position, if specified */
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

struct hash *getCustomTrackDocs(char *text, char *defaultTrackName)
/* get HTML descriptions from text with special comments to name 
 * and delimit tracks */
{
char *line;
char buf[64];
struct hash *docHash = hashNew(6);
char *trackName = defaultTrackName;
struct lineFile *lf = NULL;
struct dyString *ds = dyStringNew(1000);

if (!text)
    return NULL;
lf = lineFileOnString("custom HTML", TRUE, text);
while (lineFileNextReal(lf, &line))
    {
    if (sscanf(line, "<!-- UCSC_GB_TRACK NAME=%[^-] -->", buf) == 1)
        {
        if (strlen(ds->string))
            {
            hashAdd(docHash, trackName, dyStringCannibalize(&ds));
            }
        /* remove quotes and surrounding whitespace from track identifier*/
        trackName = skipLeadingSpaces(cloneString(buf));
        eraseTrailingSpaces(trackName);
        stripChar(trackName, '\'');
        stripChar(trackName, '\"');
        ds = dyStringNew(1000);
        continue;
        }
    dyStringAppend(ds, line);
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
if (err)
    /* restore customText data to fill text area */
    cartSetString(cart, hgCtDataText, customText);

doBrowserLines(browserLines);
if (slCount(replacedCts) != 0)
    {
    struct dyString *dsWarn = dyStringNew(0);
    dyStringAppend(dsWarn, "Replacing custom tracks: &nbsp;");
    for (ct = replacedCts; ct != NULL; ct = ct->next)
        {
        if (ct != replacedCts)
            /* not the first */
            dyStringAppend(dsWarn, ",&nbsp;");
        dyStringAppend(dsWarn, ct->tdb->shortLabel);
        }
    warn = dyStringCannibalize(&dsWarn);
    }
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

helpCustom();
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
