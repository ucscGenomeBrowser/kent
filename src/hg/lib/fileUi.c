/* fileUi.c - human genome file downloads common controls. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jsHelper.h"
#include "cart.h"
#include "hdb.h"
#include "fileUi.h"
#include "hui.h"
#include "obscure.h"
#include "mdb.h"
#include "jsHelper.h"
#include "web.h"

// FIXME: Move to hui.h since hui.c also needs this
#define ENCODE_DCC_DOWNLOADS "encodeDCC"


void fileDbFree(struct fileDb **pFileList)
// free one or more fileDb objects
{
while (pFileList && *pFileList)
    {
    struct fileDb *oneFile = slPopHead(pFileList);

    freeMem(oneFile->fileName);
    freeMem(oneFile->fileType);
    freeMem(oneFile->fileDate);
    freeMem(oneFile->sortFields);
    freeMem(oneFile->reverse);
    mdbObjsFree(&(oneFile->mdb));
    freeMem(oneFile);
    }
}

struct fileDb *fileDbGet(char *db, char *dir, char *subDir, char *fileName)
// Returns NULL or if found a fileDb struct with name, size and date filled in.
{
static char *savedDb     = NULL;
static char *savedDir    = NULL;
static char *savedSubDir = NULL;
static struct fileDb *foundFiles = NULL;// Use static list to save excess IO
struct fileDb *oneFile = NULL;
if (foundFiles == NULL
||  savedDb  == NULL    || differentString(savedDb, db)
||  savedDir == NULL    || differentString(savedDir,dir)
||  savedSubDir == NULL || differentString(savedSubDir,subDir))
    {
    // free up any static mem
    freeMem(savedDb);
    freeMem(savedDir);
    freeMem(savedSubDir);
    fileDbFree(&foundFiles);

    FILE *scriptOutput = NULL;
    char buf[1024];
    char cmd[512];
    char *words[10];
    char *server = hDownloadsServer();

    boolean useRsync = TRUE;
    // Works:         rsync -avn rsync://hgdownload.cse.ucsc.edu/goldenPath/hg18/encodeDCC/wgEncodeBroadChipSeq/
    if (hIsBetaHost())
        safef(cmd,sizeof(cmd),"rsync -n rsync://hgdownload-test.cse.ucsc.edu/goldenPath/%s/%s/%s/beta/",  db, dir, subDir); // NOTE: Force this case because beta may think it's downloads server is "hgdownload.cse.ucsc.edu"
    else
        safef(cmd,sizeof(cmd),"rsync -n rsync://%s/goldenPath/%s/%s/%s/", server, db, dir, subDir);

    scriptOutput = popen(cmd, "r");
    while(fgets(buf, sizeof(buf), scriptOutput))
        {
        eraseTrailingSpaces(buf);
        if (!endsWith(buf,".md5sum")) // Just ignore these
            {
            int count = chopLine(buf, words);
            if (count >= 6 && useRsync == FALSE) // hgwdev is same as hgdownloads-test so can't use rsync
                {
                //-rw-rw-r-- 5  502826550 2010-10-22 16:51 /usr/local/apache/htdocs-hgdownload/goldenPath/hg19/encodeDCC/wgEncodeBroadHistone/wgEncodeBroadHistoneGm12878ControlStdRawDataRep1.fastq.gz
                AllocVar(oneFile);
                oneFile->fileSize = sqlUnsignedLong(words[2]);
                oneFile->fileDate = cloneString(words[3]);
                char *atSlash = strrchr(words[5], '/');
                if (atSlash != NULL)
                    oneFile->fileName = cloneString(atSlash + 1);
                else
                    oneFile->fileName = cloneString(words[5]);
                slAddHead(&foundFiles,oneFile);
                }
            else if (count == 5 && useRsync == TRUE)// genome and hgwbeta can use rsync because files are on different machine
                {
                //-rw-rw-r--    26420982 2009/09/29 14:53:30 wgEncodeBroadChipSeq/wgEncodeBroadChipSeqSignalNhlfH4k20me1.wig.gz
                AllocVar(oneFile);
                oneFile->fileSize = sqlUnsignedLong(words[1]);
                oneFile->fileDate = cloneString(words[2]);
                strSwapChar(oneFile->fileDate,'/','-');// Standardize YYYY-MM-DD, no time
                oneFile->fileName = cloneString(words[4]);
                slAddHead(&foundFiles,oneFile);
                }
            //warn("File:%s  size:%ld",foundFiles->fileName,foundFiles->fileSize);
            }
        }
    pclose(scriptOutput);

    // mark this as done to avoid excessive io
    savedDb     = cloneString(db);
    savedDir    = cloneString(dir);
    savedSubDir = cloneString(subDir);

    if (foundFiles == NULL)
        {
        AllocVar(oneFile);
        oneFile->fileName = cloneString("No files found!");
        oneFile->fileDate = cloneString(cmd);
        slAddHead(&foundFiles,oneFile);
        warn("No files found for command:\n%s",cmd);
        return NULL;
        }
    }

// special code that only gets called in debug mode
if (sameString(fileName,"listAll"))
    {
    for(oneFile=foundFiles;oneFile;oneFile=oneFile->next)
        warn("%s",oneFile->fileName);
    return NULL;
    }
// Look up the file and return it
struct fileDb *newList = NULL;
while (foundFiles)
    {
    oneFile = slPopHead(&foundFiles);
    if (sameString(fileName,oneFile->fileName))
        break;  // Found means removed from list: shorter list for next file.

    slAddHead(&newList,oneFile);
    oneFile = NULL;
    }
if (newList)
    foundFiles = slCat(foundFiles,newList);  // Order does not remain the same

if (oneFile != NULL && oneFile->fileType == NULL)
    {
    char *suffix = strchr(oneFile->fileName, '.');
    if (suffix != NULL && strlen(suffix) > 2)
        {
        oneFile->fileType = cloneString(suffix + 1);
        if (endsWith(oneFile->fileType,".gz"))
            chopSuffix(oneFile->fileType);
        }
    }
return oneFile;
}


static sortOrder_t *fileSortOrderGet(struct cart *cart,struct trackDb *parentTdb,struct mdbObj *mdbObjs)
/* Parses 'fileSortOrder' trackDb/cart instructions and returns a sort order struct or NULL.
   Some trickiness here.  sortOrder->sortOrder is from cart (changed by user action), as is sortOrder->order,
   But columns are in original tdb order (unchanging)!  However, if cart is null, all is from trackDb.ra */
{
int ix;
sortOrder_t *sortOrder = NULL;
char *carveSetting = NULL,*setting = NULL;
if (parentTdb)
    setting = trackDbSetting(parentTdb, FILE_SORT_ORDER);
if(setting)
    {
    sortOrder = needMem(sizeof(sortOrder_t));
    sortOrder->setting = cloneString(setting);
    carveSetting = sortOrder->setting;
    }
else
    {
    if (mdbObjs != NULL)
        {
        struct dyString *dySortFields = dyStringNew(512);
        struct mdbObj *commonVars = mdbObjsCommonVars(mdbObjs);
        // common vars are already in cv defined order, searchable is also sortable
        struct mdbVar *var = commonVars->vars;
        for( ;var != NULL; var = var->next)
            {
            if (differentWord(var->var,MDB_VAR_LAB_VERSION)     // Exclude certain vars
            &&  differentWord(var->var,MDB_VAR_SOFTWARE_VERSION)
            &&  cvSearchMethod(var->var) != cvNotSearchable)   // searchable is also sortable
                {
                if (mdbObjsHasCommonVar(mdbObjs, var->var,TRUE)) // Don't bother if all the vals are the same (missing okay)
                    continue;
                dyStringPrintf(dySortFields,"%s=%s ",var->var,strSwapChar(cloneString(cvLabel(var->var)),' ','_'));
                }
            }
        if (dyStringLen(dySortFields))
            {
            dyStringAppend(dySortFields,"fileSize=Size fileType=File_Type");
            setting = dyStringCannibalize(&dySortFields);
            }
        else
            dyStringFree(&dySortFields);
        mdbObjsFree(&commonVars);
        }
    if(setting == NULL) // Must be in trackDb or not a sortable list of files
        {
        #define FILE_SORT_ORDER_DEFAULT "cell=Cell_Line lab=Lab view=View replicate=Rep fileSize=Size fileType=File_Type dateSubmitted=Submitted dateUnrestricted=RESTRICTED<BR>Until"
        setting = FILE_SORT_ORDER_DEFAULT;
        }
    sortOrder = needMem(sizeof(sortOrder_t));
    carveSetting = cloneString(setting);
    sortOrder->setting = NULL;
    }

if (parentTdb)
    {
    sortOrder->htmlId = needMem(strlen(parentTdb->track)+20);
    safef(sortOrder->htmlId, (strlen(parentTdb->track)+20), "%s.%s", parentTdb->track,FILE_SORT_ORDER);
    if(cart != NULL)
        sortOrder->sortOrder = cloneString(cartOptionalString(cart, sortOrder->htmlId));
    }

sortOrder->count = chopByWhite(carveSetting,NULL,0);  // Get size
sortOrder->column  = needMem(sortOrder->count*sizeof(char*));
sortOrder->count = chopByWhite(carveSetting,sortOrder->column,sortOrder->count);
sortOrder->title   = needMem(sortOrder->count*sizeof(char*));
sortOrder->forward = needMem(sortOrder->count*sizeof(boolean));
sortOrder->order   = needMem(sortOrder->count*sizeof(int));
for (ix = 0; ix<sortOrder->count; ix++)
    {
    // separate out mtaDb var in sortColumn from title
    sortOrder->title[ix] = strchr(sortOrder->column[ix],'='); // Could be 'cell=Cell_Line'
    if (sortOrder->title[ix] != NULL)
        {
        sortOrder->title[ix][0] = '\0';
        sortOrder->title[ix]   = strSwapChar(sortOrder->title[ix]+1,'_',' '); // +1 jumps to next char after '='
        }
    else
        sortOrder->title[ix] = sortOrder->column[ix];         // or could be just 'cell'

    // Sort order defaults to forward but may be found in a cart var
    sortOrder->order[ix] = ix+1;
    sortOrder->forward[ix] = TRUE;
    if (sortOrder->sortOrder != NULL)
        {
        char *pos = stringIn(sortOrder->column[ix], sortOrder->sortOrder);// find tdb substr in cart current order string
        if(pos != NULL && pos[strlen(sortOrder->column[ix])] == '=')
            {
            int ord=1;
            char* pos2 = sortOrder->sortOrder;
            for(;*pos2 && pos2 < pos;pos2++)
                {
                if(*pos2 == '=') // Discovering sort order in cart
                    ord++;
                }
            sortOrder->forward[ix] = (pos[strlen(sortOrder->column[ix]) + 1] == '+');
            sortOrder->order[ix] = ord;
            }
        }
    }
if (sortOrder->sortOrder == NULL)
    sortOrder->sortOrder = cloneString(setting);      // no order in cart, all power to trackDb
return sortOrder;  // NOTE cloneString:words[0]==*sortOrder->column[0] and will be freed when sortOrder is freed
}

