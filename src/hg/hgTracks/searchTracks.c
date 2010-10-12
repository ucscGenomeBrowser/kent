/* Track search code used by hgTracks CGI */

#include "common.h"
#include "searchTracks.h"
#include "hCommon.h"
#include "memalloc.h"
#include "obscure.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "htmshell.h"
#include "cart.h"
#include "hgTracks.h"
#include "web.h"
#include "jksql.h"
#include "hdb.h"
#include "mdb.h"
#include "trix.h"
#include "jsHelper.h"
#include "imageV2.h"

static char const rcsid[] = "$Id: searchTracks.c,v 1.11 2010/06/11 18:21:40 larrym Exp $";

#define ANYLABEL "Any"
#define METADATA_NAME_PREFIX "hgt.metadataName"
#define METADATA_VALUE_PREFIX "hgt.metadataValue"

static int gCmpGroup(const void *va, const void *vb)
/* Compare groups based on label. */
{
const struct group *a = *((struct group **)va);
const struct group *b = *((struct group **)vb);
return strcmp(a->label, b->label);
}

// Would like to do a radio button choice ofsorts
#define SORT_BY_VAR           "hgt_sortFound"
enum sortBy
    {
    sbRelevance=0,
    sbAbc      =1,
    sbHierarchy=2,
    };
static int gCmpTrackHierarchy(const void *va, const void *vb)
/* Compare tracks based on longLabel. */
{
const struct slRef *aa = *((struct slRef **)va);
const struct slRef *bb = *((struct slRef **)vb);
const struct track *a = ((struct track *) aa->val);
const struct track *b = ((struct track *) bb->val);
     if ( tdbIsFolder(a->tdb) && !tdbIsFolder(b->tdb))
        return -1;
else if (!tdbIsFolder(a->tdb) &&  tdbIsFolder(b->tdb))
        return 1;
     if ( tdbIsContainer(a->tdb) && !tdbIsContainer(b->tdb))
        return -1;
else if (!tdbIsContainer(a->tdb) &&  tdbIsContainer(b->tdb))
        return 1;
     if (!tdbIsContainerChild(a->tdb) &&  tdbIsContainerChild(b->tdb))
        return -1;
else if ( tdbIsContainerChild(a->tdb) && !tdbIsContainerChild(b->tdb))
        return 1;
return strcasecmp(a->longLabel, b->longLabel);
}

static int gCmpTrack(const void *va, const void *vb)
/* Compare tracks based on longLabel. */
{
const struct slRef *aa = *((struct slRef **)va);
const struct slRef *bb = *((struct slRef **)vb);
const struct track *a = ((struct track *) aa->val);
const struct track *b = ((struct track *) bb->val);
return strcasecmp(a->longLabel, b->longLabel);
}

static void findTracksSort(struct slRef **pTrack, boolean simpleSearch, enum sortBy sortBy)
{
if (sortBy == sbHierarchy)
    slSort(pTrack, gCmpTrackHierarchy);
else if (sortBy == sbAbc)
    slSort(pTrack, gCmpTrack);
else
    slReverse(pTrack);
}


// XXXX make a matchString function to support "contains", "is" etc. and wildcards in contains

//    ((sameString(op, "is") && !strcasecmp(track->shortLabel, str)) ||

static boolean isNameMatch(struct track *track, char *str, char *op)
{
return str && strlen(str) &&
    ((sameString(op, "is") && !strcasecmp(track->shortLabel, str)) ||
    (sameString(op, "is") && !strcasecmp(track->longLabel, str)) ||
    (sameString(op, "contains") && containsStringNoCase(track->shortLabel, str) != NULL) ||
    (sameString(op, "contains") && containsStringNoCase(track->longLabel, str) != NULL));
}

