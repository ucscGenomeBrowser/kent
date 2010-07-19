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
#include "trix.h"
#include "jsHelper.h"

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

static int gCmpTrack(const void *va, const void *vb)
/* Compare tracks based on longLabel. */
{
const struct slRef *aa = *((struct slRef **)va);
const struct slRef *bb = *((struct slRef **)vb);
const struct track *a = ((struct track *) aa->val);
const struct track *b = ((struct track *) bb->val);
return strcmp(a->longLabel, b->longLabel);
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

static int getTermArray(struct sqlConnection *conn, char ***terms, char *type)
// Pull out all term fields from ra entries with given type
// Returns count of items found and items via the terms argument.
{
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];
struct slName *termList = NULL;
int i, count = 0;
char **retVal;

safef(query, sizeof(query), "select distinct val from metaDb where var = '%s'", type);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    slNameAddHead(&termList, row[0]);
    count++;
    }
sqlFreeResult(&sr);
slSort(&termList, slNameCmpCase);
count++; // make room for "Any"
AllocArray(retVal, count);
retVal[0] = cloneString(ANYLABEL);
for(i = 1; termList != NULL;termList = termList->next, i++)
    {
    retVal[i] = cloneString(termList->name);
    }
*terms = retVal;
return count;
}

static struct slName *metaDbSearch(struct sqlConnection *conn, char *name, char *val, char *op)
// Search the assembly's metaDb table for var; If name == NULL, we search every metadata field.
// Search is via mysql, so it's case-insensitive.
{
struct slName *retVal = NULL;
char query[256];
struct sqlResult *sr = NULL;
char **row = NULL;

if(sameString(op, "contains"))
    if(name == NULL)
        safef(query, sizeof(query), "select obj from metaDb where val like  '%%%s%%'", val);
    else
        safef(query, sizeof(query), "select obj from metaDb where var = '%s' and val like  '%%%s%%'", name, val);
else
    if(name == NULL)
        safef(query, sizeof(query), "select distinct obj from metaDb where val = '%s'", val);
    else
        safef(query, sizeof(query), "select obj from metaDb where var = '%s' and val = '%s'", name, val);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    slNameAddHead(&retVal, row[0]);
    }
sqlFreeResult(&sr);
return retVal;
}

static int metaDbVars(struct sqlConnection *conn, char *** metaValues)
// Search the assemblies metaDb table; If name == NULL, we search every metadata field.
{
char query[256];
struct sqlResult *sr = NULL;
char **row = NULL;
int i;
struct slName *el, *varList = NULL;
char **retVal;

safef(query, sizeof(query), "select distinct var from metaDb");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    slNameAddHead(&varList, row[0]);
sqlFreeResult(&sr);
retVal = needMem(sizeof(char *) * slCount(varList));
slNameSort(&varList);
for (el = varList, i = 0; el != NULL; el = el->next, i++)
    retVal[i] = el->name;
*metaValues = retVal;
return i;
}

void doSearchTracks(struct group *groupList)
{
struct group *group;
char *ops[] = {"is", "contains"};
char *op_labels[] = {"is", "contains"};
char *groups[128];
char *labels[128];
int numGroups = 1;
groups[0] = ANYLABEL;
labels[0] = ANYLABEL;
char *currentTab = cartUsualString(cart, "hgt.currentSearchTab", "simpleTab");
char *nameSearch = cartOptionalString(cart, "hgt.nameSearch");
char *nameOp = cartOptionalString(cart, "hgt.nameOp");
char *descSearch;
char *groupSearch = cartOptionalString(cart, "hgt.groupSearch");
boolean doSearch = sameString(cartOptionalString(cart, searchTracks), "Search");
struct sqlConnection *conn = hAllocConn(database);
boolean metaDbExists = sqlTableExists(conn, "metaDb");
struct slRef *tracks = NULL;
int numMetadataSelects, tracksFound = 0;
int numMetadataNonEmpty = 0;
char **metadataName;
char **metadataValue;
struct hash *parents = newHash(4);
boolean simpleSearch;
struct trix *trix;
char trixFile[HDB_MAX_PATH_STRING];
char **descWords = NULL;
int descWordCount = 0;

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

getSearchTrixFile(database, trixFile, sizeof(trixFile));
trix = trixOpen(trixFile);
getTrackList(&groupList, -2);
slSort(&groupList, gCmpGroup);
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->trackList != NULL)
        {
        groups[numGroups] = cloneString(group->name);
        labels[numGroups] = cloneString(group->label);
        numGroups++;
        if (numGroups >= ArraySize(groups))
            internalErr();
        }
    }

