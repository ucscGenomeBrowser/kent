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
#include "searchTracks.h"
#include "cart.h"
#include "grp.h"

#define FAKE_MDB_MULTI_SELECT_SUPPORT

struct hash *trackHash = NULL;	// Is this needed?
boolean measureTiming = FALSE;  /* DON'T EDIT THIS -- use CGI param "&measureTiming=." . */

#define FILE_SEARCH              "hgfs_Search"
#define FILE_SEARCH_FORM         "fileSearch"
#define FILE_SEARCH_CURRENT_TAB  "fsCurTab"
#define FILE_SEARCH_ON_FILETYPE  "fsFileType"

// These are common with trackSearch.  Should they be?
#define TRACK_SEARCH_SIMPLE      "tsSimple"
#define TRACK_SEARCH_ON_NAME     "tsName"
#define TRACK_SEARCH_ON_GROUP    "tsGroup"
#define TRACK_SEARCH_ON_DESCR    "tsDescr"
#define TRACK_SEARCH_SORT        "tsSort"

//#define USE_TABS
//#define SUPPORT_COMPOSITE_SEARCH

#ifdef OMIT_SUPPORT_COMPOSITE_SEARCH
// make a matchString function to support "contains", "is" etc. and wildcards in contains

//    ((sameString(op, "is") && !strcasecmp(track->shortLabel, str)) ||

static boolean isNameMatch(struct trackDb *tdb, char *str, char *op)
{
return str && strlen(str) &&
    ((sameString(op, "is") && !strcasecmp(tdb->shortLabel, str)) ||
    (sameString(op, "is") && !strcasecmp(tdb->longLabel, str)) ||
    (sameString(op, "contains") && containsStringNoCase(tdb->shortLabel, str) != NULL) ||
    (sameString(op, "contains") && containsStringNoCase(tdb->longLabel, str) != NULL));
}

static boolean isDescriptionMatch(struct trackDb *tdb, char **words, int wordCount)
// We parse str and look for every word at the start of any word in track description (i.e. google style).
{
if(words)
    {
    // We do NOT lookup up parent hierarchy for html descriptions.
    char *html = tdb->html;
    if(!isEmpty(html))
        {
        // This probably could be made more efficient by parsing the html into some kind of b-tree, but I am assuming
        // that the inner html loop while only happen for 1-2 words for vast majority of the tracks.

        int i, numMatches = 0;
        html = stripRegEx(html, "<[^>]*>", REG_ICASE);
        for(i = 0; i < wordCount; i++)
            {
            char *needle = words[i];
            char *haystack, *tmp = cloneString(html);
            boolean found = FALSE;
            while((haystack = nextWord(&tmp)))
                {
                char *ptr = strstrNoCase(haystack, needle);
                if(ptr != NULL && ptr == haystack)
                    {
                    found = TRUE;
                    break;
                    }
                }
            if(found)
                numMatches++;
            else
                break;
            }
        if(numMatches == wordCount)
            return TRUE;
        }
    }
return FALSE;
}
#endif///def SUPPORT_COMPOSITE_SEARCH

#ifdef USE_TABS
static struct slRef *simpleSearchForTdbs(struct trix *trix,char **descWords,int descWordCount)
// Performs the simple search and returns the found tracks.
{
struct slRef *foundTdbs = NULL;

struct trixSearchResult *tsList;
for(tsList = trixSearch(trix, descWordCount, descWords, TRUE); tsList != NULL; tsList = tsList->next)
    {
    struct trackDb *tdb = (struct track *) hashFindVal(trackHash, tsList->itemId);
    if (track != NULL)  // It is expected that this is NULL (e.g. when the trix references trackDb tracks which have no tables)
        {
        refAdd(&foundTdbs, tdb);
        }
    }
return foundTdbs;
}
#endif///def USE_TABS