static int fileDbSortCmp(const void *va, const void *vb)
// Compare two sortable tdb items based upon sort columns.
{
const struct fileDb *a = *((struct fileDb **)va);
const struct fileDb *b = *((struct fileDb **)vb);
char **fieldsA = a->sortFields;
char **fieldsB = b->sortFields;
int ix=0;
int compared = 0;
while(fieldsA[ix] != NULL && fieldsB[ix] != NULL && compared == 0)
    {
    compared = strcmp(fieldsA[ix], fieldsB[ix]) * (a->reverse[ix]? -1: 1);
    ix++;
    }
return compared;
}

static void fileDbSortList(struct fileDb **fileList, sortOrder_t *sortOrder)
{// If sortOrder struct provided, will update sortFields in fileList, then sort it
if (sortOrder && fileList)
    {
    struct fileDb *oneFile = NULL;
    for(oneFile = *fileList;oneFile != NULL;oneFile=oneFile->next)
        {
        oneFile->sortFields = needMem(sizeof(char *)    * (sortOrder->count + 1)); // +1 Null terminated
        oneFile->reverse    = needMem(sizeof(boolean *) *  sortOrder->count);
        int ix;
        for(ix=0;ix<sortOrder->count;ix++)
            {
            char *field = NULL;
            if (sameString("fileSize",sortOrder->column[ix]))
                {
                char niceNumber[32];
                sprintf(niceNumber, "%.15lu", oneFile->fileSize);
                field = cloneString(niceNumber);
                }
            else if (sameString("fileType",sortOrder->column[ix]))
                field = oneFile->fileType;
            else
                field = mdbObjFindValue(oneFile->mdb,sortOrder->column[ix]);

            if (field)
                {
                oneFile->sortFields[sortOrder->order[ix] - 1] = field;
                oneFile->reverse[   sortOrder->order[ix] - 1] = (sortOrder->forward[ix] == FALSE);
                }
            else
                {
                oneFile->sortFields[sortOrder->order[ix] - 1] = NULL;
                oneFile->reverse[   sortOrder->order[ix] - 1] = FALSE;
                }
            oneFile->sortFields[sortOrder->count] = NULL;
            }
        }
    slSort(fileList,fileDbSortCmp);
    }
}

