// Search code which is shared between different CGIs: hgFileSearch and hgTracks(Track Search)

#include "search.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hPrint.h"
#include "trix.h"
#include "mdb.h"
#include "subText.h"
#include "jsHelper.h"

void getSearchTrixFile(char *database, char *buf, int len)
// Fill-in the name of the track search trix file
{
char *trixPath = cfgOptionDefault("browser.trixPath", "/gbdb/$db/trackDb.ix");
struct subText *subList = subTextNew("$db", database);
subTextStatic(subList, trixPath, buf, len);
}

boolean isSearchTracksSupported(char *database, struct cart *cart)
// Return TRUE if searchTracks is supported for this database and javascript is supported too
{
if (!advancedJavascriptFeaturesEnabled(cart))
    return FALSE;

char trixFile[HDB_MAX_PATH_STRING];
getSearchTrixFile(database, trixFile, sizeof(trixFile));
return fileExists(trixFile);
}

struct slPair *fileFormatSearchWhiteList()
// Gets the whitelist of approved file formats that is allowed for search
{
char *crudeTypes[] = {
    "bam",  //    "bam.bai" is now alway selected with bam,
    "tagAlign",
    "bed.gz",
    "bigBed",
    "broadPeak",
    "narrowPeak",
    "fastq",
    "bigWig",
    "wig"    // TODO: Add "other" category. TODO: make into multi-select
};
char *nicerTypes[] = {
    "Alignment binary (bam) - binary SAM",  //    "Alignment binary index (bai) - binary SAM index",
    "Alignment tags (tagAlign)",
    "bed - browser extensible data",
    "bigBed - self index, often remote bed format",
    "Peaks Broad (broadPeak) - ENCODE large region peak format",
    "Peaks Narrow (narrowPeak) - ENCODE small region peak format",
    "Raw Sequence (fastq) - High throughput sequence format",
    "Signal (bigWig) - self index, often remote wiggle format",
    "Signal (wig) - wiggle format"
};

struct slPair *fileTypes = NULL;
int ix = 0, count = sizeof(crudeTypes)/sizeof(char *);
for(ix=0;ix<count;ix++)
    slPairAdd(&fileTypes, crudeTypes[ix],cloneString(nicerTypes[ix])); // Name gets cloned while adding
return fileTypes;
}

char *fileFormatSelectHtml(char *name, char *selected, char *extraHtml)
// returns an allocated string of HTML for the fileType select drop down
{
struct slPair *fileTypes = fileFormatSearchWhiteList();
if (slCount(fileTypes) > 0)
    {
    char *dropDownHtml = cgiMakeSingleSelectDropList(name,fileTypes,selected,ANYLABEL,NULL,extraHtml);
    slPairFreeList(&fileTypes);
    return dropDownHtml;
    }
return NULL;
}

