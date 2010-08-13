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

webStartWrapperDetailedNoArgs(cart, database, "", "Track Search", FALSE, FALSE, FALSE, FALSE);

hPrintf("<form action='%s' name='SearchTracks' id='searchTracks' method='get'>\n\n", hgTracksName());

hPrintf("<input type='hidden' name='db' value='%s'>\n", database);
hPrintf("<input type='hidden' name='hgt.currentSearchTab' id='currentSearchTab' value='%s'>\n", currentTab);
hPrintf("<input type='hidden' name='hgt.delRow' value=''>\n");
hPrintf("<input type='hidden' name='hgt.addRow' value=''>\n");

hPrintf("<div id='tabs' style='display:none;'>\n"
        "<ul>\n"
        "<li><a href='#simpleTab'><span>Search</span></a></li>\n"
        "<li><a href='#advancedTab'><span>Advanced Search</span></a></li>\n"
        "</ul>\n"
        "<div id='simpleTab'>\n");

hPrintf("<input type='text' name='hgt.simpleSearch' id='simpleSearch' value='%s' size='80'>\n", descSearch == NULL ? "" : descSearch);

hPrintf("<br><input type='submit' name='%s' id='searchSubmit' value='Search' style='font-size:14px;'>\n", searchTracks);
hPrintf("<input type='submit' name='submit' value='Cancel' style='font-size:14px;'>\n");
hPrintf("</div>\n"
        "<div id='advancedTab'>\n"
        "<table>\n");

hPrintf("<tr><td></td><td></td><td></td><td><b>Description:</b></td><td align='right'>contains</td>\n");
hPrintf("<td><input type='text' name='hgt.descSearch' id='descSearch' value='%s' size='80'></td></tr>\n", descSearch == NULL ? "" : descSearch);

hPrintf("<tr><td></td><td></td><td>and</td><td><b>Track Name:</b></td><td align='right'>\n");
cgiMakeDropListFull("hgt.nameOp", op_labels, ops, ArraySize(ops), nameOp == NULL ? "contains" : nameOp, NULL);
hPrintf("</td>\n<td><input type='text' name='hgt.nameSearch' id='nameSearch' value='%s'></td></tr>\n", nameSearch == NULL ? "" : nameSearch);

hPrintf("<tr><td></td><td></td><td>and</td>\n");
hPrintf("<td><b>Group</b></td><td align='right'>is</td>\n<td>\n");
cgiMakeDropListFull("hgt.groupSearch", labels, groups, numGroups, groupSearch, NULL);
hPrintf("</td></tr>\n");

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
    metadataName = needMem(sizeof(char *) * numMetadataSelects);
    metadataValue = needMem(sizeof(char *) * numMetadataSelects);
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
        metadataName[i] = cartOptionalString(cart, buf);
        if(!simpleSearch)
            {
            safef(buf, sizeof(buf), "%s%d", METADATA_VALUE_PREFIX, i + offset);
            metadataValue[i] = cartOptionalString(cart, buf);
            if(sameString(metadataValue[i], ANYLABEL))
                metadataValue[i] = NULL;
            if(!isEmpty(metadataValue[i]))
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

        hPrintf("</td><td>and</td>\n");
        hPrintf("</td><td colspan=2 nowrap>\n");
        safef(buf, sizeof(buf), "%s%i", METADATA_NAME_PREFIX, i + 1);
        cgiMakeDropListClassWithStyleAndJavascript(buf, metaValues, count, metadataName[i],
                                                   NULL, NULL, "onchange=metadataSelectChanged(this)");
        hPrintf("is</td>\n<td>\n");
        len = getTermArray(conn, &terms, metadataName[i]);
        safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, i + 1);
        cgiMakeDropListFull(buf, terms, terms, len, metadataValue[i], NULL);
        hPrintf("</td></tr>\n");
        }
    }

hPrintf("</table>\n");

hPrintf("<input type='submit' name='%s' id='searchSubmit' value='Search' style='font-size:14px;'>\n", searchTracks);
hPrintf("<input type='submit' name='submit' value='Cancel' style='font-size:14px;'>\n");
hPrintf("</div>\n</div>\n");

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

if(tracksFound < 1)
    {
    if(doSearch)
        hPrintf("<p>No tracks found</p>\n");
    }