static char *removeCommonMdbVarsNotInSortOrder(struct mdbObj *mdbObjs,sortOrder_t *sortOrder)
{  // Removes varaibles common to all mdbObjs and not found in sortOrder. Returns allocated string oh removed var=val pairs
if (sortOrder != NULL)
    {
    // Remove common vars from mdbs grant=Bernstein; lab=Broad; dataType=ChipSeq; setType=exp; control=std;
    // However, keep the term if it is in the sortOrder
    struct dyString *dyCommon = dyStringNew(256);
    char *commonTerms[] = { "grant", "lab", "dataType", "control", "setType" };
    int tIx=0,sIx = 0;
    for(;tIx<ArraySize(commonTerms);tIx++)
        {
        for(sIx = 0;
            sIx<sortOrder->count && differentString(commonTerms[tIx],sortOrder->column[sIx]);
            sIx++) ;
        if (sIx<sortOrder->count) // Found in sort Order so leave it in mdbObjs
            continue;

        char *val = mdbRemoveCommonVar(mdbObjs, commonTerms[tIx]); // All mdbs have it and have the same val for it.
        if (val)
            dyStringPrintf(dyCommon,"%s=%s ",commonTerms[tIx],val);
        }
    return dyStringCannibalize(&dyCommon);
    }
return NULL;
}

static char *labelWithVocabLink(char *var,char *title,struct slPair *valsAndLabels,boolean tagsNotVals)
/* If the parentTdb has a controlledVocabulary setting and the vocabType is found,
   then label will be wrapped with the link to all relevent terms.  Return string is cloned. */
{
// Determine if the var is cvDefined.  If not, simple link
boolean cvDefined = FALSE;
struct hash *cvTypesOfTerms = (struct hash *)cvTermTypeHash();
if (cvTypesOfTerms != NULL)
    {
    struct hash *cvTermDef = hashFindVal(cvTypesOfTerms,var);
    if (cvTermDef)
        cvDefined = SETTING_IS_ON(hashFindVal(cvTermDef,"cvDefined"));
    }

struct dyString *dyLink = dyStringNew(256);
if (!cvDefined)
    dyStringPrintf(dyLink,"<A HREF='hgEncodeVocab?type=%s' title='Click for details of \"%s\"' TARGET=ucscVocab>%s</A>",
                   var,title,title);
else
    {
    dyStringPrintf(dyLink,"<A HREF='hgEncodeVocab?%s=",tagsNotVals?"tag":"term");
    struct slPair *oneVal = valsAndLabels;
    for(;oneVal!=NULL;oneVal=oneVal->next)
        {
        if (oneVal != valsAndLabels)
            dyStringAppendC(dyLink,',');
        dyStringAppend(dyLink,mdbPairVal(oneVal));
        }
    dyStringPrintf(dyLink,"' title='Click for details of each \"%s\"' TARGET=ucscVocab>%s</A>",title,title);
    }
return dyStringCannibalize(&dyLink);
}

