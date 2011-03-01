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

#define ANYLABEL                 "Any"
#define FILE_SEARCH              "hgfs_Search"
#define FILE_SEARCH_FORM         "fileSearch"
#define FILE_SEARCH_CURRENT_TAB  "fsCurTab"
#define FILE_SEARCH_ON_FILETYPE  "fsFileType"

// These are common with trackSearch.  Should they be?
#define METADATA_NAME_PREFIX     "hgt_mdbVar"
#define METADATA_VALUE_PREFIX    "hgt_mdbVal"
#define TRACK_SEARCH_SIMPLE      "tsSimple"
#define TRACK_SEARCH_ON_NAME     "tsName"
#define TRACK_SEARCH_ON_GROUP    "tsGroup"
#define TRACK_SEARCH_ON_DESCR    "tsDescr"
#define TRACK_SEARCH_SORT        "tsSort"

//#define USE_TABS
//#define SUPPORT_COMPOSITE_SEARCH

// Currently selected tab
enum searchTab {
    simpleTab   = 0,
    filesTab    = 2,
};

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

static int getTermArray(struct sqlConnection *conn, char ***pLabels, char ***pTerms, char *type)
// Pull out all term fields from ra entries with given type
// Returns count of items found and items via the terms argument.
{
int ix = 0, count = 0;
char **labels;
char **values;
struct slPair *pairs = mdbValLabelSearch(conn, type, MDB_VAL_STD_TRUNCATION, FALSE, TRUE); // Files not Tables
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

static int getFileFormatTypes(char ***pLabels, char ***pTypes)
{
char *crudeTypes[] = {
    ANYLABEL,
    "bam",
    "bam.bai",
    "tagAlign",
    "bed.gz",
    "bigBed",
    "broadPeak",
    "narrowPeak",
    "fastq",
    "bigWig",
    "wig"
};
char *nicerTypes[] = {
    ANYLABEL,
    "Alignment binary (bam) - binary SAM",
    "Alignment binary index (bai) - binary SAM index",
    "Alignment tags (tagAlign)",
    "bed - browser extensible data",
    "bigBed - self index, often remote bed format",
    "Peaks Broad (broadPeak) - ENCODE large region peak format",
    "Peaks Narrow (narrowPeak) - ENCODE small region peak format",
    "Raw Sequence (fastq) - High throughput sequence format",
    "Signal (bigWig) - self index, often remote wiggle format",
    "Signal (wig) - wiggle format"
};

int ix = 0, count = sizeof(crudeTypes)/sizeof(char *);
char **labels;
char **values;
AllocArray(labels, count);
AllocArray(values, count);
for(ix=0;ix<count;ix++)
    {
    labels[ix] = cloneString(nicerTypes[ix]);
    values[ix] = cloneString(crudeTypes[ix]);
    }
*pLabels = labels;
*pTypes = values;
return count;
}

static int metaDbVars(struct sqlConnection *conn, char *** metaVars, char *** metaLabels)
// Search the assemblies metaDb table; If name == NULL, we search every metadata field.
{
char query[256];
struct slPair *oneTerm,*whiteList = mdbCvWhiteList(TRUE,FALSE);
int count =0, whiteCount = slCount(whiteList);
char **retVar = needMem(sizeof(char *) * whiteCount);
char **retLab = needMem(sizeof(char *) * whiteCount);

for(oneTerm=whiteList;oneTerm!=NULL;oneTerm=oneTerm->next)
    {
    safef(query, sizeof(query), "select count(*) from metaDb where var = '%s'",oneTerm->name);
    if(sqlQuickNum(conn,query) > 0)
        {
        retVar[count] = oneTerm->name;
        retLab[count] = oneTerm->val;
        count++;
        }
    }
// Don't do it, unless you clone strings above:  slPairFreeValsAndList(&whileList);

*metaVars = retVar;
*metaLabels = retLab;
return count;
}

static int printMdbSelects(struct sqlConnection *conn,struct cart *cart,enum searchTab selectedTab,char ***pMdbVar,char ***pMdbVal,int *numMetadataNonEmpty,int cols)
// Prints a table of mdb selects if appropriate and returns number of them
// TODO: move to lib since hgTracks and hgFileSearch share it
{
// figure out how many metadata selects are visible.
int delSearchSelect = cartUsualInt(cart, TRACK_SEARCH_DEL_ROW, 0);   // 1-based row to delete
int addSearchSelect = cartUsualInt(cart, TRACK_SEARCH_ADD_ROW, 0);   // 1-based row to insert after
int numMetadataSelects = 0;
char **mdbVar = NULL;
char **mdbVal = NULL;
char **mdbVars = NULL;
char **mdbVarLabels = NULL;
int i, count = metaDbVars(conn, &mdbVars, &mdbVarLabels);

for(;;)
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
    *pMdbVar = mdbVar;
    *pMdbVal = mdbVal;
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
        if(selectedTab!=simpleTab)
            {
            int j;
            boolean found = FALSE;
            // We need to make sure mdbVar[i] is valid in this assembly; if it isn't, reset it to "cell".
            for(j = 0; j < count && !found; j++)
                if(sameString(mdbVars[j], mdbVar[i]))
                    found = TRUE;
            if(found)
                {
                safef(buf, sizeof(buf), "%s%d", METADATA_VALUE_PREFIX, i + offset);
                enum mdbCvSearchable searchBy = mdbCvSearchMethod(mdbVar[i]);
                if (searchBy == cvsSearchByMultiSelect)
                    {
                    // Adding support for multi-selects as comma delimited list of values
                    struct slName *vals = cartOptionalSlNameList(cart,buf);
                    if (vals)
                        {
                        mdbVal[i] = slNameListToString(vals,','); // A comma delimited list of values
                        slNameFreeList(&vals);
                        }
                    }
                else
                    mdbVal[i] = cloneString(cartUsualString(cart, buf, ANYLABEL));

                if (mdbVal[i] != NULL && sameString(mdbVal[i], ANYLABEL))
                    mdbVal[i] = NULL;
                }
            else
                {
                mdbVar[i] = cloneString("cell");
                mdbVal[i] = NULL;
                }
            if(!isEmpty(mdbVal[i]))
                (*numMetadataNonEmpty)++;
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

    printf("<tr><td colspan='%d' align='right' class='lineOnTop' style='height:20px; max-height:20px;'><em style='color:%s; width:200px;'>ENCODE terms</em></td></tr>\n", cols,COLOR_DARKGREY);
    for(i = 0; i < numMetadataSelects; i++)
        {
        char **terms = NULL, **labels = NULL;
        char buf[256];
        int len;

    #define PLUS_MINUS_BUTTON "<input type='button' id='%sButton%d' value='%c' style='font-size:.7em;' title='%s' onclick='findTracksMdbSelectPlusMinus(this,%d)'>"
    #define PRINT_PM_BUTTON(type,num,value) printf(PLUS_MINUS_BUTTON, (type), (num), (value), ((value) == '+' ? "add another row after":"delete"), (num))
        printf("<tr valign='top' class='mdbSelect'><td nowrap>\n");
        if(numMetadataSelects > 2 || i >= 2)
            PRINT_PM_BUTTON("minus", i + 1, '-');
        else
            printf("&nbsp;");
        PRINT_PM_BUTTON("plus", i + 1, '+');

        printf("</td><td>and&nbsp;</td><td colspan=3 nowrap>\n");
        safef(buf, sizeof(buf), "%s%i", METADATA_NAME_PREFIX, i + 1);
        cgiDropDownWithTextValsAndExtra(buf, mdbVarLabels, mdbVars,count,mdbVar[i],"class='mdbVar' style='font-size:.9em;' onchange='findTracksMdbVarChanged(this);'");
        // TODO: move to lib since hgTracks and hgApi share
        safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, i + 1);
        enum mdbCvSearchable searchBy = mdbCvSearchMethod(mdbVar[i]);
        if (searchBy == cvsSearchByMultiSelect)
            {
            printf("</td>\n<td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>is among</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",i + 1,buf);
            #define MULTI_SELECT_CBS_FORMAT "<SELECT MULTIPLE=true name='%s' style='display: none; min-width:200px; font-size:.9em;' class='filterBy mdbVal' onchange='findTracksMdbValChanged(this)'>\n"
            printf(MULTI_SELECT_CBS_FORMAT,buf);
            len = getTermArray(conn, &labels, &terms, mdbVar[i]);
            int tix=0;
            for(;tix < len;tix++)
                {
                char *selected = findWordByDelimiter(terms[tix],',', mdbVal[i]);
                printf("<OPTION%s value='%s'>%s</OPTION>\n",(selected != NULL?" SELECTED":""),terms[tix],labels[tix]);
                }
            printf("</SELECT>\n");
            }
        else if (searchBy == cvsSearchBySingleSelect)
            {
            printf("</td>\n<td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>is</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",i + 1,buf);
            len = getTermArray(conn, &labels, &terms, mdbVar[i]);
            cgiMakeDropListFull(buf, labels, terms, len, mdbVal[i], "class='mdbVal' style='min-width:200px; font-size:.9em;' onchange='findTracksMdbValChanged(this);'");
            }
        else if (searchBy == cvsSearchByFreeText)
            {
            printf("</td>\n<td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>contains</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",i + 1,buf);
            printf("<input type='text' name='%s' value='%s' class='mdbVal freeText' style='max-width:310px; width:310px; font-size:.9em;' onchange='findTracksMdbVarChanged(true);'>\n",
                    buf,(mdbVal[i] ? mdbVal[i]: ""));
            }
        else if (searchBy == cvsSearchByDateRange || searchBy == cvsSearchByIntegerRange)
            {
            // TO BE IMPLEMENTED
            }
        printf("<span id='helpLink%i'>&nbsp;</span></td>\n", i + 1);
        printf("</tr>\n");
        }

    printf("<tr><td colspan='%d' align='right' style='height:10px; max-height:10px;'>&nbsp;</td></tr>", cols);
    //printf("<tr><td colspan='%d' align='right' class='lineOnTop' style='height:20px; max-height:20px;'>&nbsp;</td></tr>", cols);

return numMetadataSelects;
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
int numMetadataSelects;
int numMetadataNonEmpty = 0;
char **mdbVar = NULL;
char **mdbVal = NULL;
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
char **formatTypes = NULL;
char **formatLabels = NULL;
int formatCount = getFileFormatTypes(&formatLabels, &formatTypes);
cgiMakeDropListFull(FILE_SEARCH_ON_FILETYPE, formatLabels, formatTypes, formatCount, fileTypeSearch, "class='fileTypeSearch' style='min-width:40%; font-size:.9em;'");
printf("</td></tr>\n");
if (selectedTab==filesTab && fileTypeSearch)
    searchTermsExist = TRUE;

// Metadata selects require careful accounting
if(metaDbExists)
    numMetadataSelects = printMdbSelects(conn, cart, selectedTab, &mdbVar, &mdbVal, &numMetadataNonEmpty, cols);
else
    numMetadataSelects = 0;

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
    // Convert to slPair list
    int ix=0;
    struct slPair *mdbPairs = NULL;
    for(ix = 0; ix < numMetadataSelects; ix++)
        {
        if(!isEmpty(mdbVal[ix]))
            slAddHead(&mdbPairs,slPairNew(mdbVar[ix],mdbVal[ix]));
        }
    slReverse(&mdbPairs);

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
        fileSearchResults(db, conn, mdbPairs, fileTypeSearch);
        if (measureTiming)
            uglyTime("Searched for files");
        }

    slPairFreeList(&mdbPairs);
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