struct slName *tdbListGetGroups(struct trackDb *tdbList)
// Returns a list of groups found in the tdbList
// FIXME: Should be movedf to trackDbCustom and shared
{
struct slName *groupList = NULL;
char *lastGroup = "[]";
struct trackDb *tdb = tdbList;
for(;tdb!=NULL;tdb=tdb->next)
    {
    if (differentString(lastGroup,tdb->grp))
        lastGroup = slNameStore(&groupList, tdb->grp);
    }
return groupList;
}

struct grp *groupsFilterForTdbList(struct grp **grps,struct trackDb *tdbList)
{
struct grp *grpList = *grps;
*grps = NULL;
struct slName *tdbGroups = tdbListGetGroups(tdbList);
if (tdbList == NULL)
    return *grps;

while (grpList != NULL)
    {
    struct grp *grp = slPopHead(&grpList);
    if (slNameInList(tdbGroups,grp->name))
        slAddHead(grps,grp);
    }
slNameFreeList(&tdbGroups);
slReverse(grps);
return *grps;
}

void doSearch(char *db,char *organism,struct cart *cart,struct trackDb *tdbList)
{
if (!advancedJavascriptFeaturesEnabled(cart))
    {
    warn("Requires advanced javascript features.");
    return;
    }
#ifdef SUPPORT_COMPOSITE_SEARCH
char *nameSearch = cartOptionalString(cart, TRACK_SEARCH_ON_NAME);
#endif///def SUPPORT_COMPOSITE_SEARCH
char *fileTypeSearch = cartOptionalString(cart, FILE_SEARCH_ON_FILETYPE);
char *descSearch=FALSE;
boolean doSearch = sameWord(cartUsualString(cart, FILE_SEARCH,"no"), "search");
struct sqlConnection *conn = hAllocConn(db);
boolean metaDbExists = sqlTableExists(conn, "metaDb");
#ifdef ONE_FUNC
struct hash *parents = newHash(4);
#endif///def ONE_FUNC
char **descWords = NULL;
int descWordCount = 0;
boolean searchTermsExist = FALSE;  // FIXME: Why is this needed?
int cols;

#ifdef USE_TABS
enum searchTab selectedTab = simpleTab;
char *currentTab = cartUsualString(cart, FILE_SEARCH_CURRENT_TAB, "simpleTab");
if(sameString(currentTab, "simpleTab"))
    {
    selectedTab = simpleTab;
    descSearch = cartOptionalString(cart, TRACK_SEARCH_SIMPLE);
    #ifdef SUPPORT_COMPOSITE_SEARCH
    freez(&nameSearch);
    #endif///def SUPPORT_COMPOSITE_SEARCH
    }
else if(sameString(currentTab, "filesTab"))
    {
    selectedTab = filesTab;
    descSearch = cartOptionalString(cart, TRACK_SEARCH_ON_DESCR);
    }
#else///ifndef USE_TABS
enum searchTab selectedTab = filesTab;
descSearch = cartOptionalString(cart, TRACK_SEARCH_ON_DESCR);
#endif///ndef USE_TABS

if(descSearch)
    stripChar(descSearch, '"');

#ifdef USE_TABS
struct trix *trix;
char trixFile[HDB_MAX_PATH_STRING];
getSearchTrixFile(db, trixFile, sizeof(trixFile));
trix = trixOpen(trixFile);
#endif///def USE_TABS

printf("<div style='max-width:1080px;'>");
// FIXME: Do we need a form at all?
//printf("<form action='%s' name='%s' id='%s' method='get'>\n\n", hgTracksName(),FILE_SEARCH_FORM,FILE_SEARCH_FORM);
printf("<form action='../cgi-bin/hgFileSearch' name='%s' id='%s' method='get'>\n\n", FILE_SEARCH_FORM,FILE_SEARCH_FORM);
cartSaveSession(cart);  // Creates hidden var of hgsid to avoid bad voodoo
//safef(buf, sizeof(buf), "%lu", clock1());
//cgiMakeHiddenVar("hgt_", buf);  // timestamps page to avoid browser cache

printf("<input type='hidden' name='db' value='%s'>\n", db);
printf("<input type='hidden' name='%s' value=''>\n",TRACK_SEARCH_DEL_ROW);
printf("<input type='hidden' name='%s' value=''>\n",TRACK_SEARCH_ADD_ROW);

#ifdef USE_TABS
printf("<input type='hidden' name='%s' id='currentTab' value='%s'>\n", FILE_SEARCH_CURRENT_TAB, currentTab);
printf("<div id='tabs' style='display:none; %s'>\n"
        "<ul>\n"
        "<li><a href='#simpleTab'><B style='font-size:.9em;font-family: arial, Geneva, Helvetica, san-serif;'>Search</B></a></li>\n"
        "<li><a href='#filesTab'><B style='font-size:.9em;font-family: arial, Geneva, Helvetica, san-serif;'>Files</B></a></li>\n"
        "</ul>\n",cgiBrowser()==btIE?"width:1060px;":"max-width:inherit;");

// Files tab
printf("<div id='simpleTab' style='max-width:inherit;'>\n");

printf("<table id='simpleTable' style='width:100%%; font-size:.9em;'><tr><td colspan='2'>");
printf("<input type='text' name='%s' id='simpleSearch' class='submitOnEnter' value='%s' style='max-width:1000px; width:100%%;' onkeyup='findTracksSearchButtonsEnable(true);'>\n",
        TRACK_SEARCH_SIMPLE,descSearch == NULL ? "" : descSearch);
if (selectedTab==simpleTab && descSearch)
    searchTermsExist = TRUE;

printf("</td></tr><td style='max-height:4px;'></td></tr></table>");
//printf("</td></tr></table>");
printf("<input type='submit' name='%s' id='searchSubmit' value='search' style='font-size:.8em;'>\n", FILE_SEARCH);
printf("<input type='button' name='clear' value='clear' class='clear' style='font-size:.8em;' onclick='findTracksClear();'>\n");
printf("<input type='submit' name='submit' value='cancel' class='cancel' style='font-size:.8em;'>\n");
printf("</div>\n");
//#else///ifndef USE_TABS
//printf("<div id='noTabs' style='width:1060px;'>\n");//,cgiBrowser()==btIE?"width:1060px;":"max-width:inherit;");
#endif///def USE_TABS

// Files tab
printf("<div id='filesTab' style='width:inherit;'>\n"
        "<table id='filesTable' cellSpacing=0 style='width:inherit; font-size:.9em;'>\n");
cols = 8;

#ifdef SUPPORT_COMPOSITE_SEARCH
//// Track Name contains
printf("<tr><td colspan=3></td>");
printf("<td nowrap><b style='max-width:100px;'>Track&nbsp;Name:</b></td>");
printf("<td align='right'>contains</td>\n");
printf("<td colspan='%d'>", cols - 4);
printf("<input type='text' name='%s' id='nameSearch' class='submitOnEnter' value='%s' onkeyup='findTracksSearchButtonsEnable(true);' style='min-width:326px; font-size:.9em;'>",
        TRACK_SEARCH_ON_NAME, nameSearch == NULL ? "" : nameSearch);
printf("</td></tr>\n");

// Description contains
printf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>");
printf("<td><b style='max-width:100px;'>Description:</b></td>");
printf("<td align='right'>contains</td>\n");
printf("<td colspan='%d'>", cols - 4);
printf("<input type='text' name='%s' id='descSearch' value='%s' class='submitOnEnter' onkeyup='findTracksSearchButtonsEnable(true);' style='max-width:536px; width:536px; font-size:.9em;'>",
        TRACK_SEARCH_ON_DESCR, descSearch == NULL ? "" : descSearch);
printf("</td></tr>\n");
if (selectedTab==filesTab && descSearch)
    searchTermsExist = TRUE;

// Set up Group dropdown
struct grp *grps = hLoadGrps(db);
grps = groupsFilterForTdbList(&grps,tdbList);
int numGroups = slCount(grps) + 1; // Add Any
char **groups = needMem(sizeof(char *) * numGroups);
char **labels = needMem(sizeof(char *) * numGroups);
groups[0] = ANYLABEL;
labels[0] = ANYLABEL;
int ix=1;
struct grp *grp = grps;
for (; grp != NULL; grp = grp->next,ix++)
    {
    groups[ix] = cloneString(grp->name);
    labels[ix] = cloneString(grp->label);
    }

printf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
printf("<td><b style='max-width:100px;'>Group:</b></td>");
printf("<td align='right'>is</td>\n");
printf("<td colspan='%d'>", cols - 4);
char *groupSearch = cartOptionalString(cart, TRACK_SEARCH_ON_GROUP);
cgiMakeDropListFull(TRACK_SEARCH_ON_GROUP, labels, groups, numGroups, groupSearch, "class='groupSearch' style='min-width:40%; font-size:.9em;'");
printf("</td></tr>\n");
if (selectedTab==filesTab && groupSearch)
    searchTermsExist = TRUE;
#endif///def SUPPORT_COMPOSITE_SEARCH

// Track Type is (drop down)
#ifdef SUPPORT_COMPOSITE_SEARCH
printf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
#else///ifndef SUPPORT_COMPOSITE_SEARCH
printf("<tr><td colspan=2></td><td align='right'>&nbsp;</td>\n");
#endif///ndef SUPPORT_COMPOSITE_SEARCH
//printf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n"); // Bring back "and" if using "Track Name,Description or Group
printf("<td nowrap><b style='max-width:100px;'>Data Format:</b></td>");
printf("<td align='right'>is</td>\n");
printf("<td colspan='%d'>", cols - 4);
char *dropDownHtml = fileFormatSelectHtml(FILE_SEARCH_ON_FILETYPE,fileTypeSearch,"style='min-width:40%; font-size:.9em;'");
if (dropDownHtml)
    {
    puts(dropDownHtml);
    freeMem(dropDownHtml);
    }
printf("</td></tr>\n");
if (selectedTab==filesTab && fileTypeSearch)
    searchTermsExist = TRUE;

// mdb selects
struct slPair *mdbSelects = NULL;
if(metaDbExists)
    {
    struct slPair *mdbVars = mdbVarsRelevant(conn);
    mdbSelects = mdbSelectPairs(cart,selectedTab, mdbVars);
    char *output = mdbSelectsHtmlRows(conn,mdbSelects,mdbVars,cols);
    if (output)
        {
        puts(output);
        freeMem(output);
        }
    slPairFreeList(&mdbVars);
    }

printf("</table>\n");
printf("<input type='submit' name='%s' id='searchSubmit' value='search' style='font-size:.8em;'>\n", FILE_SEARCH);
printf("<input type='button' name='clear' value='clear' class='clear' style='font-size:.8em;' onclick='findTracksClear();'>\n");
printf("<input type='submit' name='submit' value='cancel' class='cancel' style='font-size:.8em;'>\n");
//printf("<a target='_blank' href='../goldenPath/help/trackSearch.html'>help</a>\n");
printf("</div>\n");

#ifdef USE_TABS
printf("</div>\n"); // End tabs div
#endif///def USE_TABS

printf("</form>\n");
printf("</div>"); // Restricts to max-width:1000px;

if (measureTiming)
    uglyTime("Rendered tabs");

if(descSearch != NULL && !strlen(descSearch))
    descSearch = NULL;
#ifdef SUPPORT_COMPOSITE_SEARCH
if(groupSearch != NULL && sameString(groupSearch, ANYLABEL))
    groupSearch = NULL;
#endif///def SUPPORT_COMPOSITE_SEARCH

if(!isEmpty(descSearch))
    {
    char *tmp = cloneString(descSearch);
    char *val = nextWord(&tmp);
    struct slName *el, *descList = NULL;
    int i;
    while (val != NULL)
        {
        slNameAddTail(&descList, val);
        descWordCount++;
        val = nextWord(&tmp);
        }
    descWords = needMem(sizeof(char *) * descWordCount);
    for(i = 0, el = descList; el != NULL; i++, el = el->next)
        descWords[i] = strLower(el->name);
    }
if (doSearch && selectedTab==simpleTab && descWordCount <= 0)
    doSearch = FALSE;

if(doSearch)
    {
    // Now search
#ifdef USE_TABS
    struct slRef *foundTdbs = NULL;
    if(selectedTab==simpleTab)
        {
        foundTdbs = simpleSearchForTdbs(trix,descWords,descWordCount);
        // What to do now?
        if (measureTiming)
            uglyTime("Searched for tracks");

        // Sort and Print results
        if(selectedTab!=filesTab)
            {
            enum sortBy sortBy = cartUsualInt(cart,TRACK_SEARCH_SORT,sbRelevance);
            int tracksFound = slCount(foundTdbs);
            if(tracksFound > 1)
                findTracksSort(&tracks,sortBy);

            displayFoundTracks(cart,tracks,tracksFound,sortBy);

            if (measureTiming)
                uglyTime("Displayed found files");
            }
        }
    else if(selectedTab==filesTab && mdbPairs != NULL)
#endif///def USE_TABS
        {
        fileSearchResults(db, conn, mdbSelects, fileTypeSearch);
        if (measureTiming)
            uglyTime("Searched for files");
        }

    slPairFreeList(&mdbSelects);
    }
hFreeConn(&conn);

webNewSection("About Downloadable Files Search");
if(metaDbExists)
    printf("<p>Search for terms in track names, descriptions, groups, and ENCODE "
            "metadata.  If multiple terms are entered, only tracks with all terms "
            "will be part of the results.");
else
    printf("<p>Search for terms in track descriptions, groups, and names. "
            "If multiple terms are entered, only tracks with all terms "
            "will be part of the results.");
printf("<BR><a target='_blank' href='../goldenPath/help/trackSearch.html'>more help</a></p>\n");
webEndSectionTables();
}