static int filterBoxesForFilesList(char *db,struct mdbObj *mdbObjs,sortOrder_t *sortOrder)
{  // Will create filterBoxes for each sortOrder field.  Returns count of filterBoxes made
int count = 0;
if (sortOrder != NULL)
    {
    struct dyString *dyFilters = dyStringNew(256);
    int sIx=0;
    for(sIx = 0;sIx<sortOrder->count;sIx++)
        {
        char *var = sortOrder->column[sIx];
        enum cvSearchable searchBy = cvSearchMethod(var);
        if (searchBy != cvSearchBySingleSelect && searchBy != cvSearchByMultiSelect)
            continue; // Only single selects and multi-select make good candidates for filtering

        // get all vals for var, then convert to tag/label pairs for filterBys
        struct slName *vals = mdbObjsFindAllVals(mdbObjs, var);
        struct slPair *tagLabelPairs = NULL;
        while(vals != NULL)
            {
            struct slName *term = slPopHead(&vals);
            char *tag = (char *)cvTag(var,term->name);
            if (tag == NULL)
                tag = term->name;
            slPairAdd(&tagLabelPairs,tag,cloneString((char *)cvLabel(term->name)));
            slNameFree(&term);
            }

        // If there is more than one val for this var then create filterBy box for it
        if (slCount(tagLabelPairs) > 1)
            {
            // should have a list sorted on the label
            enum cvDataType eCvDataType = cvDataType(var);
            if (eCvDataType == cvInteger)
                slPairValAtoiSort(&tagLabelPairs);
            else
                slPairValSortCase(&tagLabelPairs);
            char extraClasses[256];
            safef(extraClasses,sizeof extraClasses,"filterTable %s",var);
        #ifdef NEW_JQUERY
            char *dropDownHtml = cgiMakeMultiSelectDropList(var,tagLabelPairs,NULL,"All",extraClasses,"onchange='filterTable(this);' style='font-size:.9em;'");
        #else///ifndef NEW_JQUERY
            char *dropDownHtml = cgiMakeMultiSelectDropList(var,tagLabelPairs,NULL,"All",extraClasses,"onchange='filterTable();' onclick='filterTableExclude(this);'");
        #endif///ndef NEW_JQUERY
            // Note filterBox has classes: filterBy & {var}
            if (dropDownHtml)
                {
                dyStringPrintf(dyFilters,"<td align='left'>\n<B>%s</B>:<BR>\n%s</td><td width=10>&nbsp;</td>\n",
                               labelWithVocabLink(var,sortOrder->title[sIx],tagLabelPairs,TRUE),dropDownHtml);  // TRUE were sending tags, not values
                freeMem(dropDownHtml);
                count++;
                }
            }
        if (slCount(tagLabelPairs) > 0)
            slPairFreeValsAndList(&tagLabelPairs);
        }

    // Finally ready to print the filterBys out
    if (count)
        {
        webIncludeResourceFile("ui.dropdownchecklist.css");
        jsIncludeFile("ui.dropdownchecklist.js",NULL);
        #define FILTERBY_HELP_LINK  "<A HREF=\"../goldenPath/help/multiView.html\" TARGET=ucscHelp>help</A>"
        cgiDown(0.9);
        printf("<B>Filter files by:</B> (select multiple %sitems - %s)\n<table><tr valign='bottom'>\n",
               (count >= 1 ? "categories and ":""),FILTERBY_HELP_LINK);
        printf("%s\n",dyStringContents(dyFilters));
        printf("</tr></table>\n");
    #ifdef NEW_JQUERY
        jsIncludeFile("ddcl.js",NULL);
        printf("<script type='text/javascript'>var newJQuery=true;</script>\n");
    #else///ifndef NEW_JQUERY
        printf("<script type='text/javascript'>var newJQuery=false;</script>\n");
        printf("<script type='text/javascript'>$(document).ready(function() { $('.filterBy').each( function(i) { $(this).dropdownchecklist({ firstItemChecksAll: true, noneIsAll: true, maxDropHeight: filterByMaxHeight(this) });});});</script>\n");
    #endif///ndef NEW_JQUERY
        }
    dyStringFree(&dyFilters);
    }
return count;
}

static void filesDownloadsPreamble(char *db, struct trackDb *tdb)
// Replacement for preamble.html which should expose parent dir, files.txt and supplemental, but
// not have any specialized notes per composite.  Specialized notes belong in track description.
{
char *server = hDownloadsServer();
char *subDir = "";
if (hIsBetaHost())
    {
    server = "hgdownload-test.cse.ucsc.edu"; // NOTE: Force this case because beta may think
    subDir = "/beta";                        // it's downloads server is "hgdownload.cse.ucsc.edu"
    }

cgiDown(0.9);
puts("<B>Data is <A HREF='http://genome.ucsc.edu/ENCODE/terms.html'>RESTRICTED FROM USE</a>");
puts("in publication  until the restriction date noted for the given data file.</B>");

cgiDown(0.7);
puts("Additional resources:");
printf("<BR>&#149;&nbsp;<B><A HREF='http://%s/goldenPath/%s/%s/%s%s/files.txt' TARGET=ucscDownloads>files.txt</A></B> - lists the name and metadata for each download.\n",
                server,db,ENCODE_DCC_DOWNLOADS, tdb->track, subDir);
printf("<BR>&#149;&nbsp;<B><A HREF='http://%s/goldenPath/%s/%s/%s%s/md5sum.txt' TARGET=ucscDownloads>md5sum.txt</A></B> - lists the md5sum output for each download.\n",
                server,db,ENCODE_DCC_DOWNLOADS, tdb->track, subDir);
printf("<BR>&#149;&nbsp;<B><A HREF='http://%s/goldenPath/%s/%s/%s%s'>downloads server</A></B> - alternative access to downloadable files (may include obsolete data).\n",
                server,db,ENCODE_DCC_DOWNLOADS, tdb->track, subDir);

struct fileDb *oneFile = fileDbGet(db, ENCODE_DCC_DOWNLOADS, tdb->track, "supplemental");
if (oneFile != NULL)
    {
    printf("<BR>&#149;&nbsp;<B><A HREF='http://%s/goldenPath/%s/%s/%s%s/supplemental/' TARGET=ucscDownloads>supplemental materials</A></B> - any related files provided by the laboratory.\n",
          server,db,ENCODE_DCC_DOWNLOADS, tdb->track, subDir);
    }
if (hIsPreviewHost())
    printf("<BR><b>WARNING</b>: This data is provided for early access via the Preview Browser -- it is unreviewed and subject to change. For high quality reviewed annotations, see the <a target=_blank href='http://%s/cgi-bin/hgTracks?db=%s'>Genome Browser</a>.",
        "genome.ucsc.edu", db);
else
    printf("<BR><b>NOTE</b>: Early access to additional track data may be available on the <a target=_blank href='http://%s/cgi-bin/hgFileUi?db=%s&g=%s'>Preview Browser</A>.",
        "genome-preview.ucsc.edu", db, tdb->track);
}

