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
#include "search.h"
#include "cart.h"
#include "grp.h"

#define FAKE_MDB_MULTI_SELECT_SUPPORT

struct hash *trackHash = NULL;	// Is this needed?
boolean measureTiming = FALSE;  /* DON'T EDIT THIS -- use CGI param "&measureTiming=." . */

#define FILE_SEARCH_WHAT "Downloadable ENCODE Files"
#define FILE_SEARCH_NAME FILE_SEARCH_WHAT " Search"

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

static struct trackDb *tdbFilterBy(struct trackDb **pTdbList, char *name, char *description, char *group)
// returns tdbs that match supplied criterion, leaving unmatched in list passed in
{
// Set the word lists up once
struct slName *nameList = NULL;
if (name)
    nameList = slNameListOfUniqueWords(cloneString(name),TRUE); // TRUE means respect quotes
struct slName *descList = NULL;
if (description)
    descList = slNameListOfUniqueWords(cloneString(description),TRUE);

struct trackDb *tdbList = *pTdbList;
struct trackDb *tdbRejects = NULL;
struct trackDb *tdbMatched = NULL;

while (tdbList != NULL)
    {
    struct trackDb *tdb = slPopHead(&tdbList);

    if (!tdbIsComposite(tdb))
        slAddHead(&tdbRejects,tdb);
    else if (group && differentString(tdb->grp,group))
        slAddHead(&tdbRejects,tdb);
    else if (name && !searchNameMatches(tdb, nameList))
        slAddHead(&tdbRejects,tdb);
    else if (description && !searchDescriptionMatches(tdb, descList))
        slAddHead(&tdbRejects,tdb);
    else
        slAddHead(&tdbMatched,tdb);
    }
*pTdbList = tdbRejects;

//warn("matched %d tracks",slCount(tdbMatched));
return tdbMatched;
}

static boolean mdbSelectsAddFoundComposites(struct slPair **pMdbSelects,struct trackDb *tdbsFound)
// Adds a composite mdbSelect (if found in tdbsFound) to the head of the pairs list.
// If tdbsFound is NULL, then add dummy composite search criteria
{
// create comma separated list of composites
struct dyString *dyComposites = dyStringNew(256);
struct trackDb *tdb = tdbsFound;
for(;tdb != NULL; tdb = tdb->next)
    {
    if (tdbIsComposite(tdb))
        dyStringPrintf(dyComposites,"%s,",tdb->track);
    else if (tdbIsCompositeChild(tdb))
        {
        struct trackDb *composite = tdbGetComposite(tdb);
        dyStringPrintf(dyComposites,"%s,",composite->track);
        }
    }
if (dyStringLen(dyComposites) > 0)
    {
    char *composites = dyStringCannibalize(&dyComposites);
    composites[strlen(composites) - 1] = '\0';  // drop the last ','
    //warn("Found composites: %s",composites);
    slPairAdd(pMdbSelects,MDB_VAR_COMPOSITE,composites); // Composite should not already be in the list, because it is only indirectly sortable
    return TRUE;
    }

//warn("No composites found");
dyStringFree(&dyComposites);
return FALSE;
}

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
// FIXME: Should be moved to trackDbCustom and shared
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