hPrintf("<form action='%s' name='SearchTracks' id='searchTracks' method='get'>\n\n", hgTracksName());

webStartWrapperDetailedNoArgs(cart, database, "", "Track Search", FALSE, FALSE, FALSE, FALSE);

hPrintf("<input type='hidden' name='db' value='%s'>\n", database);
hPrintf("<input type='hidden' name='hgt.currentSearchTab' id='currentSearchTab' value='%s'>\n", currentTab);

hPrintf("<div id='tabs'>\n"
        "<ul>\n"
        "<li><a href='#simpleTab'><span>Search</span></a></li>\n"
        "<li><a href='#advancedTab'><span>Advanced Search</span></a></li>\n"
        "</ul>\n"
        "<div id='simpleTab'>\n");

hPrintf("<input type='text' name='hgt.simpleSearch' id='simpleSearch' value='%s' size='80'>\n", descSearch == NULL ? "" : descSearch);

hPrintf("</div>\n"
        "<div id='advancedTab'>\n"
        "<table>\n");

hPrintf("<tr><td></td><td></td><td><b>Description:</b></td><td>contains</td>\n");
hPrintf("<td><input type='text' name='hgt.descSearch' id='descSearch' value='%s' size='80'></td></tr>\n", descSearch == NULL ? "" : descSearch);

hPrintf("<tr><td></td><td>and</td><td><b>Track Name:</b></td><td>\n");
cgiMakeDropListFull("hgt.nameOp", op_labels, ops, ArraySize(ops), nameOp == NULL ? "contains" : nameOp, NULL);
hPrintf("</td>\n<td><input type='text' name='hgt.nameSearch' id='nameSearch' value='%s'></td></tr>\n", nameSearch == NULL ? "" : nameSearch);

hPrintf("<tr><td></td><td>and</td>\n");
hPrintf("<td><b>Group</b></td><td>is</td>\n<td>\n");
cgiMakeDropListFull("hgt.groupSearch", labels, groups, numGroups, groupSearch, NULL);
hPrintf("</td></tr>\n");

// figure out how many metadata selects are visible.

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

if(numMetadataSelects)
    {
    metadataName = needMem(sizeof(char *) * numMetadataSelects);
    metadataValue = needMem(sizeof(char *) * numMetadataSelects);
    int i;
    for(i = 0; i < numMetadataSelects; i++)
        {
        char buf[256];
        safef(buf, sizeof(buf), "%s%d", METADATA_NAME_PREFIX, i + 1);
        metadataName[i] = cartOptionalString(cart, buf);
        if(!simpleSearch)
            {
            safef(buf, sizeof(buf), "%s%d", METADATA_VALUE_PREFIX, i + 1);
            metadataValue[i] = cartOptionalString(cart, buf);
            if(sameString(metadataValue[i], ANYLABEL))
                metadataValue[i] = NULL;
            if(!isEmpty(metadataValue[i]))
                numMetadataNonEmpty++;
            }
        }
    }
else
    {
    // create defaults
    numMetadataSelects = 2;
    metadataName = needMem(sizeof(char *) * numMetadataSelects);
    metadataValue = needMem(sizeof(char *) * numMetadataSelects);
    metadataName[0] = "cell";
    metadataName[1] = "antibody";
    metadataValue[0] = ANYLABEL;
    metadataValue[1] = ANYLABEL;
    }

