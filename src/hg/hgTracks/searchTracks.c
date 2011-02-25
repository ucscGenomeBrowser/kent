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
#include "fileUi.h"
#include "trix.h"
#include "jsHelper.h"
#include "imageV2.h"


#define ANYLABEL                 "Any"
#define TRACK_SEARCH_FORM        "trackSearch"
#define SEARCH_RESULTS_FORM      "searchResults"
#define METADATA_NAME_PREFIX     "hgt_mdbVar"
#define METADATA_VALUE_PREFIX    "hgt_mdbVal"
#define TRACK_SEARCH_CURRENT_TAB "tsCurTab"
#define TRACK_SEARCH_SIMPLE      "tsSimple"
#define TRACK_SEARCH_ON_NAME     "tsName"
#define TRACK_SEARCH_ON_TYPE     "tsType"
#define TRACK_SEARCH_ON_GROUP    "tsGroup"
#define TRACK_SEARCH_ON_DESCR    "tsDescr"
#define TRACK_SEARCH_SORT        "tsSort"

// If there are problems with multiSelect support, it can quickly be blocked!
//#define BLOCK_MULTI_SELECT_SUPPORT

//#define FILES_SEARCH
#ifdef FILES_SEARCH
    #define TRACK_SEARCH_ON_FILETYPE "tsFileType"
#endif///def FILES_SEARCH

// Currently selected tab
enum searchTab {
    simpleTab   = 0,
    advancedTab = 1,
    filesTab    = 2,
};

static int gCmpGroup(const void *va, const void *vb)
/* Compare groups based on label. */
{
const struct group *a = *((struct group **)va);
const struct group *b = *((struct group **)vb);
return strcmp(a->label, b->label);
}

// Would like to do a radio button choice ofsorts
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

static void findTracksSort(struct slRef **pTrack, enum sortBy sortBy)
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