static void doFileSearch(char *db,char *organism,struct cart *cart,struct trackDb *tdbList)
{
if (!advancedJavascriptFeaturesEnabled(cart))
    {
    warn("Requires advanced javascript features.");
    return;
    }
struct sqlConnection *conn = hAllocConn(db);
boolean metaDbExists = sqlTableExists(conn, "metaDb");
if (!sqlTableExists(conn, "metaDb"))
    {
    warn("Assembly %s %s does not support Downloadable Files search.", organism, hFreezeFromDb(db));
    hFreeConn(&conn);
    return;
    }
char *nameSearch = cartOptionalString(cart, TRACK_SEARCH_ON_NAME);
char *descSearch=NULL;
char *fileTypeSearch = cartOptionalString(cart, FILE_SEARCH_ON_FILETYPE);
boolean doSearch = sameWord(cartUsualString(cart, FILE_SEARCH,"no"), "search");
#ifdef ONE_FUNC
struct hash *parents = newHash(4);
#endif///def ONE_FUNC
boolean searchTermsExist = FALSE;  // FIXME: Why is this needed?
int cols;

#ifdef USE_TABS
enum searchTab selectedTab = simpleTab;
char *currentTab = cartUsualString(cart, FILE_SEARCH_CURRENT_TAB, "simpleTab");
if(sameString(currentTab, "simpleTab"))
    {
    selectedTab = simpleTab;
    descSearch = cartOptionalString(cart, TRACK_SEARCH_SIMPLE);
    freez(&nameSearch);
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

#ifdef USE_TABS
struct trix *trix;
char trixFile[HDB_MAX_PATH_STRING];
getSearchTrixFile(db, trixFile, sizeof(trixFile));
trix = trixOpen(trixFile);
#endif///def USE_TABS

printf("<div style='max-width:1080px;'>");
// FIXME: Do we need a form at all?
printf("<form action='../cgi-bin/hgFileSearch' name='%s' id='%s' method='get'>\n\n", FILE_SEARCH_FORM,FILE_SEARCH_FORM);
cartSaveSession(cart);  // Creates hidden var of hgsid to avoid bad voodoo

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
printf("<input type='submit' name='%s' id='searchSubmit' value='search' style='font-size:.8em;'>\n", FILE_SEARCH);
printf("<input type='button' name='clear' value='clear' class='clear' style='font-size:.8em;' onclick='findTracksClear();'>\n");
printf("<input type='submit' name='submit' value='cancel' class='cancel' style='font-size:.8em;'>\n");
printf("</div>\n");
#endif///def USE_TABS

// Files tab
printf("<div id='filesTab' style='width:inherit;'>\n"
        "<table id='filesTable' cellSpacing=0 style='width:inherit; font-size:.9em;'>\n");
cols = 8;

// Track Name contains
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

// Track Type is (drop down)
printf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
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
    struct slPair *mdbVars = mdbVarsSearchable(conn,FALSE,TRUE); // Not tables, just files
    mdbSelects = mdbSelectPairs(cart, mdbVars);
    char *output = mdbSelectsHtmlRows(conn,mdbSelects,mdbVars,cols,TRUE); // restricted to file search
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

if(nameSearch != NULL && !strlen(nameSearch))
    nameSearch = NULL;
if(descSearch != NULL && !strlen(descSearch))
    descSearch = NULL;
if(groupSearch != NULL && sameString(groupSearch, ANYLABEL))
    groupSearch = NULL;

printf("</form>\n");
printf("</div>"); // Restricts to max-width:1000px;
cgiDown(0.8);

if (measureTiming)
    uglyTime("Rendered tabs");


#ifdef USE_TABS
if (doSearch && selectedTab==simpleTab && isEmpty(descSearch))
    doSearch = FALSE;
#endif///def USE_TABS

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
        if (nameSearch || descSearch || groupSearch)
            {  // Use nameSearch, descSearch and groupSearch to narrow down the list of composites.

            if (isNotEmpty(nameSearch) || isNotEmpty(descSearch) || isNotEmpty(groupSearch))
                {
                struct trackDb *tdbList = hTrackDb(db);
                struct trackDb *tdbsMatch = tdbFilterBy(&tdbList, nameSearch, descSearch, groupSearch);

                // Now we have a list of tracks, so we need a unique list of composites to add to mdbSelects
                doSearch = mdbSelectsAddFoundComposites(&mdbSelects,tdbsMatch);
                }
            }

        if (doSearch && mdbSelects != NULL && isNotEmpty(fileTypeSearch))
            fileSearchResults(db, conn, mdbSelects, fileTypeSearch);
        else
            printf("<DIV id='filesFound'><BR>No files found.<BR></DIV><BR>\n");

        if (measureTiming)
            uglyTime("Searched for files");
        }

    slPairFreeList(&mdbSelects);
    }
hFreeConn(&conn);

webNewSection("About " FILE_SEARCH_NAME);
printf("Search for downloadable ENCODE files by entering search terms in "
        "the Track name or Description fields and/or by making selections with "
        "the group, data format, and/or ENCODE metadata drop-downs.");
printf("<BR><a target='_blank' href='../goldenPath/help/fileSearch.html'>more help</a>\n");
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

cartWebStart(cart, db, "Search for " FILE_SEARCH_WHAT " in the %s %s Assembly", organism, hFreezeFromDb(db));

// This cleverness allows us to have the background image like "Track Search" does, without all the hgTracks overhead
printf("<style type='text/css'>body {background-image:url('%s');}</style>",hBackgroundImage());

webIncludeResourceFile("HGStyle.css");
webIncludeResourceFile("jquery-ui.css");
webIncludeResourceFile("ui.dropdownchecklist.css");
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("ui.dropdownchecklist.js",NULL);
jsIncludeFile("utils.js",NULL);

// This line is needed to get the multi-selects initialized
#ifdef NEW_JQUERY
jsIncludeFile("ddcl.js",NULL);
printf("<script type='text/javascript'>var newJQuery=true;</script>\n");
printf("<script type='text/javascript'>$(document).ready(function() { updateMetaDataHelpLinks(0); });</script>\n");
#else///ifndef NEW_JQUERY
printf("<script type='text/javascript'>var newJQuery=false;</script>\n");
printf("<script type='text/javascript'>$(document).ready(function() { updateMetaDataHelpLinks(0);  $('.filterBy').each( function(i) { $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: true, maxDropHeight: filterByMaxHeight(this) });});});</script>\n");
#endif///ndef NEW_JQUERY

doFileSearch(db,organism,cart,tdbList);


printf("<BR>\n");
webEnd();
}

char *excludeVars[] = { "submit", "Submit", "g", "ajax", FILE_SEARCH,TRACK_SEARCH_ADD_ROW,TRACK_SEARCH_DEL_ROW};  // HOW IS 'ajax" going to be supported?

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
// 2) Work out simple verses advanced tabs
// 3) work out support for non-encode downloads
// 4) Make an hgTrackSearch to replace hgTracks track search ??   Simlpler code, but may not be good idea because of composite reshaping in cart vars