else
    {
    struct hash *tdbHash = makeTrackHash(database, chromName);
    hPrintf("<form action='%s' name='SearchTracks' id='searchResultsForm' method='post'>\n\n", hgTracksName());
    #define MAX_FOUND_TRACKS 100
    if(tracksFound > MAX_FOUND_TRACKS)
        {
        hPrintf("<table class='redBox'><tr><td>Found %d tracks, but only the first %d are displayed.",tracksFound,MAX_FOUND_TRACKS);
        hPrintf("<BR><B><I>Please narrow search criteria to find fewer tracks.</I></B></div></td></tr></table>\n");
        }

    #define ENOUGH_FOUND_TRACKS 10
    if(tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("<INPUT TYPE=SUBMIT NAME='submit' VALUE='View in Browser' class='viewBtn'>");
        hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<FONT class='selCbCount'></font>");
        }
    hPrintf("<table><tr><td colspan='2'>\n");
    hPrintf("</td><td align='right'>\n");
    #define PM_BUTTON "<IMG height=18 width=18 onclick=\"return findTracksCheckAllWithWait(%s);\" id='btn_%s' src='../images/%s' title='%s all found tracks'>"
    hPrintf("</td></tr><tr bgcolor='%s'><td>",HG_COL_HEADER);
    printf(PM_BUTTON,"true",  "plus_all",   "add_sm.gif",  "Select");
    printf(PM_BUTTON,"false","minus_all","remove_sm.gif","Unselect");
    hPrintf("</td><td><b>Visibility</b></td><td><b>Name</b></td><td><b>Description</b></td></tr>\n");
    // FIXME: I believe that only data tracks should appear in found tracks.  If composite is found, show its children instead

    int trackCount=0;
    struct slRef *ptr;
    while((ptr = slPopHead(&tracks)))
        {
        if(++trackCount > MAX_FOUND_TRACKS)
            break;

        struct track *track = (struct track *) ptr->val;
        // trackDbOutput(track->tdb, stderr, ',', '\n');
        hPrintf("<tr bgcolor='%s' valign='top' class='found%s%s'>\n",COLOR_BG_ALTDEFAULT,
                (tdbIsCompositeChild(track->tdb)?" subtrack":""),
                (track->canPack?" canPack":""));

        // NOTE: We have now changed the paradigm for subtrack vis:
        // There must be a {trackName}_sel to have subtrack level vis override.
        // This means rightClick must make a {trackName}_sel, which is needed for hgTrackUi conformity anyway
        // AND hgTracks.c now relies upon "_sel" even with subtrack-level vis.

        // NOTE: Difficulties with "sel" and "vis" controls:
        // 1) subtracks need both "sel" and "vis", but non-subtracks need only "vis"
        // 2) Submit of form instead of ajax is nice (because it allows cancelling changes), but do not want to set any vars, unless specifically changed on form
        // 3) When unchecked, need to delete vars instead of set them
        // Solution to "sel", "vis" difficulties
        // 1) findTracks remains a submit but:
        // 2) 'sel' and 'vis' input are not named (won't be submitted)
        // 3) hidden disabled and named 'sel' and 'vis' vars exist
        // 4a) check subtrack: enable hidden 'sel' and 'vis' track, set to 'on' and pack/full
        // 4b) check non-track: enable hidden 'vis', set to pack/full
        // 5a) uncheck subtrack: enable hidden 'sel' and 'vis' track, set to '[]' and '[]'
        // 5b) uncheck non-track: enable hidden 'vis', set to '[]'
        // 6) Change vis: enable hidden 'vis', set to non-hidden vis

        hPrintf("<td align='center' valign='center'>\n");
        char name[256];
        safef(name,sizeof(name),"%s_sel",track->track);
        boolean checked = FALSE;
        if(tdbIsCompositeChild(track->tdb))
            {
            checked = fourStateVisible(subtrackFourStateChecked(track->tdb,cart)); // Don't need all 4 states here.  Visible=checked&&enabled
            track->visibility = limitedVisFromComposite(track);
            checked = (checked && ( track->visibility != tvHide )); // Checked is only if subtrack level vis is also set!
            // Only subtracks get "_sel" var
            #define CB_HIDDEN_VAR "<INPUT TYPE=HIDDEN disabled=true NAME='%s_sel' VALUE='%s'>"
            hPrintf(CB_HIDDEN_VAR,track->track,CART_VAR_EMPTY);
            }
        else
            checked = ( track->visibility != tvHide );

        #define CB_SEEN "<INPUT TYPE=CHECKBOX id='%s_sel' VALUE='on' class='selCb' onchange='findTracksClickedOne(this,true);'%s>"
        hPrintf(CB_SEEN,track->track,(checked?" CHECKED":""));

        hPrintf("</td><td>\n");

        #define VIS_HIDDEN_VAR "<INPUT TYPE=HIDDEN disabled=true NAME='%s' VALUE='%s'>"
        hPrintf(VIS_HIDDEN_VAR,track->track,CART_VAR_EMPTY); // All tracks get vis hidden var
        if (tdbIsSuper(track->tdb))
            {
            // FIXME: Replace this with select box WITHOUT NAME but with id
            // HOWEVER, I haven't seen a single supertrack in found tracks so I think they are excluded and this is dead code
            warn("superTrack: %s '%s' doesn't work yet.",track->track,track->longLabel);
            superTrackDropDown(cart, track->tdb, superTrackHasVisibleMembers(track->tdb));
            }
        else
            {
            char extra[512];
            safef(extra,sizeof(extra),"id='%s' onchange='findTracksChangeVis(this)'",track->track);
            hTvDropDownClassWithJavascript(NULL, track->visibility,track->canPack,"normalText seenVis",extra);
            }
        hPrintf("</td>\n");
        hPrintf("<td><a target='_top' href='%s' title='Configure track...'>%s</a></td>\n", trackUrl(track->track, NULL), track->shortLabel);
        hPrintf("<td>%s", track->longLabel);
        compositeMetadataToggle(database, track->tdb, "...", TRUE, FALSE, tdbHash);
        hPrintf("</td></tr>\n");
        }
    hPrintf("</table>\n");
    hPrintf("<INPUT TYPE=SUBMIT NAME='submit' VALUE='View in Browser' class='viewBtn'>");
    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<FONT class='selCbCount'></font>");
    hPrintf("<script type='text/javascript'>findTracksNormalizeFound();</script>\n");
    hPrintf("\n</form>\n");
    }