static int getFormatTypes(char ***pLabels, char ***pTypes)
{
char *crudeTypes[] = {
    ANYLABEL,
    "bam",
    "psl",
    "chain",
    "netAlign",
    "maf",
    "bed",
    "bigBed",
    "ctgPos",
    "expRatio",
    "genePred",
    "broadPeak",
    "narrowPeak",
    "rmsk",
    "bedGraph",
    "bigWig",
    "wig",
    "wigMaf"
};
// Non-standard:
// type altGraphX
// type axt
// type bed5FloatScore
// type bed5FloatScoreWithFdr
// type chromGraph
// type clonePos
// type coloredExon
// type encodeFiveC
// type factorSource
// type ld2
// type logo
// type maf
// type sample
// type wigMafProt 0.0 1.0

char *nicerTypes[] = {
    ANYLABEL,
    "Alignment binary (bam) - binary SAM",
    "Alignment Blast (psl) - Blast output",
    "Alignment Chains (chain) - Pairwise alignment",
    "Alignment Nets (netAlign) - Net alignments",
    "Alignments (maf) - multiple alignment format",
    "bed - browser extensible data",
    "bigBed - self index, often remote bed format",
    "ctgPos - Contigs",
    "expRatio - Expression ratios",
    "Genes (genePred) - Gene prediction and annotation",
    "Peaks Broad (broadPeak) - ENCODE large region peak format",
    "Peaks Narrow (narrowPeak) - ENCODE small region peak format",
    "Repeats (rmsk) - Repeat masking",
    "Signal (bedGraph) - graphically represented bed data",
    "Signal (bigWig) - self index, often remote wiggle format",
    "Signal (wig) - wiggle format",
    "Signal (wigMaf) - multiple alignment wiggle"
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

#ifdef FILES_SEARCH
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
#endif///def FILES_SEARCH

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
            #ifdef BLOCK_MULTI_SELECT_SUPPORT
                if (searchBy == cvsSearchByMultiSelect)  // NOTE: Temprorarily bypass cv.ra
                    searchBy =  cvsSearchBySingleSelect;
            #endif///def BLOCK_MULTI_SELECT_SUPPORT
                if (searchBy == cvsSearchByMultiSelect)
                    {
                    // Multi-selects as comma delimited list of values
                    struct slName *vals = cartOptionalSlNameList(cart,buf);
                    if (vals)
                        {
                        mdbVal[i] = slNameListToString(vals,','); // A comma delimited list of values
                        slNameFreeList(&vals);
                        }
                    }
                else
                    mdbVal[i] = cloneString(cartUsualString(cart, buf,ANYLABEL));

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

    hPrintf("<tr><td colspan='%d' align='right' class='lineOnTop' style='height:20px; max-height:20px;'><em style='color:%s; width:200px;'>ENCODE terms</em></td></tr>\n", cols,COLOR_DARKGREY);
    for(i = 0; i < numMetadataSelects; i++)
        {
        char **terms = NULL, **labels = NULL;
        char buf[256];
        int len;

    #define PLUS_MINUS_BUTTON "<input type='button' id='%sButton%d' value='%c' style='font-size:.7em;' title='%s' onclick='findTracksMdbSelectPlusMinus(this,%d)'>"
    #define PRINT_PM_BUTTON(type,num,value) printf(PLUS_MINUS_BUTTON, (type), (num), (value), ((value) == '+' ? "add another row after":"delete"), (num))
        hPrintf("<tr valign='top' class='mdbSelect'><td nowrap>\n");
        if(numMetadataSelects > 2 || i >= 2)
            PRINT_PM_BUTTON("minus", i + 1, '-');
        else
            hPrintf("&nbsp;");
        PRINT_PM_BUTTON("plus", i + 1, '+');

        hPrintf("</td><td>and&nbsp;</td><td colspan=3 nowrap>\n");
        safef(buf, sizeof(buf), "%s%i", METADATA_NAME_PREFIX, i + 1);
        enum mdbCvSearchable searchBy = mdbCvSearchMethod(mdbVar[i]);
    #ifdef BLOCK_MULTI_SELECT_SUPPORT
        if (searchBy == cvsSearchByMultiSelect)  // NOTE: Temprorarily bypass cv.ra
            searchBy =  cvsSearchBySingleSelect;
    #else///ndef BLOCK_MULTI_SELECT_SUPPORT
        cgiDropDownWithTextValsAndExtra(buf, mdbVarLabels, mdbVars,count,mdbVar[i],"class='mdbVar' style='font-size:.9em;' onchange='findTracksMdbVarChanged(this);'");
        safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, i + 1);
    #endif///ndef BLOCK_MULTI_SELECT_SUPPORT
        if (searchBy == cvsSearchByMultiSelect)
            {
        #ifdef BLOCK_MULTI_SELECT_SUPPORT
            cgiDropDownWithTextValsAndExtra(buf, mdbVarLabels, mdbVars,count,mdbVar[i],"class='mdbVar' style='font-size:.9em;' onchange='findTracksMdbVarChanged(this);'");
            safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, i + 1);
        #endif///ndef BLOCK_MULTI_SELECT_SUPPORT
            printf("</td>\n<td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>is (any of)</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",i + 1,buf);
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
        #ifdef BLOCK_MULTI_SELECT_SUPPORT
            cgiDropDownWithTextValsAndExtra(buf, mdbVarLabels, mdbVars,count,mdbVar[i],"class='mdbVar noMulti' style='font-size:.9em;' onchange='findTracksMdbVarChanged(this);'");
            safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, i + 1);
        #endif///ndef BLOCK_MULTI_SELECT_SUPPORT
            hPrintf("</td>\n<td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>is</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",i + 1,buf);
            len = getTermArray(conn, &labels, &terms, mdbVar[i]);
            cgiMakeDropListFull(buf, labels, terms, len, mdbVal[i], "class='mdbVal' style='min-width:200px; font-size:.9em;' onchange='findTracksMdbValChanged(this);'");
            }
        else if (searchBy == cvsSearchByFreeText)
            {
        #ifdef BLOCK_MULTI_SELECT_SUPPORT
            cgiDropDownWithTextValsAndExtra(buf, mdbVarLabels, mdbVars,count,mdbVar[i],"class='mdbVar noMulti' style='font-size:.9em;' onchange='findTracksMdbVarChanged(this);'");
            safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, i + 1);
        #endif///ndef BLOCK_MULTI_SELECT_SUPPORT
            hPrintf("</td><td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>contains</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",i + 1,buf);
            hPrintf("<input type='text' name='%s' value='%s' class='mdbVal freeText' style='max-width:310px; width:310px; font-size:.9em;' onchange='findTracksMdbVarChanged(true);'>\n",
                    buf,(mdbVal[i] ? mdbVal[i]: ""));
            }
        else if (searchBy == cvsSearchByDateRange || searchBy == cvsSearchByIntegerRange)
            {
            // TO BE IMPLEMENTED
            }
        hPrintf("<span id='helpLink%i'>&nbsp;</span></td>\n", i + 1);
        hPrintf("</tr>\n");
        }

    hPrintf("<tr><td colspan='%d' align='right' style='height:10px; max-height:10px;'>&nbsp;</td></tr>", cols);
    //hPrintf("<tr><td colspan='%d' align='right' class='lineOnTop' style='height:20px; max-height:20px;'>&nbsp;</td></tr>", cols);

return numMetadataSelects;
}

