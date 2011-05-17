
#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jsHelper.h"
#include "trackDb.h"
#include "hdb.h"
#include "web.h"
#include "mdb.h"
#include "hCommon.h"
#include "hui.h"
#include "fileUi.h"

#define MAIN_FORM "mainForm"
#define WIGGLE_HELP_PAGE  "../goldenPath/help/hgWiggleTrackHelp.html"

void fileUi(struct cart *cart,struct trackDb *tdb, char *db, char *chrom, boolean ajax)
// Downloadable Files UI
{
if (!ajax)
    {
    jsIncludeFile("jquery.js", NULL);
    webIncludeResourceFile("jquery-ui.css");
    jsIncludeFile("jquery-ui.js", NULL);
    jsIncludeFile("utils.js",NULL);
    }

// QUESTION: Is this needed?  Are we doing a submit on hgTrackUi to get here??  Probably not.
//if(tdbIsContainer(tdb) && !ajax)
//    cartTdbTreeReshapeIfNeeded(cart,tdb);

printf("<B style='font-family:serif; font-size:200%%;'>%s</B>\n", tdb->longLabel);

// If Composite, link to the hgTrackUi.  But if downloadsOnly then link to any superTrack.
#define LINK_TO_PARENT "%s<B style='font-family:serif;'>(<A HREF='%s?%s=%u&c=%s&g=%s' title='Link to %s track settings'><IMG height=12 src='../images/ab_up.gif'>%s</A>)</B>\n"
if (tdbIsComposite(tdb))
    {
    char *encodedTrackName = cgiEncode(tdb->track);
    printf(LINK_TO_PARENT,"&nbsp;&nbsp;", hgTrackUiName(), cartSessionVarName(), cartSessionId(cart), chrom, encodedTrackName,tdb->shortLabel,"Track settings");
    freeMem(encodedTrackName);
    }
else if (tdb->parent) //Print link for parent track
    {
    char *encodedTrackName = cgiEncode(tdb->parent->track);
    printf(LINK_TO_PARENT,"&nbsp;&nbsp;", hgTrackUiName(), cartSessionVarName(), cartSessionId(cart), chrom, encodedTrackName, tdb->parent->shortLabel, tdb->parent->shortLabel);
    freeMem(encodedTrackName);
    }

// NAVLINKS - Link to Description down below
if (tdb->html != NULL && tdb->html[0] != 0)
    {
    printf("<span id='navDown' style='float:right; display:none;'>");
    // First put up a button to go to File Search
    printf("<A HREF='hgFileSearch?db=%s' TITLE='Search for other downloadable files ...'>File Search</A>&nbsp;&nbsp;&nbsp;",db);

    // Now link to description
    char *downArrow = "&dArr;";
    enum browserType browser = cgiBrowser();
    if (browser == btIE || browser == btFF)
        downArrow = "&darr;";
    printf("<A HREF='#TRACK_HTML' TITLE='Jump to description section of page'>Description%s</A>",downArrow);
    printf("</span>");
    }
puts("<BR>");

filesDownloadUi(db,cart,tdb);

// Print data version trackDB setting, if any */
char *version = trackDbSetting(tdb, "dataVersion");
if (version)
    {
    cgiDown(0.7);
    printf("<B>Data version:</B> %s<BR>\n", version);
    }

// Print lift information from trackDb, if any
(void) trackDbPrintOrigAssembly(tdb, db);

if (tdb->html != NULL && tdb->html[0] != 0)
    {
    char *browserVersion;
    if (btIE == cgiClientBrowser(&browserVersion, NULL, NULL) && *browserVersion < '8')
        htmlHorizontalLine();
    else // Move line down, since <H2>Description (in ->html) is proceded by too much space
        printf("<HR ALIGN='bottom' style='position:relative; top:1em;'>");

    printf("<table class='windowSize'><tr valign='top'><td rowspan=2>");
    puts("<A NAME='TRACK_HTML'></A>");    // include anchor for Description link

    // Add pennantIcon
    printPennantIconNote(tdb);

    puts(tdb->html);
    printf("</td><td nowrap>");
    cgiDown(0.7); // positions top link below line
    makeTopLink(tdb);
    printf("&nbsp</td></tr><tr valign='bottom'><td nowrap>");
    makeTopLink(tdb);
    printf("&nbsp</td></tr></table>");
    }
}

void doMiddle(struct cart *cart)
/* Write body of web page. */
{
struct trackDb *tdb = NULL;
char *track;
char *ignored;
char *db = NULL;
track = cartString(cart, "g"); // QUESTION: Should this be 'f' ??
getDbAndGenome(cart, &db, &ignored, NULL);
char *chrom = cartUsualString(cart, "c", hDefaultChrom(db));

tdb = tdbForTrack(db, track,NULL);// We only need to see one tdb.


if (tdb == NULL)
    {
    errAbort("Can't find %s in track database %s", track, db);
    return;
    }
cartWebStart(cart, db, "%s %s", tdb->shortLabel, DOWNLOADS_ONLY_TITLE);

if (!tdbIsComposite(tdb) && !tdbIsDownloadsOnly(tdb))
    {
    warn("Track '%s' of type %s is not supported by hgFileUi.",track, tdb->type);
    return;
    }

fileUi(cart, tdb, db, chrom, FALSE);

printf("<BR>\n");
webEnd();
}

char *excludeVars[] = { "submit", "Submit", "g", NULL, "ajax", NULL,};  // HOW IS 'ajax" going to be supported?

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetBackground(hBackgroundImage());
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