if(metaDbExists)
    {
    int i;
    char **metaValues = NULL;
    int count = metaDbVars(conn, &metaValues);

    for(i = 0; i < numMetadataSelects; i++)
        {
        char **terms;
        char buf[256];
        int len;

        hPrintf("<tr><td>\n");

        if(i == 0)
            hPrintf("&nbsp;\n");
        else
            hButtonWithOnClick("hgt.ignoreme", "+", "add a select", "alert('add a select is not yet implemented'); return false;");

        hPrintf("</td><td>and</td>\n");
        hPrintf("</td><td>\n");
        safef(buf, sizeof(buf), "%s%i", METADATA_NAME_PREFIX, i + 1);
        cgiMakeDropListClassWithStyleAndJavascript(buf, metaValues, count, metadataName[i], 
                                                   NULL, NULL, "onchange=metadataSelectChanged(this)");
        hPrintf("</td><td>is</td>\n<td>\n");
        len = getTermArray(conn, &terms, metadataName[i]);
        safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, i + 1);
        cgiMakeDropListFull(buf, terms, terms, len, metadataValue[i], NULL);
        hPrintf("</td></tr>\n");
        }
    }

hPrintf("</table>\n");

hPrintf("</div>\n</div>\n");

hPrintf("<input type='submit' name='%s' id='searchSubmit' value='Search'>\n", searchTracks);
hPrintf("<input type='submit' name='submit' value='Cancel'>\n");
hPrintf("</form>\n");

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
        val = nextWord(&tmp);
        }
    descWordCount = slCount(descList);
    descWords = needMem(sizeof(char *) * descWordCount);
    for(i = 0, el = descList; el != NULL; i++, el = el->next)
        descWords[i] = strLower(el->name);

    }

if(doSearch)
    {
    if(simpleSearch)
        {
        struct trixSearchResult *tsList;
        struct hash *trackHash = newHash(0);

        // Create a hash of tracks, so we can map the track name into a track struct.
        for (group = groupList; group != NULL; group = group->next)
            {
            struct trackRef *tr;
            for (tr = group->trackList; tr != NULL; tr = tr->next)
                {
                struct track *track = tr->track;
                hashAdd(trackHash, track->track, track);
                struct track *subTrack = track->subtracks;
                for (subTrack = track->subtracks; subTrack != NULL; subTrack = subTrack->next)
                    hashAdd(trackHash, subTrack->track, subTrack);
                }
            }
        for(tsList = trixSearch(trix, descWordCount, descWords, TRUE); tsList != NULL; tsList = tsList->next)
            {
            struct track *track = (struct track *) hashFindVal(trackHash, tsList->itemId);
            refAdd(&tracks, track);
            tracksFound++;
            }
        slReverse(&tracks);
        }
    else if(!isEmpty(nameSearch) || descSearch != NULL || groupSearch != NULL || numMetadataNonEmpty)
        {
        // First do the metaDb searches, which can be done quickly for all tracks with db queries.
        struct hash *matchingTracks = newHash(0);
        struct hash *trackMetadata = newHash(0);
        struct slName *el, *metaTracks = NULL;
        int i;

        for(i = 0; i < numMetadataSelects; i++)
            {
            if(!isEmpty(metadataValue[i]))
                {
                struct slName *tmp = metaDbSearch(conn, metadataName[i], metadataValue[i], "is");
                if(metaTracks == NULL)
                    metaTracks = tmp;
                else
                    metaTracks = slNameIntersection(metaTracks, tmp);
                }
            }
        for (el = metaTracks; el != NULL; el = el->next)
            hashAddInt(matchingTracks, el->name, 1);

        if(metaDbExists && !isEmpty(descSearch))
            {
            // Load all metadata words for each track to facilitate metadata search.
            char query[256];
            struct sqlResult *sr = NULL;
            char **row;
            safef(query, sizeof(query), "select obj, val from metaDb");
            sr = sqlGetResult(conn, query);
            while ((row = sqlNextRow(sr)) != NULL)
                {
                char *str = cloneString(row[1]);
                hashAdd(trackMetadata, row[0], str);
                }
            sqlFreeResult(&sr);
            }

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
                        if((isEmpty(nameSearch) || isNameMatch(track, nameSearch, nameOp)) && 
                           (isEmpty(descSearch) || isDescriptionMatch(track, descWords, descWordCount)) &&
                           (!numMetadataNonEmpty || hashLookup(matchingTracks, track->track) != NULL))
                            {
                            tracksFound++;
                            refAdd(&tracks, track);
                            }
                        if (track->subtracks != NULL)
                            {
                            struct track *subTrack;
                            for (subTrack = track->subtracks; subTrack != NULL; subTrack = subTrack->next)
                                {
                                if((isEmpty(nameSearch) || isNameMatch(subTrack, nameSearch, nameOp)) &&
                                   (isEmpty(descSearch) || isDescriptionMatch(subTrack, descWords, descWordCount)) &&
                                   (!numMetadataNonEmpty || hashLookup(matchingTracks, subTrack->track) != NULL))
                                    {
                                    // XXXX to parent hash. - use tdb->parent instead.
                                    hashAdd(parents, subTrack->track, track);
                                    tracksFound++;
                                    refAdd(&tracks, subTrack);
                                    }
                                }                        
                            }
                        }
                    }
                }
            }
        slSort(&tracks, gCmpTrack);
        }
    }