struct slPair *mdbSelectPairs(struct cart *cart, struct slPair *mdbVars)
// Returns the current mdb  vars and vals in the table of drop down selects
{
// figure out how many metadata selects are visible.
int numMetadataSelects = 0;

struct slPair *mdbSelectPairs = NULL;
if (mdbVars == NULL)
    return 0;

// Get the current number of rows in the table of mdb selects
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

// Requesting to add or delete any?
int delSearchSelect = cartUsualInt(cart, TRACK_SEARCH_DEL_ROW, 0);   // 1-based row to delete
int addSearchSelect = cartUsualInt(cart, TRACK_SEARCH_ADD_ROW, 0);   // 1-based row to insert after
if(delSearchSelect)
    numMetadataSelects--;
if(addSearchSelect)
    numMetadataSelects++;

if(numMetadataSelects)
    {
    int ix;
    char buf[256];
    for(ix = 0; ix < numMetadataSelects; ix++)
        {
        int offset;   // used to handle additions/deletions
        if(addSearchSelect > 0 && ix >= addSearchSelect)
            offset = 0; // do nothing to offset (i.e. copy data from previous row)
        else if(delSearchSelect > 0 && ix + 1 >= delSearchSelect)
            offset = 2;
        else
            offset = 1;
        safef(buf, sizeof(buf), "%s%d", METADATA_NAME_PREFIX, ix + offset);
        char *var = cartOptionalString(cart, buf);
        char *val = NULL;

        // We need to make sure var is valid in this assembly; if it isn't, reset it to "cell".
        if(slPairFindVal(mdbVars,var) == NULL)
            var = "cell";
        else
            {
            safef(buf, sizeof(buf), "%s%d", METADATA_VALUE_PREFIX, ix + offset);
            enum cvSearchable searchBy = cvSearchMethod(var);
            if (searchBy == cvSearchByMultiSelect)
                {
                // Multi-selects as comma delimited list of values
                struct slName *vals = cartOptionalSlNameList(cart,buf);
                if (vals)
                    {
                    val = slNameListToString(vals,','); // A comma delimited list of values
                    slNameFreeList(&vals);
                    }
                }
            else if (searchBy == cvSearchBySingleSelect
                 ||  searchBy == cvSearchByFreeText
                 ||  searchBy == cvSearchByWildList)
                val = cloneString(cartUsualString(cart, buf,ANYLABEL));
            //else if (searchBy == cvSearchByDateRange || searchBy == cvSearchByIntegerRange)
            //    {
            //    // TO BE IMPLEMENTED
            //    }

            if (val != NULL && sameString(val, ANYLABEL))
                val = NULL;
            }
        slPairAdd(&mdbSelectPairs,var,val); // val already cloned
        }
    if(delSearchSelect > 0)
        {
        safef(buf, sizeof(buf), "%s%d", METADATA_NAME_PREFIX, numMetadataSelects + 1);
        cartRemove(cart, buf);
        safef(buf, sizeof(buf), "%s%d", METADATA_VALUE_PREFIX, numMetadataSelects + 1);
        cartRemove(cart, buf);
        }
    }
else
    {
    // create defaults
    slPairAdd(&mdbSelectPairs,"cell",    NULL);
    slPairAdd(&mdbSelectPairs,"antibody",NULL);
    }

slReverse(&mdbSelectPairs);
return mdbSelectPairs;
}