hPrintf("<p><b>Recently Done</b><ul>\n"
        "<li>Deleting/Adding selection criteria with [-][+] buttons in 'Advanced Search'.</li>"
        "<li>Found track list shows only the first 100 tracks with warning to narrow search.\"</li>\n"
        "<li>Clicks in menu bar up top now work correctly.</li>\n"
        "<li>Found tracks are selected by checkboxes.</li>"
        "<li>Checkbox state should be persistent.  Subtracks selected will be checked in their composite.</li>"
        "<li>Full descriptions of metadata items are indexed in simple search index (e.g. cell descriptions).</li>"
        "</ul></p>"
        "<p><b>Known Problems</b><ul>"
        "<li>Search results should be cleared when switching between simple and advanced tabs (do we agree on that?)</li>"
        "<li>Subtracks often come up with the wrong visibility (but saving visibility for subtracks does work)."
        "<BR>QUESTION: Upon selecting with checkbox, what is correct default vis for any track?  Perhaps 'full' for wigs, 'pack' for others?</li>"
        "<li>Too large of found track list results in slow scripts.</li>"
        "<li>Strangeness seen in finding tracks: 'ENCODE' in description combined with antibody selection results in no tracks found."
        "<BR>QUESTION: Is that a bug? REM that description search is NOT inherited. (* Suggestions below.)</li>"
        "</ul></p>"
        "<p><b>Suggested improvments:</b><ul>\n"
        "<li>Only data tracks should be seen in found track list.  Found superTracks/composites should be converted into their children.</li>"
        "<li>Configuring found tracks should be by pop-up dialog box.</li>"
        "<li>Look and feel of found track list (here) and composite subtrack list (hgTrackUi) should converge.</li>"
        "<li>Drop-down list of terms (cells, antibodies, etc.) should be multi-select with checkBoxes as seen in filterComposites.</li>"
        "<li>* 'Descrtiption' and 'Track Name' should be merged into a single search term.</li>"
        "<li>* 'Descrtiption' should be inherited by subtracks?</li>"
        "</ul></p>\n");

webEndSectionTables();
}