if(tracksFound)
    {
    hPrintf("<h3><b>%d tracks found:</b></h3>\n", tracksFound);
    hPrintf("<form action='%s' name='SearchTracks' id='searchResultsForm' method='post'>\n\n", hgTracksName());
    hPrintf("<table><tr><td colspan='2'>\n");
    hButton("submit", "View in Browser");
    hPrintf("</td><td align='right'>\n");
    hButtonWithOnClick("hgt.ignoreme", "Select All", "show all found tracks", "changeSearchVisibilityPopups('full'); return false;");
    hButtonWithOnClick("hgt.ignoreme", "Unselect All", "show all found tracks", "changeSearchVisibilityPopups('hide'); return false;");
    hPrintf("</td></tr><tr bgcolor='#666666'><td><br /></td><td><b>Name</b></td><td><b>Description</b></td></tr>\n");
    struct slRef *ptr;
    while((ptr = slPopHead(&tracks)))
        {
        struct track *track = (struct track *) ptr->val;
        // trackDbOutput(track->tdb, stderr, ',', '\n');
        hPrintf("<tr bgcolor='#EEEEEE'>\n");
        hPrintf("<td>\n");
        if (tdbIsSuper(track->tdb))
            {
            superTrackDropDown(cart, track->tdb,
                               superTrackHasVisibleMembers(track->tdb));
            }
        else
            {
            hTvDropDownClassVisOnly(track->track, track->visibility,
                                    track->canPack, (track->visibility == tvHide) ? 
                                    "hiddenText" : "normalText", 
                                    trackDbSetting(track->tdb, "onlyVisibility"));
            }
        hPrintf("</td>\n");
        hPrintf("<td>%s", track->shortLabel);
        compositeMetadataToggle(database, track->tdb, "...", TRUE, FALSE);
        hPrintf("</td>\n");
        hPrintf("<td><a target='_top' href='%s'>%s</a></td>\n", trackUrl(track->track, NULL), track->longLabel);
        hPrintf("</tr>\n");
        }
    hPrintf("</table>\n");
    hButton("submit", "View in Browser");
    hPrintf("\n</form>\n");
    } 
else
    {
    if(doSearch)
        hPrintf("<p>No tracks found</p>\n");
    }

hPrintf("<p><b>Known Problems</b></p><ul><li>Menu bar up top doesn't work (clicks are ignored)</li>\n"
        "<li>subtracks often come up with the wrong visibility (but saving visibility for subtracks does work</li>"
        "</ul>\n");

webEndSectionTables();
}