char *mdbSelectsHtmlRows(struct sqlConnection *conn,struct slPair *mdbSelects,struct slPair *mdbVars,int cols,boolean fileSearch)
// genereates the html for the table rows containing mdb var and val selects.  Assume tableSearch unless fileSearch
{
struct dyString *output = dyStringNew(1024);

dyStringPrintf(output,"<tr><td colspan='%d' align='right' class='lineOnTop' style='height:20px; max-height:20px;'><em style='color:%s; width:200px;'>ENCODE terms</em></td></tr>\n", cols,COLOR_DARKGREY);

struct slPair *mdbSelect = mdbSelects;
int row = 0;
for(;mdbSelect != NULL; mdbSelect = mdbSelect->next)
    {
    char buf[256];
    char *dropDownHtml = NULL;

    #define PLUS_MINUS_BUTTON "<input type='button' id='%sButton%d' value='%c' style='font-size:.7em;' title='%s' onclick='findTracksMdbSelectPlusMinus(this,%d)'>"
    #define ADD_PM_BUTTON(type,num,value) dyStringPrintf(output,PLUS_MINUS_BUTTON, (type), (num), (value), ((value) == '+' ? "add another row after":"delete"), (num))

    dyStringAppend(output,"<tr valign='top' class='mdbSelect'><td nowrap>\n");
    row++;

    if(slCount(mdbSelects) > 2 || row > 2)
        ADD_PM_BUTTON("minus", row, '-');
    else
        dyStringAppend(output,"&nbsp;");
    ADD_PM_BUTTON("plus", row, '+');

    dyStringAppend(output,"</td><td>and&nbsp;</td><td colspan=3 nowrap>\n");
    safef(buf, sizeof(buf), "%s%i", METADATA_NAME_PREFIX, row);

    // Left side select of vars
    dropDownHtml = cgiMakeSingleSelectDropList(buf, mdbVars,mdbSelect->name, NULL,"mdbVar","style='font-size:.9em;' onchange='findTracksMdbVarChanged(this);'");
    if (dropDownHtml)
        {
        dyStringAppend(output,dropDownHtml);
        freeMem(dropDownHtml);
        }

    // Right side select of vals
    safef(buf, sizeof(buf), "%s%i", METADATA_VALUE_PREFIX, row);
    enum cvSearchable searchBy = cvSearchMethod(mdbSelect->name);
    if (searchBy == cvSearchBySingleSelect || searchBy == cvSearchByMultiSelect)
        {
        dyStringPrintf(output,"</td>\n<td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>is%s</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",
                row,(searchBy == cvSearchByMultiSelect?" among":""),buf);
        struct slPair *pairs = mdbValLabelSearch(conn, mdbSelect->name, MDB_VAL_STD_TRUNCATION, FALSE, !fileSearch, fileSearch); // not tags, either a file or table search
        if (slCount(pairs) > 0)
            {
            char *dropDownHtml = cgiMakeSelectDropList((searchBy == cvSearchByMultiSelect),
                    buf, pairs,mdbSelect->val, ANYLABEL,"mdbVal","style='min-width:200px; font-size:.9em;' onchange='findTracksMdbValChanged(this);'");
            if (dropDownHtml)
                {
                dyStringAppend(output,dropDownHtml);
                freeMem(dropDownHtml);
                }
            slPairFreeList(&pairs);
            }
        }
    else if (searchBy == cvSearchByFreeText)
        {
        dyStringPrintf(output,"</td><td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>contains</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",
                       row,buf);
        dyStringPrintf(output,"<input type='text' name='%s' value='%s' class='mdbVal freeText' style='max-width:310px; width:310px; font-size:.9em;' onchange='findTracksMdbVarChanged(true);'>\n",
                buf,(mdbSelect->val ? (char *)mdbSelect->val: ""));
        }
    else if (searchBy == cvSearchByWildList)
        {
        dyStringPrintf(output,"</td><td align='right' id='isLike%i' style='width:10px; white-space:nowrap;'>is among</td>\n<td nowrap id='%s' style='max-width:600px;'>\n",
                       row,buf);
        dyStringPrintf(output,"<input type='text' name='%s' value='%s' class='mdbVal wildList' title='enter comma separated list of values' style='max-width:310px; width:310px; font-size:.9em;' onchange='findTracksMdbVarChanged(true);'>\n",
                buf,(mdbSelect->val ? (char *)mdbSelect->val: ""));
        }
    //else if (searchBy == cvSearchByDateRange || searchBy == cvSearchByIntegerRange)
    //    {
    //    // TO BE IMPLEMENTED
    //    }
    dyStringPrintf(output,"<span id='helpLink%i'>&nbsp;</span></td>\n", row);
    dyStringPrintf(output,"</tr>\n");
    }

    dyStringPrintf(output,"<tr><td colspan='%d' align='right' style='height:10px; max-height:10px;'>&nbsp;</td></tr>", cols);

return dyStringCannibalize(&output);
}


static boolean searchMatchToken(char *string, char *token)
{
// do this with regex ? Would require all sorts of careful parsing for ()., etc.
if (string == NULL)
    return (token == NULL);
if (token == NULL)
    return TRUE;

if (!strchr(token,'*') && !strchr(token,'?'))
    return (strcasestr(string,token) != NULL);

char wordWild[1024];
safef(wordWild,sizeof wordWild,"*%s*",token);
return wildMatch(wordWild, string);

}

boolean searchNameMatches(struct trackDb *tdb, struct slName *wordList)
// returns TRUE if all words in preparsed list matches short or long label
// A "word" can be "multiple words" (parsed from quoteed string).
{
if (tdb->shortLabel == NULL || tdb->longLabel == NULL)
    return (wordList != NULL);

struct slName *word = wordList;
for(; word != NULL; word = word->next)
    {
    if (!searchMatchToken(tdb->shortLabel,word->name)
    &&  !searchMatchToken(tdb->longLabel, word->name))
        return FALSE;
    }
return TRUE;
}

boolean searchDescriptionMatches(struct trackDb *tdb, struct slName *wordList)
// returns TRUE if all words in preparsed list matches html description page.
// A "word" can be "multiple words" (parsed from quoteed string).
// Because description contains html, quoted string match has limits.
// DANGER: this will alter html of tdb struct (replacing \n with ' ', so the html should not be displayed after.
{
if (tdb->html == NULL)
    return (wordList != NULL);

if (strchr(tdb->html,'\n'))
    strSwapChar(tdb->html,'\n',' ');   // DANGER: don't own memory.  However, this CGI will use html for no other purpose

struct slName *word = wordList;
for(; word != NULL; word = word->next)
    {
    if (!searchMatchToken(tdb->html,word->name))
        return FALSE;
    }
return TRUE;
}