static int filesPrintTable(char *db, struct trackDb *parentTdb, struct fileDb *fileList, sortOrder_t *sortOrder,boolean filterable)
// Prints filesList as a sortable table. Returns count
{
// Table class=sortable
int columnCount = 0;
int restrictedColumn = 0;
char *nowrap = (sortOrder->setting != NULL ? " nowrap":""); // Sort order trackDb setting found so rely on <BR> in titles for wrapping
printf("<TABLE class='sortable' style='border: 2px outset #006600;'>\n");
printf("<THEAD class='sortable'>\n");
printf("<TR class='sortable' valign='bottom'>\n");
printf("<TD align='center' valign='center'>&nbsp;");
int filesCount = slCount(fileList);
if (filesCount > 5)
    printf("<em><span class='filesCount'></span>%d files</em>",filesCount);

//if (sortOrder) // NOTE: This could be done to preserve sort order   FIXME: However hgFileUi would need form OR changes would need to be ajaxed over AND hgsid would be needed.
//    printf("<INPUT TYPE=HIDDEN NAME='%s' class='sortOrder' VALUE=\"%s\">",sortOrder->htmlId, sortOrder->sortOrder);
printf("</TD>\n");
columnCount++;

/*#define SHOW_FOLDER_FOR_COMPOSITE_DOWNLOADS
#ifdef SHOW_FOLDER_FOR_COMPOSITE_DOWNLOADS
if (parentTdb == NULL)
    {
    printf("<TD align='center' valign='center'>&nbsp;</TD>");
    columnCount++;
    }
#endif///def SHOW_FOLDER_FOR_COMPOSITE_DOWNLOADS
*/
// Now the columns
int curOrder = 0,ix=0;
if (sortOrder)
    {
    curOrder = sortOrder->count;
    for(ix=0;ix<sortOrder->count;ix++)
        {
        char *align = (sameString("labVersion",sortOrder->column[ix]) || sameString("softwareVersion",sortOrder->column[ix]) ? " align='left'":"");
        printf("<TH class='sortable sort%d%s' %s%s%s>%s</TH>\n",
            sortOrder->order[ix],(sortOrder->forward[ix]?"":" sortRev"),
            (sameString("fileSize",sortOrder->column[ix])?"abbr='use' ":""),
            nowrap,align,sortOrder->title[ix]); // keeing track of sortOrder
        columnCount++;
        if (sameWord(sortOrder->column[ix],"dateUnrestricted"))
            restrictedColumn = columnCount;
        }
    }
//#define INCLUDE_FILENAMES
#ifndef INCLUDE_FILENAMES
else
#endif///defn INCLUDE_FILENAMES
    {
    printf("<TH class='sortable sort%d' nowrap>File Name</TH>\n",++curOrder);
    columnCount++;
    }
printf("<TH class='sortable sort%d' align='left' nowrap>Additional Details</TH>\n",++curOrder);
columnCount++;
printf("</TR></THEAD>\n");

// Now the files...
char *server = hDownloadsServer();
char *subDir = "";
if (hIsBetaHost())
    {
    server = "hgdownload-test.cse.ucsc.edu"; // NOTE: Force this case because beta may think it's downloads server is "hgdownload.cse.ucsc.edu"
    subDir = "/beta";
    }
struct fileDb *oneFile = fileList;
printf("<TBODY class='sortable sorting'>\n"); // 'sorting' is a fib but it conveniently greys the list till the table is initialized.
for( ;oneFile!= NULL;oneFile=oneFile->next)
    {
    oneFile->mdb->next = NULL; // mdbs were in list for generating sortOrder, but list no longer needed
    char *field = NULL;

    printf("<TR valign='top'%s>",filterable?" class='filterable'":"");
    // Download button
    printf("<TD nowrap>");
    if (parentTdb)
        field = parentTdb->track;
    else
        field = mdbObjFindValue(oneFile->mdb,"composite");
    assert(field != NULL);

    printf("<input type='button' value='Download' onclick=\"window.location='http://%s/goldenPath/%s/%s/%s%s/%s';\" title='Download %s ...'>",
          server,db,ENCODE_DCC_DOWNLOADS, field, subDir, oneFile->fileName, oneFile->fileName);

#define SHOW_FOLDER_FRO_COMPOSITE_DOWNLOADS
#ifdef SHOW_FOLDER_FRO_COMPOSITE_DOWNLOADS
    if (parentTdb == NULL)
        printf("&nbsp;<A href='../cgi-bin/hgFileUi?db=%s&g=%s' title='Navigate to downloads page for %s set...'><IMG SRC='../images/folderC.png'></a>&nbsp;", db,field,field);
#endif///def SHOW_FOLDER_FRO_COMPOSITE_DOWNLOADS
    printf("</TD>\n");

    // Each of the pulled out mdb vars
    if (sortOrder)
        {
        for(ix=0;ix<sortOrder->count;ix++)
            {
            char *align = (sameString("labVersion",sortOrder->column[ix]) || sameString("softwareVersion",sortOrder->column[ix]) ? " align='left'":" align='center'");
            if (sameString("fileSize",sortOrder->column[ix]))
                {
                char niceNumber[128];
                sprintWithGreekByte(niceNumber, sizeof(niceNumber), oneFile->fileSize);
                field = oneFile->sortFields[sortOrder->order[ix] - 1];
                printf("<TD abbr='%s' align='right' nowrap>%s</td>",field,niceNumber);
                }
            else
                {
                field = oneFile->sortFields[sortOrder->order[ix] - 1];
                boolean isFieldEmpty = cvTermIsEmpty(sortOrder->column[ix],field);
                char class[128];
                class[0] = '\0';
                if (filterable)
                    {
                    enum cvSearchable searchBy = cvSearchMethod(sortOrder->column[ix]);
                    if (searchBy == cvSearchBySingleSelect || searchBy == cvSearchByMultiSelect)
                        {
                        char *cleanClass = NULL;
                        if (isFieldEmpty)
                            cleanClass = "None";
                        else
                            {
                            cleanClass = (char *)cvTag(sortOrder->column[ix],field); // class should be tag
                            if (cleanClass == NULL)
                                cleanClass = field;
                            }
                        safef(class,sizeof class," class='%s %s'",sortOrder->column[ix],cleanClass);
                        }
                    }

                if (sameString("dateUnrestricted",sortOrder->column[ix]) && field && dateIsOld(field,"%F"))
                    printf("<TD%s nowrap style='color: #BBBBBB;'%s>%s</td>",align,class,field);
                else
                    printf("<TD%s nowrap%s>%s</td>",align,class,isFieldEmpty?" &nbsp;":field);
                if (!sameString("fileType",sortOrder->column[ix]))
                    mdbObjRemoveVars(oneFile->mdb,sortOrder->column[ix]); // Remove this from mdb now so that it isn't displayed in "extras'
                }
            }
        }
#ifndef INCLUDE_FILENAMES
    else
#endif///ndef INCLUDE_FILENAMES
        { // fileName
        printf("<TD nowrap>%s",oneFile->fileName);
        //// FIXME: " The "..." encapsulation could be rebuilt so it could be called here
        //printf("&nbsp;<A HREF='#a_meta_%s' onclick='return metadataShowHide(\"%s\",true,true);' title='Show metadata details...'>...</A>",
        //    oneFile->mdb->obj,oneFile->mdb->obj);
        //printf("<DIV id='div_%s_meta' style='display:none;'></div></td>",oneFile->mdb->obj);
        }

    // Extras  grant=Bernstein; lab=Broad; dataType=ChipSeq; setType=exp; control=std;
    mdbObjRemoveVars(oneFile->mdb,MDB_VAR_FILENAME " " MDB_VAR_FILEINDEX " " MDB_VAR_COMPOSITE " " MDB_VAR_PROJECT); // Remove this from mdb now so that it isn't displayed in "extras'
    field = mdbObjVarValPairsAsLine(oneFile->mdb,TRUE);
    printf("<TD nowrap>%s</td>",field?field:" &nbsp;");

    printf("</TR>\n");
    }

printf("</TBODY><TFOOT class='bgLevel1'>\n");
printf("<TR valign='top'>");

// Restriction policy link in first column?
if (restrictedColumn == 1)
    printf("<TH colspan=%d><A HREF='%s' TARGET=BLANK style='font-size:.9em;'>Restriction Policy</A></TH>", (columnCount - restrictedColumn),ENCODE_DATA_RELEASE_POLICY);

printf("<TD colspan=%d>&nbsp;&nbsp;&nbsp;&nbsp;",(restrictedColumn > 1 ? (restrictedColumn - 1) : columnCount));

// Total
if (filesCount > 5)
    printf("<em><span class='filesCount'></span>%d files</em>",filesCount);

// Restriction policy link in later column?
if (restrictedColumn > 1)
    printf("</TD><TH colspan=%d align='left'><A HREF='%s' TARGET=BLANK style='font-size:.9em;'>Restriction Policy</A>", columnCount,ENCODE_DATA_RELEASE_POLICY);

printf("</TD></TR>\n");
printf("</TFOOT></TABLE>\n");

if (parentTdb == NULL)
    printf("<script type='text/javascript'>{$(document).ready(function() {sortTableInitialize($('table.sortable')[0],true,true);});}</script>\n");

return filesCount;
}