static boolean isDescriptionMatch(struct track *track, char **words, int wordCount)
// We parse str and look for every word at the start of any word in track description (i.e. google style).
{
if(words)
    {
    // We do NOT lookup up parent hierarchy for html descriptions.
    char *html = track->tdb->html;
    if(!isEmpty(html))
        {
        /* This probably could be made more efficient by parsing the html into some kind of b-tree, but I am assuming
           that the inner html loop while only happen for 1-2 words for vast majority of the tracks. */

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

static int getTermArray(struct sqlConnection *conn, char ***pLabels, char ***pTerms, char *type)
// Pull out all term fields from ra entries with given type
// Returns count of items found and items via the terms argument.
{
int ix = 0, count = 0;
char **labels;
char **values;
struct slPair *pairs = mdbValLabelSearch(conn, type, MDB_VAL_STD_TRUNCATION, TRUE, FALSE); // Tables not files
count = slCount(pairs) + 1; // make room for "Any"
AllocArray(labels, count);
AllocArray(values, count);
labels[ix] = cloneString(ANYLABEL);
values[ix] = cloneString(ANYLABEL);
struct slPair *pair = NULL;
while((pair = slPopHead(&pairs)) != NULL)
    {
    ix++;
    labels[ix] = pair->name;
    values[ix] = pair->val;
    freeMem(pair);
    }
*pLabels = labels;
*pTerms = values;
return count;
}

static int metaDbVars(struct sqlConnection *conn, char *** metaVars, char *** metaLabels)
// Search the assemblies metaDb table; If name == NULL, we search every metadata field.
{
char query[256];
#define WHITE_LIST_COUNT 35
#ifdef WHITE_LIST_COUNT
#define WHITE_LIST_VAR 0
#define WHITE_LIST_LABEL 1
char *whiteList[WHITE_LIST_COUNT][2] = {
    {"age",              "Age of experimental organism"},
    {"antibody",         "Antibody or target protein"},
    {"origAssembly",     "Assembly originally mapped to"},
    {"cell",             "Cell, tissue or DNA sample"},
    {"localization",     "Cell compartment"},
    {"control",          "Control or Input for ChIPseq"},
    //{"controlId",        "ControlId - explicit relationship"},
    {"dataType",         "Experiment type"},
    {"dataVersion",      "ENCODE release"},
    //{"fragLength",       "Fragment Length for ChIPseq"},
    //{"freezeDate",       "Gencode freeze date"},
    //{"level",            "Gencode level"},
    //{"annotation",       "Gencode annotation"},
    {"geoSample",        "GEO accession"},
    {"growthProtocol",   "Growth Protocol"},
    {"lab",              "Lab producing data"},
    {"labVersion",       "Lab specific details"},
    {"labExpId",         "Lab specific identifier"},
    {"softwareVersion",  "Lab specific informatics"},
    {"protocol",         "Library Protocol"},
    {"mapAlgorithm",     "Mapping algorithm"},
    {"readType",         "Paired/Single reads lengths"},
    {"grant",            "Principal Investigator"},
    {"replicate",        "Replicate number"},
    //{"restrictionEnzyme","Restriction Enzyme used"},
    //{"ripAntibody",      "RIP Antibody"},
    //{"ripTgtProtein",    "RIP Target Protein"},
    {"rnaExtract",       "RNA Extract"},
    {"seqPlatform",      "Sequencing Platform"},
    {"setType",          "Experiment or Input"},
    {"sex",              "Sex of organism"},
    {"strain",           "Strain of organism"},
    {"subId",            "Submission Id"},
    {"treatment",        "Treatment"},
    {"view",             "View - Peaks or Signals"},
};
// FIXME: The whitelist should be a table or ra
// FIXME: The whitelist should be in list order
// FIXME: Should read in list, then verify that an mdb val exists.

char **retVar = needMem(sizeof(char *) * WHITE_LIST_COUNT);
char **retLab = needMem(sizeof(char *) * WHITE_LIST_COUNT);
int ix,count;
for(ix=0,count=0;ix<WHITE_LIST_COUNT;ix++)
    {
    safef(query, sizeof(query), "select count(*) from metaDb where var = '%s'",whiteList[ix][WHITE_LIST_VAR]);
    if(sqlQuickNum(conn,query) > 0)
        {
        retVar[count] = whiteList[ix][WHITE_LIST_VAR];
        retLab[count] = whiteList[ix][WHITE_LIST_LABEL];
        count++;
        }
    }
if(count == 0)
    {
    freez(&retVar);
    freez(&retLab);
    }
*metaVars = retVar;
*metaLabels = retLab;
return count;

#else///ifndef WHITE_LIST_COUNT

char **retVar;
char **retLab;
struct slName *el, *varList = NULL;
struct sqlResult *sr = NULL;
char **row = NULL;

safef(query, sizeof(query), "select distinct var from metaDb order by var");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    slNameAddHead(&varList, row[0]);
sqlFreeResult(&sr);
retVar = needMem(sizeof(char *) * slCount(varList));
retLab = needMem(sizeof(char *) * slCount(varList));
slReverse(&varList);
//slNameSort(&varList);
int count = 0;
for (el = varList; el != NULL; el = el->next)
    {
    retVar[count] = el->name;
    retLab[count] = el->name;
    count++;
    }
*metaVars = retVar;
*whiteLabels = retLab;
return count;
#endif///ndef WHITE_LIST_COUNT
}

#define MAX_FOUND_TRACKS 100
#define FOUND_TRACKS_PAGING "hgt_startFrom"
void findTracksPageLinks(int tracksFound, int startFrom)
{
if (tracksFound <= MAX_FOUND_TRACKS)
    return;

// Opener
int curPage  = (startFrom/MAX_FOUND_TRACKS) + 1;
int endAt = startFrom+MAX_FOUND_TRACKS;
if (endAt > tracksFound)
    endAt = tracksFound;
hPrintf("<em>Listing %d - %d of %d tracks</em>&nbsp;&nbsp;&nbsp;",startFrom+1,endAt,tracksFound);

// << and <
hPrintf("<a href='' title='First page of found tracks' onclick='return findTracksPage(\"%s\",%d);'>&lt;&lt;</a>&nbsp;",FOUND_TRACKS_PAGING,0);
if (startFrom >= MAX_FOUND_TRACKS)
    hPrintf("&nbsp;<a href='' title='Previous page of found tracks' onclick='return findTracksPage(\"%s\",%d);'>&lt;</a>&nbsp;",FOUND_TRACKS_PAGING,startFrom - MAX_FOUND_TRACKS);
else
    hPrintf("&nbsp;<em>&lt;</em>&nbsp;");

// page number links
int lastPage = (tracksFound/MAX_FOUND_TRACKS);
if ((tracksFound % MAX_FOUND_TRACKS) > 0)
    lastPage++;

int thisPage = curPage - 3; // Window of 3 pages above and below
if (thisPage < 1)
    thisPage = 1;
for (;thisPage <= lastPage && thisPage <= curPage + 3; thisPage++)
    {
    if (thisPage != curPage)
        {
        int willStartAt = ((thisPage - 1) * MAX_FOUND_TRACKS);
        endAt = willStartAt+ MAX_FOUND_TRACKS;
        if (endAt > tracksFound)
            endAt = tracksFound;
        hPrintf("&nbsp;<a href='' title='Page %d (%d - %d) tracks' onclick='return findTracksPage(\"%s\",%d);'>%d</a>&nbsp;",
                thisPage,willStartAt+1,endAt,FOUND_TRACKS_PAGING,((thisPage - 1) * MAX_FOUND_TRACKS),thisPage);
        }
    else
        hPrintf("&nbsp;<em>%d</em>&nbsp;",thisPage);
    }

// > and >>
if ((startFrom + MAX_FOUND_TRACKS) < tracksFound)
    hPrintf("&nbsp;<a href='' title='Next page of found tracks' onclick='return findTracksPage(\"%s\",%d);'>&gt;</a>&nbsp;",FOUND_TRACKS_PAGING,(startFrom + MAX_FOUND_TRACKS));
else
    hPrintf("&nbsp;<em>&gt;</em>&nbsp;");
thisPage =  tracksFound - (tracksFound % MAX_FOUND_TRACKS);
if (thisPage == tracksFound)
    thisPage -= MAX_FOUND_TRACKS;
hPrintf("&nbsp;<a href='' title='Last page of found tracks' onclick='return findTracksPage(\"%s\",%d);'>&gt;&gt;</a>\n",FOUND_TRACKS_PAGING,thisPage);
}

void doSearchTracks(struct group *groupList)
{
struct group *group;
char *groups[128];
char *labels[128];
int numGroups = 1;
groups[0] = ANYLABEL;
labels[0] = ANYLABEL;
char *currentTab = cartUsualString(cart, "hgt.currentSearchTab", "simpleTab");
char *nameSearch = cartOptionalString(cart, "hgt.nameSearch");
char *descSearch;
char *groupSearch = cartOptionalString(cart, "hgt.groupSearch");
boolean doSearch = sameString(cartOptionalString(cart, searchTracks), "Search") || cartUsualInt(cart, "hgt.forceSearch", 0) == 1;
struct sqlConnection *conn = hAllocConn(database);
boolean metaDbExists = sqlTableExists(conn, "metaDb");
struct slRef *tracks = NULL;
int numMetadataSelects, tracksFound = 0;
int numMetadataNonEmpty = 0;
char **mdbVar;
char **mdbVal;
struct hash *parents = newHash(4);
boolean simpleSearch;
struct trix *trix;
char trixFile[HDB_MAX_PATH_STRING];
char **descWords = NULL;
int descWordCount = 0;
boolean searchTermsExist = FALSE;
int cols;

if(sameString(currentTab, "simpleTab"))
    {
    descSearch = cartOptionalString(cart, "hgt.simpleSearch");
    simpleSearch = TRUE;
    freez(&nameSearch);
    freez(&groupSearch);
    }
else
    {
    descSearch = cartOptionalString(cart, "hgt.descSearch");
    simpleSearch = FALSE;
    }

trackList = getTrackList(&groupList, -2); // global
makeGlobalTrackHash(trackList);

// NOTE: This is necessary when container cfg by '*' results in vis changes
// This will handle composite/view override when subtrack specific vis exists, AND superTrack reshaping.
#ifdef SUBTRACKS_HAVE_VIS
parentChildCartCleanup(trackList,cart,oldVars); // Subtrack settings must be removed when composite/view settings are updated
#endif///def SUBTRACKS_HAVE_VIS

getSearchTrixFile(database, trixFile, sizeof(trixFile));
trix = trixOpen(trixFile);
slSort(&groupList, gCmpGroup);
for (group = groupList; group != NULL; group = group->next)
    {
    groupTrackListAddSuper(cart, group);
    if (group->trackList != NULL)
        {
        groups[numGroups] = cloneString(group->name);
        labels[numGroups] = cloneString(group->label);
        numGroups++;
        if (numGroups >= ArraySize(groups))
            internalErr();
        }
    }
webStartWrapperDetailedNoArgs(cart, database, "", "Search for Tracks", FALSE, FALSE, FALSE, FALSE);

hPrintf("<div style='max-width:1080px;'>");
hPrintf("<form action='%s' name='SearchTracks' id='searchTracks' method='get'>\n\n", hgTracksName());
cartSaveSession(cart);  // Creates hidden var of hgsid to avoid bad voodoo

hPrintf("<input type='hidden' name='db' value='%s'>\n", database);
hPrintf("<input type='hidden' name='hgt.currentSearchTab' id='currentSearchTab' value='%s'>\n", currentTab);
hPrintf("<input type='hidden' name='hgt.delRow' value=''>\n");
hPrintf("<input type='hidden' name='hgt.addRow' value=''>\n");
hPrintf("<input type='hidden' name='hgt.forceSearch' value=''>\n");

hPrintf("<div id='tabs' style='display:none; %s'>\n"
        "<ul>\n"
        "<li><a href='#simpleTab'><span>Search</span></a></li>\n"
        "<li><a href='#advancedTab'><span>Advanced</span></a></li>\n"
        "</ul>\n"
        "<div id='simpleTab' style='max-width:inherit;'>\n",cgiBrowser()==btIE?"width:1060px;":"max-width:inherit;");

hPrintf("<table style='width:100%%;'><tr><td colspan='2'>");
hPrintf("<input type='text' name='hgt.simpleSearch' id='simpleSearch' value='%s' style='max-width:1000px; width:100%%' onkeyup='findTracksSearchButtonsEnable(true);'>\n", descSearch == NULL ? "" : descSearch);
hPrintf("</td></tr><tr><td>");
if (simpleSearch && descSearch)
    searchTermsExist = TRUE;

hPrintf("<input type='submit' name='%s' id='searchSubmit' value='Search' style='font-size:14px;'>\n", searchTracks);
hPrintf("<input type='button' name='clear' value='Clear' class='clear' style='font-size:14px;' onclick='findTracksClear();'>\n");
hPrintf("<input type='submit' name='submit' value='Cancel' class='cancel' style='font-size:14px;'>\n");
hPrintf("<a target='_blank' href='../goldenPath/help/trackSearch.html'>help</a></td></tr></table>\n");
//hPrintf("</td><td align='right'><a target='_blank' href='../goldenPath/help/trackSearch.html'>help</a></td></tr></table>\n");
hPrintf("</div>\n"
        "<div id='advancedTab' style='width:inherit;'>\n"
        "<table cellSpacing=0 style='width:inherit;'>\n");

cols = 7;

// Track Name contains
hPrintf("<tr><td colspan=3></td>");
hPrintf("<td nowrap><b style='max-width:100px;'>Track&nbsp;Name:</b></td>");
hPrintf("<td align='right'>contains</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
hPrintf("<input type='text' name='hgt.nameSearch' id='nameSearch' value='%s' onkeyup='findTracksSearchButtonsEnable(true);' style='min-width:326px;'>", nameSearch == NULL ? "" : nameSearch);
hPrintf("</td></tr>\n");


// Description contains
hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>");
hPrintf("<td><b style='max-width:100px;'>Description:</b></td>");
hPrintf("<td align='right'>contains</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
hPrintf("<input type='text' name='hgt.descSearch' id='descSearch' value='%s' onkeyup='findTracksSearchButtonsEnable(true);' style='max-width:536px; width:536px;'>",
        descSearch == NULL ? "" : descSearch);
hPrintf("</td></tr>\n");
if (!simpleSearch && descSearch)
    searchTermsExist = TRUE;

hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
hPrintf("<td><b style='max-width:100px;'>Group</b></td>");
hPrintf("<td align='right'>is</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
cgiMakeDropListFull("hgt.groupSearch", labels, groups, numGroups, groupSearch, "class='groupSearch' style='min-width:40%%;'");
hPrintf("</td></tr>\n");
if (!simpleSearch && groupSearch)
    searchTermsExist = TRUE;

// figure out how many metadata selects are visible.
int delSearchSelect = cartUsualInt(cart, "hgt.delRow", 0);   // 1-based row to delete
int addSearchSelect = cartUsualInt(cart, "hgt.addRow", 0);   // 1-based row to insert after

for(numMetadataSelects = 0;;)
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s%d", METADATA_NAME_PREFIX, numMetadataSelects + 1);
    char *str = cartOptionalString(cart, buf);
    if(isEmpty(str))
        break;
    else
        numMetadataSelects++;
    }

if(delSearchSelect)
    numMetadataSelects--;
if(addSearchSelect)
    numMetadataSelects++;

if(numMetadataSelects)
    {
    mdbVar = needMem(sizeof(char *) * numMetadataSelects);
    mdbVal = needMem(sizeof(char *) * numMetadataSelects);
    int i;
    for(i = 0; i < numMetadataSelects; i++)
        {
        char buf[256];
        int offset;   // used to handle additions/deletions
        if(addSearchSelect > 0 && i >= addSearchSelect)
            offset = 0; // do nothing to offset (i.e. copy data from previous row)
        else if(delSearchSelect > 0 && i + 1 >= delSearchSelect)
            offset = 2;
        else
            offset = 1;
        safef(buf, sizeof(buf), "%s%d", METADATA_NAME_PREFIX, i + offset);
        mdbVar[i] = cloneString(cartOptionalString(cart, buf));
        if(!simpleSearch)
            {
            safef(buf, sizeof(buf), "%s%d", METADATA_VALUE_PREFIX, i + offset);
            mdbVal[i] = cloneString(cartOptionalString(cart, buf));
            if(sameString(mdbVal[i], ANYLABEL))
                mdbVal[i] = NULL;
            if(!isEmpty(mdbVal[i]))
                numMetadataNonEmpty++;
            }
        }
    if(delSearchSelect > 0)
        {
        char buf[255];
        safef(buf, sizeof(buf), "%s%d", METADATA_NAME_PREFIX, numMetadataSelects + 1);
        cartRemove(cart, buf);
        safef(buf, sizeof(buf), "%s%d", METADATA_VALUE_PREFIX, numMetadataSelects + 1);
        cartRemove(cart, buf);
        }
    }
else
    {
    // create defaults
    numMetadataSelects = 2;
    mdbVar = needMem(sizeof(char *) * numMetadataSelects);
    mdbVal = needMem(sizeof(char *) * numMetadataSelects);
    mdbVar[0] = "cell";
    mdbVar[1] = "antibody";
    mdbVal[0] = ANYLABEL;
    mdbVal[1] = ANYLABEL;
    }

if(metaDbExists)
    {
    int i;
    char **mdbVars = NULL;
    char **mdbVarLabels = NULL;
    int count = metaDbVars(conn, &mdbVars,&mdbVarLabels);

    hPrintf("<tr><td colspan='%d' align='right' class='lineOnTop' style='height:20px; max-height:20px;'><em style='color:%s; width:200px;'>ENCODE terms</em></td></tr>", cols,COLOR_DARKGREY);
    for(i = 0; i < numMetadataSelects; i++)
        {
        char **terms = NULL, **labels = NULL;
        char buf[256];
        int len;

        hPrintf("<tr><td>\n");
        if(numMetadataSelects > 2 || i >= 2)
            {
            safef(buf, sizeof(buf), "return delSearchSelect(this, %d);", i + 1);
            hButtonWithOnClick(searchTracks, "-", "delete this row", buf);
            }
        else
            hPrintf("&nbsp;");
        hPrintf("</td><td>\n");
        safef(buf, sizeof(buf), "return addSearchSelect(this, %d);", i + 1);
        hButtonWithOnClick(searchTracks, "+", "add another row after this row", buf);

        hPrintf("</td><td>and&nbsp;</td><td colspan=3 nowrap>\n");
        safef(buf, sizeof(buf), "%s%i", METADATA_NAME_PREFIX, i + 1);
        cgiDropDownWithTextValsAndExtra(buf, mdbVarLabels, mdbVars,count,mdbVar[i],"class='mdbVar' onchange=findTracksMdbVarChanged(this)");
        hPrintf("</td><td nowrap style='max-width:600px;'>is\n");
        len = getTermArray(conn, &labels, &terms, mdbVar[i]);
        safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, i + 1);
        cgiMakeDropListFull(buf, labels, terms, len, mdbVal[i], "class='mdbVal' style='min-width:200px;' onchange='findTracksSearchButtonsEnable(true)'");
        if (!simpleSearch && mdbVal[i])
            searchTermsExist = TRUE;
        hPrintf("<span id='helpLink%d'>help</span></td>\n", i + 1);
        hPrintf("</tr>\n");
        }
    }

hPrintf("<tr><td colspan='%d'>\n", cols);
hPrintf("<input type='submit' name='%s' id='searchSubmit' value='Search' style='font-size:14px;'>\n", searchTracks);
hPrintf("<input type='button' name='clear' value='Clear' class='clear' style='font-size:14px;' onclick='findTracksClear();'>\n");
hPrintf("<input type='submit' name='submit' value='Cancel' class='cancel' style='font-size:14px;'>\n");
hPrintf("<a target='_blank' href='../goldenPath/help/trackSearch.html'>help</a></td></tr>\n");
hPrintf("</table>\n");
hPrintf("</div>\n</div>\n");
hPrintf("</form>\n");
hPrintf("</div"); // Restricts to max-width:1000px;

if(descSearch != NULL && !strlen(descSearch))
    descSearch = NULL;
if(groupSearch != NULL && sameString(groupSearch, ANYLABEL))
    groupSearch = NULL;

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
if (doSearch && simpleSearch && descWordCount <= 0)
    doSearch = FALSE;

enum sortBy sortBy = cartUsualInt(cart,SORT_BY_VAR,sbRelevance);
if(doSearch)
    {
    if(simpleSearch)
        {
        struct trixSearchResult *tsList;
        for(tsList = trixSearch(trix, descWordCount, descWords, TRUE); tsList != NULL; tsList = tsList->next)
            {
            struct track *track = (struct track *) hashFindVal(trackHash, tsList->itemId);
            if (track != NULL)  // It is expected that this is NULL (e.g. when the trix references trackDb tracks which have no tables)
                {
                refAdd(&tracks, track);
                tracksFound++;
                }
            }
        }
    else if(!isEmpty(nameSearch) || descSearch != NULL || groupSearch != NULL || numMetadataNonEmpty)
        {
        // First do the metaDb searches, which can be done quickly for all tracks with db queries.
        struct hash *matchingTracks = newHash(0);
        struct slName *el, *metaTracks = NULL;
        int i;

        for(i = 0; i < numMetadataSelects; i++)
            {
            if(!isEmpty(mdbVal[i]))
                {
                struct slName *tmp = mdbObjSearch(conn, mdbVar[i], mdbVal[i], "is", MDB_VAL_STD_TRUNCATION, TRUE, FALSE);
                if(metaTracks == NULL)
                    metaTracks = tmp;
                else
                    metaTracks = slNameIntersection(metaTracks, tmp);
                }
            }
        for (el = metaTracks; el != NULL; el = el->next)
            hashAddInt(matchingTracks, el->name, 1);
        for (group = groupList; group != NULL; group = group->next)
            {
            if(groupSearch == NULL || sameString(group->name, groupSearch))
                {
                if (group->trackList != NULL)
                    {
                    struct trackRef *tr;
                    for (tr = group->trackList; tr != NULL; tr = tr->next)
                        {
                        struct track *track = tr->track;
                        if((isEmpty(nameSearch) || isNameMatch(track, nameSearch, "contains")) &&
                           (isEmpty(descSearch) || isDescriptionMatch(track, descWords, descWordCount)) &&
                          (!numMetadataNonEmpty || hashLookup(matchingTracks, track->track) != NULL))
                            {
                            if (track != NULL)
                                {
                                tracksFound++;
                                refAdd(&tracks, track);
                                }
                            else
                                warn("found group track is NULL.");
                            }
                        if (track->subtracks != NULL)
                            {
                            struct track *subTrack;
                            for (subTrack = track->subtracks; subTrack != NULL; subTrack = subTrack->next)
                                {
                                if((isEmpty(nameSearch) || isNameMatch(subTrack, nameSearch, "contains")) &&
                                   (isEmpty(descSearch) || isDescriptionMatch(subTrack, descWords, descWordCount)) &&
                                   (!numMetadataNonEmpty || hashLookup(matchingTracks, subTrack->track) != NULL))
                                    {
                                    // XXXX to parent hash. - use tdb->parent instead.
                                    hashAdd(parents, subTrack->track, track);
                                    if (track != NULL)
                                        {
                                        tracksFound++;
                                        refAdd(&tracks, subTrack);
                                        }
                                    else
                                        warn("found subtrack is NULL.");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    if(tracksFound > 1)
        findTracksSort(&tracks,simpleSearch,sortBy);
    }

hPrintf("<div id='found' style='display:none;'>\n"); // This div allows the clear button to empty it
if(tracksFound < 1)
    {
    if(doSearch)
        hPrintf("<p>No tracks found</p>\n");
    }
else
    {
    struct hash *tdbHash = makeTrackHash(database, chromName);
    hPrintf("<form action='%s' name='SearchTracks' id='searchResultsForm' method='post'>\n\n", hgTracksName());
    cartSaveSession(cart);  // Creates hidden var of hgsid to avoid bad voodoo

    #ifdef FOUND_TRACKS_PAGING
    int startFrom = 0;
    #else///ifndef FOUND_TRACKS_PAGING
    if(tracksFound > MAX_FOUND_TRACKS)
        {
        hPrintf("<table class='redBox'><tr><td>Found %d tracks, but only the first %d are displayed.",tracksFound,MAX_FOUND_TRACKS);
        hPrintf("<BR><B><I>Please narrow search criteria to find fewer tracks.</I></B></div></td></tr></table>\n");
        }
    #endif///ndef FOUND_TRACKS_PAGING

    hPrintf("<table id='foundTracks'>\n");

    // Opening view in browser button and foundTracks count
    #define ENOUGH_FOUND_TRACKS 10
    if(tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("<tr><td colspan=3>\n");
        hPrintf("<INPUT TYPE=SUBMIT NAME='submit' VALUE='Return to Browser' class='viewBtn'>");
        hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<FONT class='selCbCount'></font>\n");

    #ifdef FOUND_TRACKS_PAGING
        startFrom = cartUsualInt(cart,FOUND_TRACKS_PAGING,0);
        if (startFrom > 0 && startFrom < tracksFound)
            {
            int countUp = 0;
            for(countUp=0; countUp < startFrom;countUp++)
                {
                if (slPopHead(&tracks) == NULL) // memory waste
                    break;
                }
            }
        hPrintf("</td><td align='right'>\n");
        findTracksPageLinks(tracksFound,startFrom);
        hPrintf("</td></tr>\n");
    #endif///ndef FOUND_TRACKS_PAGING
        }

    // Begin foundTracks table
    //hPrintf("<table id='foundTracks'><tr><td colspan='2'>\n");
    hPrintf("<tr><td colspan='2'>\n");
    hPrintf("</td><td align='right'>\n");
    #define PM_BUTTON "<IMG height=18 width=18 onclick=\"return findTracksCheckAllWithWait(%s);\" id='btn_%s' src='../images/%s' title='%s all found tracks'>"
    hPrintf("</td></tr><tr bgcolor='#%s'><td>",HG_COL_HEADER);
    hPrintf(PM_BUTTON,"true",  "plus_all",   "add_sm.gif",  "Select");
    hPrintf(PM_BUTTON,"false","minus_all","remove_sm.gif","Unselect");
    hPrintf("</td><td><b>Visibility</b></td><td colspan=2>&nbsp;&nbsp;<b>Track Name</b>\n");

    // Sort options?
    if(tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("<span style='float:right;'>Sort:");
        cgiMakeOnClickRadioButton(SORT_BY_VAR, "0", (sortBy == sbRelevance),"onchange=\"findTracksSortNow(this);\"");
        hPrintf("by Relevance");
        cgiMakeOnClickRadioButton(SORT_BY_VAR, "1", (sortBy == sbAbc),      "onchange=\"findTracksSortNow(this);\"");
        hPrintf("Alphabetically");
        cgiMakeOnClickRadioButton(SORT_BY_VAR, "2",(sortBy == sbHierarchy), "onchange=\"findTracksSortNow(this);\"");
        hPrintf("by Hierarchy&nbsp;&nbsp;</span>\n");
        }
    hPrintf("</td></tr>\n");

    // Set up json for js functionality
    struct dyString *jsonTdbVars = NULL;

    int trackCount=0;
    boolean containerTrackCount = 0;
    struct slRef *ptr;
    while((ptr = slPopHead(&tracks)))
        {
        if(++trackCount > MAX_FOUND_TRACKS)
            break;

        struct track *track = (struct track *) ptr->val;
        jsonTdbSettingsBuild(&jsonTdbVars, track);

        if (tdbIsFolder(track->tdb)) // supertrack
            hPrintf("<tr bgcolor='%s' valign='top' class='found'>\n","#EED5B7");//"#DEB887");//"#E6B426");//#FCECC0//COLOR_LTGREY);//COLOR_LTGREEN);//COLOR_TRACKLIST_LEVEL1);
        else if (tdbIsContainer(track->tdb))
            hPrintf("<tr bgcolor='%s' valign='top' class='found'>\n",COLOR_TRACKLIST_LEVEL3);
        else
            hPrintf("<tr bgcolor='%s' valign='top' class='found'>\n",COLOR_TRACKLIST_LEVEL2);

        hPrintf("<td align='center'>\n");

        // Determine visibility and checked state
        track->visibility = tdbVisLimitedByAncestry(cart, track->tdb, FALSE);
        boolean checked = ( track->visibility != tvHide );
        if(tdbIsContainerChild(track->tdb))
            {
            //track->visibility = limitedVisFromComposite(track);
            checked = fourStateVisible(subtrackFourStateChecked(track->tdb,cart)); // Don't need all 4 states here.  Visible=checked&&enabled
            checked = (checked && ( track->visibility != tvHide )); // Checked is only if subtrack level vis is also set!
            }

        // Setup the check box
        #define CB_HIDDEN_VAR "<INPUT TYPE=HIDDEN disabled=true NAME='%s_sel' VALUE='%s'>"
        if (tdbIsContainerChild(track->tdb) || tdbIsFolderContent(track->tdb))  // subtracks and folder children get "_sel" var.  ("_sel" var is temporary on folder children)
            hPrintf(CB_HIDDEN_VAR,track->track,checked?"1":CART_VAR_EMPTY);
        #define CB_SEEN "<INPUT TYPE=CHECKBOX id='%s_sel_id' VALUE='on' class='selCb' onclick='findTracksClickedOne(this,true);'%s>"
        hPrintf(CB_SEEN,track->track,(checked?" CHECKED":""));
        hPrintf("</td><td>\n");

        // Setup the visibility drop down
        #define VIS_HIDDEN_VAR "<INPUT TYPE=HIDDEN disabled=true NAME='%s' VALUE='%s'>"
        hPrintf(VIS_HIDDEN_VAR,track->track,CART_VAR_EMPTY); // All tracks get vis hidden var
        char extra[512];
        if (tdbIsFolder(track->tdb))
            {
            safef(extra,sizeof(extra),"id='%s_id' onchange='findTracksChangeVis(this)'",track->track);
            hideShowDropDownWithClassAndExtra(track->track, (track->visibility != tvHide), "normalText visDD",extra);

            }
        else
            {
            safef(extra,sizeof(extra),"id='%s_id' onchange='findTracksChangeVis(this)'",track->track);
            hTvDropDownClassWithJavascript(NULL, track->visibility,track->canPack,"normalText seenVis",extra);
            }

        // If this is a container track, allow configuring...
        if (tdbIsContainer(track->tdb) || tdbIsFolder(track->tdb))
            {
            containerTrackCount++;
            //hPrintf("&nbsp;<a href='hgTrackUi?db=%s&g=%s&hgt_searchTracks=1&hgt_searchTracks=Search' title='Configure this container track...'>*</a>&nbsp;",database,track->track);
            hPrintf("&nbsp;<a href='hgTrackUi?db=%s&g=%s&hgt_searchTracks=1&hgt_searchTracks=Search' title='Configure this container track...'><IMG SRC='../images/folderWrench.png'></a>&nbsp;",database,track->track);
            }
        hPrintf("</td>\n");

        // shortLabel has description popup and longLabel has "..." metadata
        hPrintf("<td><a target='_top' onclick=\"hgTrackUiPopUp('%s',true); return false;\" href='%s' title='Display track details'>%s</a></td>\n", track->track, trackUrl(track->track, NULL), track->shortLabel);
        hPrintf("<td>%s", track->longLabel);
        compositeMetadataToggle(database, track->tdb, "...", TRUE, FALSE, tdbHash);
        hPrintf("</td></tr>\n");
        }
    //hPrintf("</table>\n");

    // Closing view in browser button and foundTracks count
    hPrintf("<tr><td colspan=3>");
    hPrintf("<INPUT TYPE=SUBMIT NAME='submit' VALUE='Return to Browser' class='viewBtn'>");
    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<FONT class='selCbCount'></font>");
    #ifdef FOUND_TRACKS_PAGING
    if(tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("</td><td align='right'>\n");
        findTracksPageLinks(tracksFound,startFrom);
        hPrintf("</td></tr>\n");
        }
    #endif///ndef FOUND_TRACKS_PAGING
    hPrintf("</table>\n");

    if(containerTrackCount > 0)
        hPrintf("<BR><IMG SRC='../images/folderWrench.png'>&nbsp;Tracks so marked are containers which group related data tracks.  Containers may need additional configuration (by clicking on the <IMG SRC='../images/folderWrench.png'> icon) before they can be viewed in the browser.<BR>\n");
        //hPrintf("* Tracks so marked are containers which group related data tracks.  These may not be visible unless further configuration is done.  Click on the * to configure these.<BR><BR>\n");
    hPrintf("\n</form>\n");

    // be done with json
    hWrites(jsonTdbSettingsUse(&jsonTdbVars));
    }

if(!doSearch)
    {
    hPrintf("<p><b>Recently Done</b><ul>\n"
        "<li>Can now page through found tracks 100 at a time.</li>"
        "<li>Added <IMG SRC='../images/folderWrench.png'> icon for contqainers with a configuration link.  Is this okay?</li>"
        "<li>SuperTracks can now be found.</li>"
        "<li>Configuration of superTrack children's vis should result in proper superTrack reshaping. (This is really an hgTrackUi feature.)</li>"
        "<li>Added sort toggle: Relevance, Alphabetically or by Hierarchy.</li>"
        "<li>Composite/view visibilites in hgTrackUi get reshaped to reflect found/selected subtracks.  (In demo1: only default state composites; demo2: all composites.)</li>"
        "<li>Non-data 'container' tracks (composites and supertracks) have '*' to mark them, and can be configured before displaying.  Better suggestions?</li>"
        "</ul></p>"
        "<p><b>Suggested improvments:</b><ul>\n"
        "<li>The metadata values will not be white-listed, but it would be nice to have more descriptive text for them.  A short label added to cv.ra?</li>"
        "<li>Look and feel of found track list (here) and composite subtrack list (hgTrackUi) should converge.  Jim suggests look and feel of hgTracks 'Configure Tracks...' list instead.</li>"
        "<li>Drop-down list of terms (cells, antibodies, etc.) should be multi-select with checkBoxes as seen in filterComposites. Perhaps saved for v2.0.</li>"
        "</ul></p>\n");
    }
hPrintf("</div"); // This div allows the clear button to empty it
hFreeConn(&conn);
webEndSectionTables();
}