static struct slRef *simpleSearchForTracksstruct(struct trix *trix,char **descWords,int descWordCount)
// Performs the simple search and returns the found tracks.
{
struct slRef *tracks = NULL;

struct trixSearchResult *tsList;
for(tsList = trixSearch(trix, descWordCount, descWords, TRUE); tsList != NULL; tsList = tsList->next)
    {
    struct track *track = (struct track *) hashFindVal(trackHash, tsList->itemId);
    if (track != NULL)  // It is expected that this is NULL (e.g. when the trix references trackDb tracks which have no tables)
        {
        refAdd(&tracks, track);
        }
    }
return tracks;
}

static struct slRef *advancedSearchForTracks(struct sqlConnection *conn,struct group *groupList, char **descWords,int descWordCount,
                                             char *nameSearch, char *typeSearch, char *descSearch, char *groupSearch, struct slPair *mdbPairs)
// Performs the advanced search and returns the found tracks.
{
int tracksFound = 0;
struct slRef *tracks = NULL;
int numMetadataNonEmpty = slCount(mdbPairs);

    if(!isEmpty(nameSearch) || typeSearch != NULL || descSearch != NULL || groupSearch != NULL || numMetadataNonEmpty)
        {
        // First do the metaDb searches, which can be done quickly for all tracks with db queries.
        struct hash *matchingTracks = newHash(0);

        if (numMetadataNonEmpty)
            {

            struct mdbObj *mdbObj, *mdbObjs = mdbObjRepeatedSearch(conn,mdbPairs,TRUE,FALSE);
            if (mdbObjs)
                {
                for (mdbObj = mdbObjs; mdbObj != NULL; mdbObj = mdbObj->next)
                    hashAddInt(matchingTracks, mdbObj->obj, 1);
                mdbObjsFree(&mdbObjs);
                }
            }

        struct group *group;
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
                        char *trackType = cloneFirstWord(track->tdb->type); // will be spilled
                        if((isEmpty(nameSearch) || isNameMatch(track, nameSearch, "contains")) &&
                           (isEmpty(typeSearch) || (sameWord(typeSearch, trackType) && !tdbIsComposite(track->tdb))) &&
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
                                trackType = cloneFirstWord(subTrack->tdb->type); // will be spilled
                                if((isEmpty(nameSearch) || isNameMatch(subTrack, nameSearch, "contains")) &&
                                   (isEmpty(typeSearch) || sameWord(typeSearch, trackType)) &&
                                   (isEmpty(descSearch) || isDescriptionMatch(subTrack, descWords, descWordCount)) &&
                                   (!numMetadataNonEmpty || hashLookup(matchingTracks, subTrack->track) != NULL))
                                    {
                                    // XXXX to parent hash. - use tdb->parent instead.
                                    //hashAdd(parents, subTrack->track, track);
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

return tracks;
}

#define MAX_FOUND_TRACKS 100
static void findTracksPageLinks(int tracksFound, int startFrom)
{
if (tracksFound <= MAX_FOUND_TRACKS)
    return;

// Opener
int willStartAt = 0;
int curPage  = (startFrom/MAX_FOUND_TRACKS) + 1;
int endAt = startFrom+MAX_FOUND_TRACKS;
if (endAt > tracksFound)
    endAt = tracksFound;
hPrintf("<span><em style='font-size:.9em;'>Listing %d - %d of %d tracks</em>&nbsp;&nbsp;&nbsp;",startFrom+1,endAt,tracksFound);

// << and <
if (startFrom >= MAX_FOUND_TRACKS)
    {
    hPrintf("<a href='../cgi-bin/hgTracks?%s=Search&%s=0' title='First page of found tracks' onclick='return findTracksPage(\"%s\",0);'>&#171;</a>&nbsp;",
            TRACK_SEARCH,TRACK_SEARCH_PAGER,TRACK_SEARCH_PAGER);
    willStartAt = startFrom - MAX_FOUND_TRACKS;
    hPrintf("&nbsp;<a href='../cgi-bin/hgTracks?%s=Search&%s=%d' title='Previous page of found tracks' onclick='return findTracksPage(\"%s\",%d);'>&#139;</a>&nbsp;",
            TRACK_SEARCH,TRACK_SEARCH_PAGER,willStartAt,TRACK_SEARCH_PAGER,willStartAt);
    }

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
        willStartAt = ((thisPage - 1) * MAX_FOUND_TRACKS);
        endAt = willStartAt+ MAX_FOUND_TRACKS;
        if (endAt > tracksFound)
            endAt = tracksFound;
        hPrintf("&nbsp;<a href='../cgi-bin/hgTracks?%s=Search&%s=%d' title='Page %d (%d - %d) tracks' onclick='return findTracksPage(\"%s\",%d);'>%d</a>&nbsp;",
                TRACK_SEARCH,TRACK_SEARCH_PAGER,willStartAt,thisPage,willStartAt+1,endAt,TRACK_SEARCH_PAGER,willStartAt,thisPage);
        }
    else
        hPrintf("&nbsp;<em style='color:%s;'>%d</em>&nbsp;",COLOR_DARKGREY,thisPage);
    }

// > and >>
if ((startFrom + MAX_FOUND_TRACKS) < tracksFound)
    {
    willStartAt = startFrom + MAX_FOUND_TRACKS;
    hPrintf("&nbsp;<a href='../cgi-bin/hgTracks?%s=Search&%s=%d' title='Next page of found tracks' onclick='return findTracksPage(\"%s\",%d);'>&#155;</a>&nbsp;",
            TRACK_SEARCH,TRACK_SEARCH_PAGER,willStartAt,TRACK_SEARCH_PAGER,willStartAt);
    willStartAt =  tracksFound - (tracksFound % MAX_FOUND_TRACKS);
    if (willStartAt == tracksFound)
        willStartAt -= MAX_FOUND_TRACKS;
    hPrintf("&nbsp;<a href='../cgi-bin/hgTracks?%s=Search&%s=%d' title='Last page of found tracks' onclick='return findTracksPage(\"%s\",%d);'>&#187;</a></span>\n",
            TRACK_SEARCH,TRACK_SEARCH_PAGER,willStartAt,TRACK_SEARCH_PAGER,willStartAt);
    }
}

static void displayFoundTracks(struct cart *cart, struct slRef *tracks, int tracksFound,enum sortBy sortBy)
// Routine for displaying found tracks
{
hPrintf("<div id='found' style='display:none;'>\n"); // This div allows the clear button to empty it
if(tracksFound < 1)
    {
    hPrintf("<p>No tracks found</p>\n");
    }
else
    {
    struct hash *tdbHash = makeTrackHash(database, chromName);
    hPrintf("<form action='%s' name='%s' id='%s' method='post'>\n\n", hgTracksName(),SEARCH_RESULTS_FORM,SEARCH_RESULTS_FORM);
    cartSaveSession(cart);  // Creates hidden var of hgsid to avoid bad voodoo

    int startFrom = 0;
    hPrintf("<table id='foundTracks'>\n");

    // Opening view in browser button and foundTracks count
    #define ENOUGH_FOUND_TRACKS 10
    if(tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("<tr><td nowrap colspan=3>\n");
        hPrintf("<INPUT TYPE=SUBMIT NAME='submit' VALUE='return to browser' class='viewBtn' style='font-size:.8em;'>");
        hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<FONT class='selCbCount'></font>\n");

        startFrom = cartUsualInt(cart,TRACK_SEARCH_PAGER,0);
        if (startFrom > 0 && startFrom < tracksFound)
            {
            int countUp = 0;
            for(countUp=0; countUp < startFrom;countUp++)
                {
                if (slPopHead(&tracks) == NULL) // memory waste
                    break;
                }
            }
        hPrintf("</td><td align='right' valign='bottom'>\n");
        findTracksPageLinks(tracksFound,startFrom);
        hPrintf("</td></tr>\n");
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
        cgiMakeOnClickRadioButton(TRACK_SEARCH_SORT, "0", (sortBy == sbRelevance),"onchange=\"findTracksSortNow(this);\"");
        hPrintf("by Relevance");
        cgiMakeOnClickRadioButton(TRACK_SEARCH_SORT, "1", (sortBy == sbAbc),      "onchange=\"findTracksSortNow(this);\"");
        hPrintf("Alphabetically");
        cgiMakeOnClickRadioButton(TRACK_SEARCH_SORT, "2",(sortBy == sbHierarchy), "onchange=\"findTracksSortNow(this);\"");
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
        jsonTdbSettingsBuild(&jsonTdbVars, track, FALSE); // FALSE: No configuration from track search

        if (tdbIsFolder(track->tdb)) // supertrack
            hPrintf("<tr bgcolor='%s' valign='top' class='found'>\n","#EED5B7");//"#DEB887");//"#E6B426");//#FCECC0//COLOR_LTGREY);//COLOR_LTGREEN);//COLOR_TRACKLIST_LEVEL1);
        else if (tdbIsContainer(track->tdb))
            hPrintf("<tr bgcolor='%s' valign='top' class='found'>\n",COLOR_TRACKLIST_LEVEL3);
        else
            hPrintf("<tr bgcolor='%s' valign='top' class='found'>\n",COLOR_TRACKLIST_LEVEL2);

        hPrintf("<td align='center'>\n");

        // Determine visibility and checked state
        track->visibility = tdbVisLimitedByAncestors(cart, track->tdb, TRUE, TRUE);
        boolean checked = ( track->visibility != tvHide );
        if(tdbIsContainerChild(track->tdb))
            {
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
            containerTrackCount++; // Using onclick ensures return to search tracks on submit
            hPrintf("&nbsp;<IMG SRC='../images/folderWrench.png' style='cursor:pointer;' title='Configure this track container...' onclick='findTracksConfigureSet(\"%s\");'>&nbsp;", track->track);
            }
//#define SHOW_PARENT_FOLDER
#ifdef SHOW_PARENT_FOLDER
        else if (tdbIsContainerChild(track->tdb) || tdbIsFolderContent(track->tdb))
            {
            struct trackDb *parentTdb = tdbIsContainerChild(track->tdb) ? tdbGetContainer(track->tdb) : tdbGetImmediateFolder(track->tdb);
            if (parentTdb != NULL) // Using href will not return to search tracks on submit
                hPrintf("&nbsp;<A HREF='../cgi-bin/hgTrackUi?g=%s'><IMG SRC='../images/folderC.png' title='Navigate to parent container...'></A>&nbsp;", parentTdb->track);
            }
#endif///def SHOW_PARENT_FOLDER
        hPrintf("</td>\n");

        // shortLabel has description popup and longLabel has "..." metadata
        hPrintf("<td><a target='_top' onclick=\"hgTrackUiPopUp('%s',true); return false;\" href='%s' title='Display track details'>%s</a></td>\n", track->track, trackUrl(track->track, NULL), track->shortLabel);
        hPrintf("<td>%s", track->longLabel);
        compositeMetadataToggle(database, track->tdb, "...", TRUE, FALSE, tdbHash);
        hPrintf("</td></tr>\n");
        }
    //hPrintf("</table>\n");

    // Closing view in browser button and foundTracks count
    hPrintf("<tr><td nowrap colspan=3>");
    hPrintf("<INPUT TYPE=SUBMIT NAME='submit' VALUE='Return to Browser' class='viewBtn' style='font-size:.8em;'>");
    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<FONT class='selCbCount'></font>");
    if(tracksFound >= ENOUGH_FOUND_TRACKS)
        {
        hPrintf("</td><td align='right' valign='top'>\n");
        findTracksPageLinks(tracksFound,startFrom);
        hPrintf("</td></tr>\n");
        }
    hPrintf("</table>\n");

    if(containerTrackCount > 0)
        hPrintf("<BR><IMG SRC='../images/folderWrench.png'>&nbsp;Tracks so marked are containers which group related data tracks.  Containers may need additional configuration (by clicking on the <IMG SRC='../images/folderWrench.png'> icon) before they can be viewed in the browser.<BR>\n");
        //hPrintf("* Tracks so marked are containers which group related data tracks.  These may not be visible unless further configuration is done.  Click on the * to configure these.<BR><BR>\n");
    hPrintf("\n</form>\n");

    // be done with json
    hWrites(jsonTdbSettingsUse(&jsonTdbVars));
    }
#ifdef OMIT
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
#endif///def OMIT
hPrintf("</div>"); // This div allows the clear button to empty it
}

void doSearchTracks(struct group *groupList)
{
if (!advancedJavascriptFeaturesEnabled(cart))
    {
    warn("Requires advanced javascript features.");
    return;
    }

#ifndef BLOCK_MULTI_SELECT_SUPPORT
webIncludeResourceFile("ui.dropdownchecklist.css");
//jsIncludeFile("ui.core.js",NULL);   // NOTE: This appears to be not needed as long as jquery-ui.js comes before ui.dropdownchecklist.js
jsIncludeFile("ui.dropdownchecklist.js",NULL);
// This line is needed to get the multi-selects initialized
hPrintf("<script type='text/javascript'>$(document).ready(function() { $('.filterBy').each( function(i) { $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: true });});});</script>\n");
#endif///ndef BLOCK_MULTI_SELECT_SUPPORT

struct group *group;
char *groups[128];
char *labels[128];
int numGroups = 1;
groups[0] = ANYLABEL;
labels[0] = ANYLABEL;
char *nameSearch = cartOptionalString(cart, TRACK_SEARCH_ON_NAME);
char *typeSearch = cartOptionalString(cart, TRACK_SEARCH_ON_TYPE);
#ifdef FILES_SEARCH
char *fileTypeSearch = cartOptionalString(cart, TRACK_SEARCH_ON_FILETYPE);
#endif///def FILES_SEARCH
char *descSearch=FALSE;
char *groupSearch = cartOptionalString(cart, TRACK_SEARCH_ON_GROUP);
boolean doSearch = sameString(cartOptionalString(cart, TRACK_SEARCH), "Search") || cartUsualInt(cart, TRACK_SEARCH_PAGER, -1) >= 0;
struct sqlConnection *conn = hAllocConn(database);
boolean metaDbExists = sqlTableExists(conn, "metaDb");
int numMetadataSelects, tracksFound = 0;
int numMetadataNonEmpty = 0;
char **mdbVar = NULL;
char **mdbVal = NULL;
#ifdef ONE_FUNC
struct hash *parents = newHash(4);
#endif///def ONE_FUNC
struct trix *trix;
char trixFile[HDB_MAX_PATH_STRING];
char **descWords = NULL;
int descWordCount = 0;
boolean searchTermsExist = FALSE;
int cols;
char buf[512];

enum searchTab selectedTab = simpleTab;
char *currentTab = cartUsualString(cart, TRACK_SEARCH_CURRENT_TAB, "simpleTab");
if(sameString(currentTab, "simpleTab"))
    {
    selectedTab = simpleTab;
    descSearch = cartOptionalString(cart, TRACK_SEARCH_SIMPLE);
    freez(&nameSearch);
    }
else if(sameString(currentTab, "advancedTab"))
    {
    selectedTab = advancedTab;
    descSearch = cartOptionalString(cart, TRACK_SEARCH_ON_DESCR);
    }
#ifdef FILES_SEARCH
else if(sameString(currentTab, "filesTab"))
    {
    selectedTab = filesTab;
    descSearch = cartOptionalString(cart, TRACK_SEARCH_ON_DESCR);
    }
#endif///def FILES_SEARCH

if(descSearch)
    stripChar(descSearch, '"');
trackList = getTrackList(&groupList, -2); // global
makeGlobalTrackHash(trackList);

// NOTE: This is necessary when container cfg by '*' results in vis changes
// This will handle composite/view override when subtrack specific vis exists, AND superTrack reshaping.
parentChildCartCleanup(trackList,cart,oldVars); // Subtrack settings must be removed when composite/view settings are updated

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

safef(buf, sizeof(buf),"Search for Tracks in the %s %s Assembly", organism, hFreezeFromDb(database));
webStartWrapperDetailedNoArgs(cart, database, "", buf, FALSE, FALSE, FALSE, FALSE);

hPrintf("<div style='max-width:1080px;'>");
hPrintf("<form action='%s' name='%s' id='%s' method='get'>\n\n", hgTracksName(),TRACK_SEARCH_FORM,TRACK_SEARCH_FORM);
cartSaveSession(cart);  // Creates hidden var of hgsid to avoid bad voodoo
safef(buf, sizeof(buf), "%lu", clock1());
cgiMakeHiddenVar("hgt_", buf);  // timestamps page to avoid browser cache


hPrintf("<input type='hidden' name='db' value='%s'>\n", database);
hPrintf("<input type='hidden' name='%s' id='currentTab' value='%s'>\n", TRACK_SEARCH_CURRENT_TAB, currentTab);
hPrintf("<input type='hidden' name='%s' value=''>\n",TRACK_SEARCH_DEL_ROW);
hPrintf("<input type='hidden' name='%s' value=''>\n",TRACK_SEARCH_ADD_ROW);
hPrintf("<input type='hidden' name='%s' value=''>\n",TRACK_SEARCH_PAGER);

hPrintf("<div id='tabs' style='display:none; %s'>\n"
        "<ul>\n"
        "<li><a href='#simpleTab'><B style='font-size:.9em;font-family: arial, Geneva, Helvetica, san-serif;'>Search</B></a></li>\n"
        "<li><a href='#advancedTab'><B style='font-size:.9em;font-family: arial, Geneva, Helvetica, san-serif;'>Advanced</B></a></li>\n"
#ifdef FILES_SEARCH
        "<li><a href='#filesTab'><B style='font-size:.9em;font-family: arial, Geneva, Helvetica, san-serif;'>Files</B></a></li>\n"
#endif///def FILES_SEARCH
        "</ul>\n"
        "<div id='simpleTab' style='max-width:inherit;'>\n",cgiBrowser()==btIE?"width:1060px;":"max-width:inherit;");

hPrintf("<table id='simpleTable' style='width:100%%; font-size:.9em;'><tr><td colspan='2'>");
hPrintf("<input type='text' name='%s' id='simpleSearch' class='submitOnEnter' value='%s' style='max-width:1000px; width:100%%;' onkeyup='findTracksSearchButtonsEnable(true);'>\n",
        TRACK_SEARCH_SIMPLE,descSearch == NULL ? "" : descSearch);
if (selectedTab==simpleTab && descSearch)
    searchTermsExist = TRUE;

hPrintf("</td></tr><td style='max-height:4px;'></td></tr></table>");
//hPrintf("</td></tr></table>");
hPrintf("<input type='submit' name='%s' id='searchSubmit' value='search' style='font-size:.8em;'>\n", TRACK_SEARCH);
hPrintf("<input type='button' name='clear' value='clear' class='clear' style='font-size:.8em;' onclick='findTracksClear();'>\n");
hPrintf("<input type='submit' name='submit' value='cancel' class='cancel' style='font-size:.8em;'>\n");
hPrintf("</div>\n");

// Advanced tab
hPrintf("<div id='advancedTab' style='width:inherit;'>\n"
        "<table id='advancedTable' cellSpacing=0 style='width:inherit; font-size:.9em;'>\n");
cols = 8;

// Track Name contains
hPrintf("<tr><td colspan=3></td>");
hPrintf("<td nowrap><b style='max-width:100px;'>Track&nbsp;Name:</b></td>");
hPrintf("<td align='right'>contains</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
hPrintf("<input type='text' name='%s' id='nameSearch' class='submitOnEnter' value='%s' onkeyup='findTracksSearchButtonsEnable(true);' style='min-width:326px; font-size:.9em;'>",
        TRACK_SEARCH_ON_NAME, nameSearch == NULL ? "" : nameSearch);
hPrintf("</td></tr>\n");

// Description contains
hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>");
hPrintf("<td><b style='max-width:100px;'>Description:</b></td>");
hPrintf("<td align='right'>contains</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
hPrintf("<input type='text' name='%s' id='descSearch' value='%s' class='submitOnEnter' onkeyup='findTracksSearchButtonsEnable(true);' style='max-width:536px; width:536px; font-size:.9em;'>",
        TRACK_SEARCH_ON_DESCR, descSearch == NULL ? "" : descSearch);
hPrintf("</td></tr>\n");
if (selectedTab==advancedTab && descSearch)
    searchTermsExist = TRUE;

hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
hPrintf("<td><b style='max-width:100px;'>Group:</b></td>");
hPrintf("<td align='right'>is</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
cgiMakeDropListFull(TRACK_SEARCH_ON_GROUP, labels, groups, numGroups, groupSearch, "class='groupSearch' style='min-width:40%; font-size:.9em;'");
hPrintf("</td></tr>\n");
if (selectedTab==advancedTab && groupSearch)
    searchTermsExist = TRUE;

// Track Type is (drop down)
hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
hPrintf("<td nowrap><b style='max-width:100px;'>Data Format:</b></td>");
hPrintf("<td align='right'>is</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
char **formatTypes = NULL;
char **formatLabels = NULL;
int formatCount = getFormatTypes(&formatLabels, &formatTypes);
cgiMakeDropListFull(TRACK_SEARCH_ON_TYPE, formatLabels, formatTypes, formatCount, typeSearch, "class='typeSearch' style='min-width:40%; font-size:.9em;'");
hPrintf("</td></tr>\n");
if (selectedTab==advancedTab && typeSearch)
    searchTermsExist = TRUE;

// Metadata selects require careful accounting
if(metaDbExists)
    numMetadataSelects = printMdbSelects(conn, cart, selectedTab, &mdbVar, &mdbVal, &numMetadataNonEmpty, cols);
else
    numMetadataSelects = 0;

hPrintf("</table>\n");
hPrintf("<input type='submit' name='%s' id='searchSubmit' value='search' style='font-size:.8em;'>\n", TRACK_SEARCH);
hPrintf("<input type='button' name='clear' value='clear' class='clear' style='font-size:.8em;' onclick='findTracksClear();'>\n");
hPrintf("<input type='submit' name='submit' value='cancel' class='cancel' style='font-size:.8em;'>\n");
//hPrintf("<a target='_blank' href='../goldenPath/help/trackSearch.html'>help</a>\n");
hPrintf("</div>\n");

#ifdef FILES_SEARCH
// Files tab
hPrintf("<div id='filesTab' style='width:inherit;'>\n"
        "<table id='filesTable' cellSpacing=0 style='width:inherit; font-size:.9em;'>\n");
cols = 8;

//// Track Name contains
//hPrintf("<tr><td colspan=3></td>");
//hPrintf("<td nowrap><b style='max-width:100px;'>Track&nbsp;Name:</b></td>");
//hPrintf("<td align='right'>contains</td>\n");
//hPrintf("<td colspan='%d'>", cols - 4);
//hPrintf("<input type='text' name='%s' id='nameSearch' class='submitOnEnter' value='%s' onkeyup='findTracksSearchButtonsEnable(true);' style='min-width:326px; font-size:.9em;'>",
//        TRACK_SEARCH_ON_NAME, nameSearch == NULL ? "" : nameSearch);
//hPrintf("</td></tr>\n");
//
//// Description contains
//hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>");
//hPrintf("<td><b style='max-width:100px;'>Description:</b></td>");
//hPrintf("<td align='right'>contains</td>\n");
//hPrintf("<td colspan='%d'>", cols - 4);
//hPrintf("<input type='text' name='%s' id='descSearch' value='%s' class='submitOnEnter' onkeyup='findTracksSearchButtonsEnable(true);' style='max-width:536px; width:536px; font-size:.9em;'>",
//        TRACK_SEARCH_ON_DESCR, descSearch == NULL ? "" : descSearch);
//hPrintf("</td></tr>\n");
//if (selectedTab==fileTab && descSearch)
//    searchTermsExist = TRUE;
//
//hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n");
//hPrintf("<td><b style='max-width:100px;'>Group:</b></td>");
//hPrintf("<td align='right'>is</td>\n");
//hPrintf("<td colspan='%d'>", cols - 4);
//cgiMakeDropListFull(TRACK_SEARCH_ON_GROUP, labels, groups, numGroups, groupSearch, "class='groupSearch' style='min-width:40%; font-size:.9em;'");
//hPrintf("</td></tr>\n");
//if (selectedTab==fileTab && groupSearch)
//    searchTermsExist = TRUE;

// Track Type is (drop down)
hPrintf("<tr><td colspan=2></td><td align='right'>&nbsp;</td>\n");
//hPrintf("<tr><td colspan=2></td><td align='right'>and&nbsp;</td>\n"); // Bring back "and" if using "Track Name,Description or Group
hPrintf("<td nowrap><b style='max-width:100px;'>Data Format:</b></td>");
hPrintf("<td align='right'>is</td>\n");
hPrintf("<td colspan='%d'>", cols - 4);
formatCount = getFileFormatTypes(&formatLabels, &formatTypes);
cgiMakeDropListFull(TRACK_SEARCH_ON_FILETYPE, formatLabels, formatTypes, formatCount, fileTypeSearch, "class='fileTypeSearch' style='min-width:40%; font-size:.9em;'");
hPrintf("</td></tr>\n");
if (selectedTab==filesTab && fileTypeSearch)
    searchTermsExist = TRUE;

// Metadata selects require careful accounting
if(metaDbExists)
    numMetadataSelects = printMdbSelects(conn, cart, selectedTab, &mdbVar, &mdbVal, &numMetadataNonEmpty, cols);
else
    numMetadataSelects = 0;

hPrintf("</table>\n");
hPrintf("<input type='submit' name='%s' id='searchSubmit' value='search' style='font-size:.8em;'>\n", TRACK_SEARCH);
hPrintf("<input type='button' name='clear' value='clear' class='clear' style='font-size:.8em;' onclick='findTracksClear();'>\n");
hPrintf("<input type='submit' name='submit' value='cancel' class='cancel' style='font-size:.8em;'>\n");
//hPrintf("<a target='_blank' href='../goldenPath/help/trackSearch.html'>help</a>\n");
hPrintf("</div>\n");

#endif///def FILES_SEARCH
hPrintf("</div>\n");

hPrintf("</form>\n");
hPrintf("</div>"); // Restricts to max-width:1000px;

if (measureTiming)
    uglyTime("Rendered tabs");

if(descSearch != NULL && !strlen(descSearch))
    descSearch = NULL;
if(groupSearch != NULL && sameString(groupSearch, ANYLABEL))
    groupSearch = NULL;
if(typeSearch != NULL && sameString(typeSearch, ANYLABEL))
    typeSearch = NULL;

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
    struct slRef *tracks = NULL;
    if(selectedTab==simpleTab)
        tracks = simpleSearchForTracksstruct(trix,descWords,descWordCount);
    else if(selectedTab==advancedTab)
        tracks = advancedSearchForTracks(conn,groupList,descWords,descWordCount,nameSearch,typeSearch,descSearch,groupSearch,mdbPairs);
#ifdef FILES_SEARCH
    else if(selectedTab==filesTab && mdbPairs != NULL)
        fileSearchResults(database, conn, mdbPairs, fileTypeSearch);
#endif///def FILES_SEARCH

    if (measureTiming)
        uglyTime("Searched for tracks");

    // Sort and Print results
    if(selectedTab!=filesTab)
        {
        enum sortBy sortBy = cartUsualInt(cart,TRACK_SEARCH_SORT,sbRelevance);
        tracksFound = slCount(tracks);
        if(tracksFound > 1)
            findTracksSort(&tracks,sortBy);

        displayFoundTracks(cart,tracks,tracksFound,sortBy);

        if (measureTiming)
            uglyTime("Displayed found tracks");
        }
    slPairFreeList(&mdbPairs);
    }
hFreeConn(&conn);

webNewSection("About Track Search");
if(metaDbExists)
    hPrintf("<p>Search for terms in track names, descriptions, groups, and ENCODE "
            "metadata.  If multiple terms are entered, only tracks with all terms "
            "will be part of the results.");
else
    hPrintf("<p>Search for terms in track descriptions, groups, and names. "
            "If multiple terms are entered, only tracks with all terms "
            "will be part of the results.");
hPrintf("<BR><a target='_blank' href='../goldenPath/help/trackSearch.html'>more help</a></p>\n");
webEndSectionTables();
}