static int filesFindInDir(char *db, struct mdbObj **pmdbFiles, struct fileDb **pFileList, char *fileType, int limit,boolean *exceededLimit)
// Prints list of files in downloads directories matching mdb search terms. Returns count
{
int fileCount = 0;
// Verify file existance and make fileList of those found
struct fileDb *fileList = NULL, *oneFile = NULL; // Will contain found files
struct mdbObj *mdbFiles = NULL; // Will caontain a list of mdbs for the found files
struct mdbObj *mdbList = *pmdbFiles;
while (mdbList && (limit == 0 || fileCount < limit))
    {
    boolean found = FALSE;
    struct mdbObj *mdbFile = slPopHead(&mdbList);
    char *composite = mdbObjFindValue(mdbFile,MDB_VAR_COMPOSITE);
    if (composite == NULL)
        {
        mdbObjsFree(&mdbFile);
        continue;
        }

    // First for FileName
    char *fileName = mdbObjFindValue(mdbFile,MDB_VAR_FILENAME);
    if (fileName == NULL)
        {
        mdbObjsFree(&mdbFile);
        continue;
        }

//#define NO_FILENAME_LISTS
#ifdef NO_FILENAME_LISTS
    oneFile = fileDbGet(db, ENCODE_DCC_DOWNLOADS, composite, fileName);
    if (oneFile == NULL)
        {
        mdbObjsFree(&mdbFile);
        continue;
        }

    //warn("%s == %s",fileType,oneFile->fileType);
    if (isEmpty(fileType) || sameWord(fileType,"Any")
    || (oneFile->fileType && sameWord(fileType,oneFile->fileType)))
        {
        slAddHead(&fileList,oneFile);
        oneFile->mdb = mdbFile;
        slAddHead(&mdbFiles,oneFile->mdb);
        found = TRUE;
        fileCount++;
        if (limit > 0 && fileCount >= limit)
            break;
        }
    else
        fileDbFree(&oneFile);

#else///ifndef NO_FILENAME_LISTS

    struct slName *fileSet = slNameListFromComma(fileName);
    struct slName *md5Set = NULL;
    char *md5sums = mdbObjFindValue(mdbFile,MDB_VAR_MD5SUM);
    if (md5sums != NULL)
        md5Set = slNameListFromComma(md5sums);
    while (fileSet != NULL)
        {
        struct slName *file = slPopHead(&fileSet);
        struct slName *md5 = NULL;
        if (md5Set)
            md5 = slPopHead(&md5Set);
        oneFile = fileDbGet(db, ENCODE_DCC_DOWNLOADS, composite, file->name);
        if (oneFile == NULL)
            {
            slNameFree(&file);
            if (md5)
                slNameFree(&md5);
            continue;
            }

        //warn("%s == %s",fileType,oneFile->fileType);
        if (isEmpty(fileType) || sameWord(fileType,"Any")
        || (oneFile->fileType && startsWithWordByDelimiter(fileType,'.',oneFile->fileType))) // Starts with!  This ensures both bam and bam.bai are found.
            {
            slAddHead(&fileList,oneFile);
            if (found) // if already found then need two mdbObjs (assertable but then this is metadata)
                oneFile->mdb = mdbObjClone(mdbFile);  // Yes clone this as differences will occur
            else
                oneFile->mdb = mdbFile;
            if (md5 != NULL)
                mdbObjSetVar(oneFile->mdb,MDB_VAR_MD5SUM,md5->name);
            else
                mdbObjRemoveVars(oneFile->mdb,MDB_VAR_MD5SUM);
            slAddHead(&mdbFiles,oneFile->mdb);
            found = TRUE;
            fileCount++;
            if (limit > 0 && fileCount >= limit)
                {
                slNameFreeList(&fileSet);
                if (md5Set)
                    slNameFreeList(&md5Set);
                break;
                }
            }
        else
            fileDbFree(&oneFile);

        slNameFree(&file);
        if (md5)
            slNameFree(&md5);
        }
#endif///ndef NO_FILENAME_LISTS

    // FIXME: This support of fileIndex and implicit bam.bai's should be removed when mdb is cleaned up.
    // Now for FileIndexes
    if (limit == 0 || fileCount < limit)
        {
        char buf[512];
        if (strchr(fileName,',') == NULL && endsWith(fileName,".bam")) // Special to fill in missing .bam.bai's
            {
            safef(buf,sizeof(buf),"%s.bai",fileName);
            fileName = buf;
            }
        else
            {
            fileName = mdbObjFindValue(mdbFile,MDB_VAR_FILEINDEX);   // This mdb var should be going away.
            if (fileName == NULL)
                continue;
            }

        // Verify existance first
        oneFile = fileDbGet(db, ENCODE_DCC_DOWNLOADS, composite, fileName);  // NOTE: won't be found if already found in comma delimited fileName!
        if (oneFile == NULL)
            continue;

        //warn("%s == %s",fileType,oneFile->fileType);
        if (isEmpty(fileType) || sameWord(fileType,"Any")
        || (oneFile->fileType && sameWord(fileType,oneFile->fileType))
        || (oneFile->fileType && sameWord(fileType,"bam") && sameWord("bam.bai",oneFile->fileType))) // TODO: put fileType matching into search.c lib code to segregate index logic.
            {
            slAddHead(&fileList,oneFile);
            if (found) // if already found then need two mdbObjs (assertable but then this is metadata)
                oneFile->mdb = mdbObjClone(mdbFile);
            else
                oneFile->mdb = mdbFile;
            mdbObjRemoveVars(oneFile->mdb,MDB_VAR_MD5SUM);
            slAddHead(&mdbFiles,oneFile->mdb);
            fileCount++;
            found = TRUE;
            continue;
            }
        else
            fileDbFree(&oneFile);
        }

    if (!found)
        mdbObjsFree(&mdbFile);
    }
*pmdbFiles = mdbFiles;
*pFileList = fileList;
if (exceededLimit != NULL)
    *exceededLimit = FALSE;
if (mdbList != NULL)
    {
    if (exceededLimit != NULL)
        *exceededLimit = TRUE;
    mdbObjsFree(&mdbList);
    }
return fileCount;
}