void doMiddle(struct cart *cart)
/* Write body of web page. */
{
struct trackDb *tdbList = NULL;
char *organism = NULL;
char *db = NULL;
getDbAndGenome(cart, &db, &organism, NULL);
char *chrom = cartUsualString(cart, "c", hDefaultChrom(db));
measureTiming = isNotEmpty(cartOptionalString(cart, "measureTiming"));

// QUESTION: Do We need track list ???  trackHash ??? Can't we just get one track and no children
trackHash = trackHashMakeWithComposites(db,chrom,&tdbList,FALSE);

cartWebStart(cart, db, "Search for Downloadable Files in the %s %s Assembly", organism, hFreezeFromDb(db));

webIncludeResourceFile("HGStyle.css");
webIncludeResourceFile("jquery-ui.css");
webIncludeResourceFile("ui.dropdownchecklist.css");
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);
//jsIncludeFile("ui.core.js",NULL);   // NOTE: This appears to be not needed as long as jquery-ui.js comes before ui.dropdownchecklist.js
jsIncludeFile("ui.dropdownchecklist.js",NULL);
jsIncludeFile("utils.js",NULL);

// This line is needed to get the multi-selects initialized
//printf("<script type='text/javascript'>$(document).ready(function() { setTimeout('updateMetaDataHelpLinks(0);',50);  $('.filterBy').each( function(i) { $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: true });});});</script>\n");
printf("<script type='text/javascript'>$(document).ready(function() { updateMetaDataHelpLinks(0);  $('.filterBy').each( function(i) { $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: true });});});</script>\n");

doSearch(db,organism,cart,tdbList);


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

// TODO:
// 1) Done: Limit to first 1000
// 2) SORT OF: Work out strangeness with dropdownchecklist and use in hgTracks (By some miracle multiselect is working in my hgTracks)
// 3) Work out support for selecting composites and limiting search to those
// 4) Work out simple verses advanced tabs
// 5) work out support for non-encode downloads
// 6) Make an hgTrackSearch to replces hgTracks track search ??   Silpler code, but may not be good idea.
