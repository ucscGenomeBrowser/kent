// Code to parse list of recommended track sets from file and print as browser dialog
//      for client dialog (js)
//
/* Copyright (C) 2020 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "hCommon.h"
#include "htmshell.h"
#include "hash.h"
#include "web.h"
#include "ra.h"
#include "hgTracks.h"
#include "hgFind.h"
#include "obscure.h"
#include "net.h"

#define REC_TRACK_SETS_FILE  "inc/recommendedTrackSets.tab"

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
    char *db;           // should match db in settings field of named sessions table
    char *userName;     // field in named sessions table
    char *sessionName;  // field in named sessions table (CGI encoded)
    char *description;  // descriptive phrase or sentence.  Display uses this
                                // instead of description in session settings to allow
                                // updating by other than session author (e.g. QA)
    };

char *recTrackSetsFile()
/* Generate path to file specifying menu of recommended track sets */
{
char *root = hDocumentRoot();
char buf[200];
safef(buf, sizeof(buf), "%s/%s", root, REC_TRACK_SETS_FILE);
return cloneString(buf);
}

boolean recTrackSetsEnabled()
/* Return TRUE if feature is available */
{
return fileExists(recTrackSetsFile());
}

struct recTrackSet *loadRecTrackSets()
/* Read from tab-sep file.  Return list */
{
struct recTrackSet *recTrackSet, *recTrackSets = NULL;
struct lineFile *lf = lineFileOpen(recTrackSetsFile(), TRUE);
#define cols 5
char *row[cols];
while (lineFileNextRowTab(lf, row, cols))
    {
    char *db = row[1];
    // limit to sessions in current database
    if (differentString(db, database))
        continue;
    AllocVar(recTrackSet);
    recTrackSet->label = cloneString(row[0]);
    recTrackSet->db = cloneString(row[1]);
    recTrackSet->userName = cloneString(row[2]);
    recTrackSet->sessionName = cloneString(row[3]);
    recTrackSet->description = cloneString(row[4]);
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

void printRecTrackSets()
/* Create dialog with list of recommended track sets */
{
if (!recTrackSetsEnabled())
    return;

struct recTrackSet *recTrackSet, *recTrackSets = loadRecTrackSets();
if (!recTrackSets)
    return;

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
    hPrintf("<li><a class='recTrackSetLink' href='./hgTracks?"
                    "pix=%d&textSize=%s"  // preserve these user settings 
                    "&hgS_otherUserName=%s"
                    "&hgS_otherUserSessionName=%s"
                    "&hgS_otherUserSessionLabel=%s"
                    "&hgS_otherUserSessionDesc=%s"
                    "&hgS_doOtherUser=submit"
                    "&position="        // JS fills in position
                    "'>" 
                "%s</a>: <small>%s</small></li>",
                    tl.picWidth, tl.textSize,
                    recTrackSet->userName, recTrackSet->sessionName, 
                    recTrackSet->label, recTrackSet->description, 
                    recTrackSet->label, recTrackSet->description);
    }
hPrintf("</ul>");

hPrintf("<p>Return to <a href='./hgTracks?hgt.reset=on'>Default</a> browser tracks</p>\n");
hPrintf("</div>\n");
}

