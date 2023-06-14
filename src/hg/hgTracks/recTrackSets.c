// Code to parse list of recommended track sets from file and print as browser dialog
//      for client dialog (js)
//
/* Copyright (C) 2020 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "dystring.h"
#include "hCommon.h"
#include "hgConfig.h"
#include "htmshell.h"
#include "hash.h"
#include "web.h"
#include "ra.h"
#include "hgTracks.h"
#include "hgFind.h"
#include "obscure.h"
#include "net.h"


/* Recommended track sets are Special 'curated' sessions, created by browser team, e.g. for clinical users
 *      This is expected to be a very limited number (under 10 ?)
 *      The list references sessions in namedSessionDb table, by userName and sessionName
 *      (unfortunately not required to be unique, so depending on curator to 
 *      just make one (code willl pick the first one)
 */      
struct recTrackSet
    {
    struct recTrackSet *next;
    char *label;        // short label for display on browser and dialogs
    char *userName;     // field in named sessions table
    char *sessionName;  // field in named sessions table (CGI encoded)
    char *description;  // descriptive phrase or sentence.  Display uses this
                                // instead of description in session settings to allow
                                // updating by other than session author (e.g. QA)
    };

#define REC_TRACK_SETS_FILE  "recTrackSets"
#define REC_TRACK_SETS_DIR  "inc"
#define REC_TRACK_SETS_EXT  "tab"

char *recTrackSetsFile()
/* Generate path to file specifying menu of recommended track sets.
 *      eg, DOCUMENT_ROOT/inc/recTrackSets.hg19.tab */
{
char *root = hDocumentRoot();
char buf[200];
safef(buf, sizeof buf, "%s/%s/%s.%s.%s", 
        root, REC_TRACK_SETS_DIR, REC_TRACK_SETS_FILE, database, REC_TRACK_SETS_EXT);
return cloneString(buf);
}

boolean recTrackSetsEnabled()
/* Return TRUE if feature is available */
{
char *cfgEnabled = cfgOption("browser.recTrackSets");
return cfgEnabled && (sameString(cfgEnabled, "on") || sameString(cfgEnabled, "true")) &&
        fileExists(recTrackSetsFile());
}

boolean recTrackSetsChangeDetectEnabled()
/* Return TRUE if feature is available, in hgConf */
{
char *cfgChanges = cfgOption("browser.recTrackSetsDetectChange");
if (cfgChanges && (sameString(cfgChanges, "on") || sameString(cfgChanges, "true")))
    return TRUE;
return FALSE;
}

struct recTrackSet *loadRecTrackSets()
/* Read from tab-sep file.  Return list or NULL if no track sets for this database */
{
struct recTrackSet *recTrackSet, *recTrackSets = NULL;
struct lineFile *lf = lineFileOpen(recTrackSetsFile(), TRUE);
if (!lf)
    return NULL;
#define cols 4
char *row[cols];
while (lineFileNextRowTab(lf, row, cols))
    {
    AllocVar(recTrackSet);
    recTrackSet->label = cloneString(row[0]);
    recTrackSet->userName = cloneString(row[1]);
    recTrackSet->sessionName = cloneString(row[2]);
    recTrackSet->description = cloneString(row[3]);
    slAddHead(&recTrackSets, recTrackSet);
    }
slReverse(&recTrackSets);
lineFileClose(&lf);
return recTrackSets;
}

int recTrackSetsForDb()
/* Return number of recommended track sets for this database */
{
return slCount(loadRecTrackSets());
}

boolean hasRecTrackSet(struct cart *cart)
/* Check if currently loaded session is in the recommended track set */
{
if (!recTrackSetsEnabled())
    return FALSE;
struct recTrackSet *ts, *recTrackSets = loadRecTrackSets();
if (!recTrackSets)
    return FALSE;
char *session = cartOptionalString(cart, hgsOtherUserSessionName);
char *user = cartOptionalString(cart, hgsOtherUserName);
if (!session || !user)
    return FALSE;
for (ts = recTrackSets; ts; ts = ts->next)
    {
    if (sameString(replaceChars(ts->sessionName, "%20", " "), session) && 
        sameString(ts->userName, user))
            return TRUE;
    }
return FALSE;
}

void printRecTrackSets()
/* Create dialog with list of recommended track sets */
{
if (!recTrackSetsEnabled())
    return;
struct recTrackSet *recTrackSet, *recTrackSets = loadRecTrackSets();
if (!recTrackSets)
    return;

if (recTrackSetsChangeDetectEnabled())
    jsInline("var recTrackSetsDetectChanges = true;");

hPrintf("<div style='display:none;' id='recTrackSetsPopup' title='Recommended Track Sets'>\n");

// TODO: Consider moving this to the tab file as a header section
hPrintf("<p>These links provide track sets selected and pre-configured for "
            "specific user scenarios. They are designed to be useful at "
            "different genomic loci. Clicking a link below will create a browser "
            "window with these tracks visible, without changing the locus.</p>");

hPrintf("<ul class='indent'>");
for (recTrackSet = recTrackSets; recTrackSet != NULL; recTrackSet = recTrackSet->next)
    {
// TODO: consider libifying hgSession.c:add/getSessionLink() and using that
    boolean mergeSession = cfgOptionBooleanDefault("mergeRecommended", FALSE);

    if (mergeSession)
#define rtsLoadSessionName  "rtsLoad"
        hPrintf("<li><a class='recTrackSetLink' href='./hgTracks?"
                    hgsOtherUserName "=%s"
                    "&" rtsLoadSessionName "=%s"
                    "&hgsid=%s"
                    "&position="        // JS fills in position
                    "'>" 
                "%s</a>: <small>%s</small></li>",
                    recTrackSet->userName, recTrackSet->sessionName, cartSessionId(cart),
                    recTrackSet->label, recTrackSet->description);
    else
        hPrintf("<li><a class='recTrackSetLink' href='./hgTracks?"
                    // preserve these user settings 
                    "pix=%d&textSize=%s&textFont=%s&hgt.labelWidth=%d"
                    "&" hgsOtherUserName "=%s"
                    "&" hgsOtherUserSessionName "=%s"
                    "&" hgsOtherUserSessionLabel "=%s"
                    "&hgS_otherUserSessionDesc=%s"
                    "&" hgsDoOtherUser "=submit"
                    "&position="        // JS fills in position
                    "'>" 
                "%s</a>: <small>%s</small></li>",
                    tl.picWidth, tl.textSize, tl.textFont, tl.leftLabelWidthChars,
                    recTrackSet->userName, recTrackSet->sessionName, 
                    recTrackSet->label, recTrackSet->description,
                    recTrackSet->label, recTrackSet->description);
    }
hPrintf("</ul>");

hPrintf("<p>Return to <a href='./hgTracks?hgt.reset=on'>Default</a> browser tracks.</p>\n");
hPrintf("<p><small><em>This tool is for research use only. For personal medical or "
                "genetic advising, consult a qualified physician.</small></em></p>\n");
hPrintf("</div>\n");
}