void filesDownloadUi(char *db, struct cart *cart, struct trackDb *tdb)
// UI for a "composite like" track: This will list downloadable files associated with
// a single trackDb entry (composite or of type "downloadsOnly". The list of files
// will have links to their download and have metadata information associated.
// The list will be a sortable table and there may be filtering controls.
{
boolean debug = cartUsualBoolean(cart,"debug",FALSE);

struct sqlConnection *conn = hAllocConn(db);
char *mdbTable = mdbTableName(conn,TRUE); // Look for sandBox name first
if(mdbTable == NULL)
    errAbort("TABLE NOT FOUND: '%s.%s'.\n",db,MDB_DEFAULT_NAME);

// Get an mdbObj list of all that belong to this track and have a fileName
char buf[256];
safef(buf,sizeof(buf),"%s=%s %s=?",MDB_VAR_COMPOSITE,tdb->track,MDB_VAR_FILENAME);
struct mdbByVar *mdbVars = mdbByVarsLineParse(buf);
struct mdbObj *mdbList = mdbObjsQueryByVars(conn,mdbTable,mdbVars);

// Now get Indexes  But be sure not to duplicate entries in the list!!!
safef(buf,sizeof(buf),"%s=%s %s= %s!=",MDB_VAR_COMPOSITE,tdb->track,MDB_VAR_FILEINDEX,MDB_VAR_FILENAME);
mdbVars = mdbByVarsLineParse(buf);
mdbList = slCat(mdbList, mdbObjsQueryByVars(conn,mdbTable,mdbVars));
mdbObjRemoveHiddenVars(mdbList);
hFreeConn(&conn);

if (slCount(mdbList) == 0)
    {
    warn("No files specified in metadata for: %s\n%s",tdb->track,tdb->longLabel);
    return;
    }

// Verify file existance and make fileList of those found
struct fileDb *fileList = NULL; // Will contain found files

int fileCount = filesFindInDir(db, &mdbList, &fileList, NULL, 0, NULL);
assert(fileCount == slCount(fileList));

if (fileCount == 0)
    {
    warn("No downloadable files currently available for: %s\n%s",tdb->track,tdb->longLabel);
    return;  // No files so nothing to do.
    }
if (debug)
    {
    warn("The following files are in goldenPath/%s/%s/%s/ but NOT in the mdb:",db,ENCODE_DCC_DOWNLOADS, tdb->track);
    fileDbGet(db, ENCODE_DCC_DOWNLOADS, tdb->track, "listAll");
    }

jsIncludeFile("hui.js",NULL);
jsIncludeFile("ajax.js",NULL);

// standard preamble
filesDownloadsPreamble(db,tdb);

// Now update all files with their sortable fields and sort the list
mdbObjReorderByCv(mdbList,FALSE);// Start with cv defined order for visible vars. NOTE: will not need to reorder during print!
sortOrder_t *sortOrder = fileSortOrderGet(cart,tdb,mdbList);
boolean filterable = FALSE;
if (sortOrder != NULL)
    {
    char *vars = removeCommonMdbVarsNotInSortOrder(mdbList,sortOrder);
    if (vars)
        {
        if (debug)
            warn("These terms are common:%s",vars);
        freeMem(vars);
        }

    // Fill in and sort fileList
    fileDbSortList(&fileList,sortOrder);

    // FilterBoxes
    filterable = (filterBoxesForFilesList(db,mdbList,sortOrder) > 0);
    }

// Print table
filesPrintTable(db,tdb,fileList,sortOrder,filterable);

//fileDbFree(&fileList); // Why bother on this very long running cgi?
//mdbObjsFree(&mdbList);
}

int fileSearchResults(char *db, struct sqlConnection *conn, struct slPair *varValPairs, char *fileType)
// Prints list of files in downloads directories matching mdb search terms. Returns count
{
struct sqlConnection *connLocal = conn;
if (conn == NULL)
    connLocal = hAllocConn(db);
struct mdbObj *mdbList = mdbObjRepeatedSearch(connLocal,varValPairs,FALSE,TRUE);
if (conn == NULL)
    hFreeConn(&connLocal);
if (slCount(mdbList) == 0)
    {
    printf("<DIV id='filesFound'><BR>No files found.<BR></DIV><BR>\n");
    return 0;
    }

// Now sort mdbObjs so that composites will stay together and lookup of files will be most efficient
mdbObjsSortOnVars(&mdbList, MDB_VAR_COMPOSITE);
mdbObjRemoveHiddenVars(mdbList);

#define FOUND_FILE_LIMIT 1000
struct fileDb *fileList = NULL; // Will contain found files
int filesExpected = slCount(mdbList);
boolean exceededLimit = FALSE;
int fileCount = filesFindInDir(db, &mdbList, &fileList, fileType, FOUND_FILE_LIMIT, &exceededLimit);
assert(fileCount == slCount(fileList));

if (fileCount == 0)
    {
    printf("<DIV id='filesFound'><BR>No files found.<BR></DIV><BR>\n");
    return 0;  // No files so nothing to do.
    }

// TODO Could sort on varValPairs by creating a sortOrder struct of them
//// Now update all files with their sortable fields and sort the list
mdbObjReorderByCv(mdbList,FALSE);// Start with cv defined order for visible vars. NOTE: will not need to reorder during print!
sortOrder_t *sortOrder = fileSortOrderGet(NULL,NULL,mdbList); // No cart, no tdb
if (sortOrder != NULL)
    {
    // Fill in and sort fileList
    fileDbSortList(&fileList,sortOrder);
    }

mdbObjRemoveVars(mdbList,"tableName"); // Remove this from mdb now so that it isn't displayed in "extras'

//jsIncludeFile("hui.js",NULL);
//jsIncludeFile("ajax.js",NULL);

// Print table
printf("<DIV id='filesFound'>");
if (exceededLimit)
    {
    // What is the expected count?  Difficult to say because of comma delimited list in fileName.
    if (filesExpected <= FOUND_FILE_LIMIT)
        filesExpected = FOUND_FILE_LIMIT + 1;

    printf("<DIV class='redBox' style='width: 380px;'>Too many files found.  Displaying first %d of at least %d.<BR>Narrow search parameters and try again.</DIV><BR>\n",
           fileCount,filesExpected);
    //warn("Too many files found.  Displaying first %d of at least %d.<BR>Narrow search parameters and try again.\n", fileCount,filesExpected);
    }

fileCount = filesPrintTable(db,NULL,fileList,sortOrder,FALSE); // FALSE=Don't offer more filtering on the file search page
printf("</DIV><BR>\n");

//fileDbFree(&fileList); // Why bother on this very long running cgi?
//mdbObjsFree(&mdbList);

return fileCount;
}

