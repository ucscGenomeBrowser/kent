/* fileUi.c - human genome file downloads common controls. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "trashDir.h"


static boolean timeIt = FALSE; // Can remove when satisfied with timing.

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

static struct fileDb *fileDbReadFromBackup(char *db, char *dir, char *subDir)
{
struct tempName buFile;
boolean exists = trashDirReusableFile(&buFile, dir, subDir, db);  // encodeDCC/composite.db
if (!exists)
    return NULL;

struct fileDb *fileList = NULL;
struct fileDb *oneFile = NULL;
struct lineFile *lf = lineFileOpen(buFile.forCgi, TRUE);
char *words[4];
while (lineFileChop(lf, words) >= 3)
    {
    AllocVar(oneFile);
    oneFile->fileName = cloneString(words[0]);
    oneFile->fileSize = sqlUnsignedLong(words[1]);
    oneFile->fileDate = cloneString(words[2]);
    slAddHead(&fileList,oneFile);
    }
lineFileClose(&lf);
if (fileList == NULL)
    unlink(buFile.forCgi);    // remove empty file

return fileList;
}

// Cache is not faster, so just use it as a backup
#define CACHE_IS_FASTER_THAN_RSYNC
#ifdef CACHE_IS_FASTER_THAN_RSYNC
static boolean fileDbBackupAvailable(char *db, char *dir, char *subDir)
// Checks if there is a recent enough cache file
{
if (cgiVarExists("clearCache")) // Trick to invalidate cache at will.
    return FALSE;

struct tempName buFile;
boolean exists = trashDirReusableFile(&buFile, dir, subDir, db);  // encodeDCC/composite.db
if (exists)
    {
    struct stat mystat;
    ZeroVar(&mystat);
    if (stat(buFile.forCgi,&mystat)==0)
        {
        // how old is old?
        int secs = (clock1() - mystat.st_ctime); // seconds since created
        if (secs < (24 * 60 * 60)) // one date
            return TRUE;
        }
    }
return FALSE;
}
#endif///def CACHE_IS_FASTER_THAN_RSYNC

static void fileDbWriteToBackup(char *db, char *dir, char *subDir,struct fileDb *fileList)
{
struct tempName buFile;
(void)trashDirReusableFile(&buFile, dir, subDir, db);  // encodeDCC/composite.db

FILE *fd = NULL;
if ((fd = fopen(buFile.forCgi, "w")) != NULL)
    {
    struct fileDb *oneFile = fileList;
    for (;oneFile != NULL;oneFile=oneFile->next)
        {
        char buf[1024];
        safef(buf,sizeof buf,"%s %ld %s\n",oneFile->fileName,oneFile->fileSize,oneFile->fileDate);
        fwrite(buf, strlen(buf), 1, fd);
        }
    fclose(fd);
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

#ifdef CACHE_IS_FASTER_THAN_RSYNC
    if (fileDbBackupAvailable(db, dir, subDir))  // check backup first
        foundFiles = fileDbReadFromBackup(db, dir, subDir);
    else
#endif///def CACHE_IS_FASTER_THAN_RSYNC
        {
        FILE *scriptOutput = NULL;
        char buf[1024];
        char cmd[512];
        char *words[10];
        char *server = hDownloadsServer();

        // Always uses rsync, even when same server, to ensure testability
        if (hIsBetaHost())
            {
            // NOTE: Force this case because beta may think it's downloads server is
            //       "hgdownload.soe.ucsc.edu"
            safef(cmd,sizeof(cmd),"rsync -n rsync://hgdownload-test.soe.ucsc.edu/goldenPath/"
                  "%s/%s/%s/beta/",  db, dir, subDir);
            }
        else
            safef(cmd,sizeof(cmd),"rsync -n rsync://%s/goldenPath/%s/%s/%s/",
                  server, db, dir, subDir);

        scriptOutput = popen(cmd, "r");
        while (fgets(buf, sizeof(buf), scriptOutput))
            {
            eraseTrailingSpaces(buf);
            if (!endsWith(buf,".md5sum")) // Just ignore these
                {
                int count = chopLine(buf, words);
                if (count == 5)
                    {
                    //-rw-rw-r-- 26420982 2009/09/29 14:53:30 wgEncodeBroadChipSeq/wgEncode...
                    // rsync 3.1 adds commas:
                    //-rw-rw-r-- 26,420,982 2009/09/29 14:53:30 wgEncodeBroadChipSeq/wgEncode...
                    AllocVar(oneFile);
                    stripChar(words[1], ',');
                    oneFile->fileSize = sqlUnsignedLong(words[1]);
                    oneFile->fileDate = cloneString(words[2]);
                    strSwapChar(oneFile->fileDate,'/','-');// Standardize YYYY-MM-DD, no time
                    oneFile->fileName = cloneString(words[4]);
                    slAddHead(&foundFiles,oneFile);
                    }
                }
            }
        pclose(scriptOutput);
        if (foundFiles == NULL)
            {
            foundFiles = fileDbReadFromBackup(db, dir, subDir);
            if (foundFiles == NULL)
                {
                AllocVar(oneFile);
                oneFile->fileName = cloneString("No files found!");
                oneFile->fileDate = cloneString(cmd);
                slAddHead(&foundFiles,oneFile);
                warn("No files found for command:\n%s",cmd);
                }
            }
        else
            {
            fileDbWriteToBackup(db, dir, subDir,foundFiles);
            if (timeIt)
                uglyTime("Successful rsync found %d files",slCount(foundFiles));
            }
        }

    // mark this as done to avoid excessive io
    savedDb     = cloneString(db);
    savedDir    = cloneString(dir);
    savedSubDir = cloneString(subDir);

    if (foundFiles == NULL)
        return NULL;
    }

// special code that only gets called in debug mode
if (sameString(fileName,"listAll"))
    {
    for (oneFile=foundFiles;oneFile;oneFile=oneFile->next)
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


static sortOrder_t *fileSortOrderGet(struct cart *cart,struct trackDb *parentTdb,
                                     struct mdbObj *mdbObjs)
// Parses 'fileSortOrder' trackDb/cart instructions and returns a sort order struct or NULL.
// Some trickiness here.  sortOrder->sortOrder is from cart (changed by user action),
// as is sortOrder->order, but columns are in original tdb order (unchanging)!
// However, if cart is null, all is from trackDb.ra.
{
int ix;
sortOrder_t *sortOrder = NULL;
char *carveSetting = NULL,*setting = NULL;
if (parentTdb)
    setting = trackDbSetting(parentTdb, FILE_SORT_ORDER);
if (setting)
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
        for (;var != NULL; var = var->next)
            {
            if (differentWord(var->var,MDB_VAR_LAB_VERSION)     // Exclude certain vars
            &&  differentWord(var->var,MDB_VAR_SOFTWARE_VERSION)
            &&  cvSearchMethod(var->var) != cvNotSearchable)   // searchable is also sortable
                {
                if (mdbObjsHasCommonVar(mdbObjs, var->var,TRUE)) // Don't bother if all the vals
                    continue;                                    // are the same (missing okay)
                dyStringPrintf(dySortFields,"%s=%s ",var->var,
                               strSwapChar(cloneString(cvLabel(NULL,var->var)),' ','_'));
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
    if (setting == NULL) // Must be in trackDb or not a sortable list of files
        {
        #define FILE_SORT_ORDER_DEFAULT "cell=Cell_Line lab=Lab view=View replicate=Rep " \
                                "fileSize=Size fileType=File_Type dateSubmitted=Submitted " \
                                "dateUnrestricted=RESTRICTED<BR>Until"
        setting = FILE_SORT_ORDER_DEFAULT;
        }
    sortOrder = needMem(sizeof(sortOrder_t));
    carveSetting = cloneString(setting);
    sortOrder->setting = NULL;
    }
if (parentTdb)
    {
    sortOrder->htmlId = needMem(strlen(parentTdb->track)+20);
    safef(sortOrder->htmlId, (strlen(parentTdb->track)+20), "%s.%s",
          parentTdb->track,FILE_SORT_ORDER);
    if (cart != NULL)
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
        sortOrder->title[ix][0] = '\0';                     // +1 jumps to next char after '='
        sortOrder->title[ix] = strSwapChar(sortOrder->title[ix]+1,'_',' ');
        }
    else
        sortOrder->title[ix] = sortOrder->column[ix];         // or could be just 'cell'

    // Sort order defaults to forward but may be found in a cart var
    sortOrder->order[ix] = ix+1;
    sortOrder->forward[ix] = TRUE;
    if (sortOrder->sortOrder != NULL)
        {
        // find tdb substr in cart current order string
        char *pos = stringIn(sortOrder->column[ix], sortOrder->sortOrder);
        if (pos != NULL && pos[strlen(sortOrder->column[ix])] == '=')
            {
            int ord=1;
            char* pos2 = sortOrder->sortOrder;
            for (;*pos2 && pos2 < pos;pos2++)
                {
                if (*pos2 == '=') // Discovering sort order in cart
                    ord++;
                }
            sortOrder->forward[ix] = (pos[strlen(sortOrder->column[ix]) + 1] == '+');
            sortOrder->order[ix] = ord;
            }
        }
    }
if (sortOrder->sortOrder == NULL)
    sortOrder->sortOrder = cloneString(setting);      // no order in cart, all power to trackDb
return sortOrder;  // NOTE cloneString:words[0]==*sortOrder->column[0]
}                  // will be freed when sortOrder is freed

static int fileDbSortCmp(const void *va, const void *vb)
// Compare two sortable tdb items based upon sort columns.
{
const struct fileDb *a = *((struct fileDb **)va);
const struct fileDb *b = *((struct fileDb **)vb);
char **fieldsA = a->sortFields;
char **fieldsB = b->sortFields;
int ix=0;
int compared = 0;
while (fieldsA[ix] != NULL && fieldsB[ix] != NULL)
    {
    compared = strcmp(fieldsA[ix], fieldsB[ix]);
    if (compared != 0)
        {
        if (a->reverse[ix])
            compared *= -1;
        break;
        }
    ix++;
    }
return compared;
}

static void fileDbSortList(struct fileDb **fileList, sortOrder_t *sortOrder)
// If sortOrder struct provided, will update sortFields in fileList, then sort it
{
if (sortOrder && fileList)
    {
    struct fileDb *oneFile = NULL;
    for (oneFile = *fileList;oneFile != NULL;oneFile=oneFile->next)
        {                                                                // + 1 Null terminated
        oneFile->sortFields = needMem(sizeof(char *)    * (sortOrder->count + 1));
        oneFile->reverse    = needMem(sizeof(boolean *) *  sortOrder->count);
        int ix;
        for (ix=0;ix<sortOrder->count;ix++)
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
            }
        oneFile->sortFields[sortOrder->count] = NULL;
        }
    slSort(fileList,fileDbSortCmp);
    }
}

static int removeCommonMdbVarsNotInSortOrder(struct mdbObj *mdbObjs,sortOrder_t *sortOrder)
// Removes varaibles common to all mdbObjs and not found in sortOrder.
// Returns count of vars removed
{
if (sortOrder != NULL)
    {
    // Remove common vars from mdbs grant=Bernstein; lab=Broad; dataType=ChipSeq; setType=exp;
    // However, keep the term if it is in the sortOrder
    int count = 0;
    struct dyString *dyCommon = dyStringNew(256);
    char *commonTerms[] = { "grant", "lab", "dataType", "control", "setType" };
    int tIx=0,sIx = 0;
    for (;tIx<ArraySize(commonTerms);tIx++)
        {
        for (sIx = 0;
             sIx<sortOrder->count && differentString(commonTerms[tIx],sortOrder->column[sIx]);
             sIx++) ;
        if (sIx<sortOrder->count) // Found in sort Order so leave it in mdbObjs
            continue;

        if (mdbObjsHasCommonVar(mdbObjs, commonTerms[tIx], TRUE)) // val the same or missing
            {
            count++;
            dyStringPrintf(dyCommon,"%s ",commonTerms[tIx]);
            }
        }
    if (count > 0)
        mdbObjRemoveVars(mdbObjs,dyStringContents(dyCommon)); // removes from full list of mdbs
    dyStringFree(&dyCommon);
    return count;
    }
return 0;
}

static char *labelWithVocabLink(char *var,char *title,struct slPair *valsAndLabels,
                                boolean tagsNotVals)
// If the parentTdb has a controlledVocabulary setting and the vocabType is found,
// then label will be wrapped with the link to all relevent terms.  Return string is cloned.
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
    dyStringPrintf(dyLink,"<A HREF='hgEncodeVocab?type=%s' title='Click for details of \"%s\"'"
                          " TARGET=ucscVocab>%s</A>",var,title,title);
else
    {
    dyStringPrintf(dyLink,"<A HREF='hgEncodeVocab?%s=",tagsNotVals?"tag":"term");
    struct slPair *oneVal = valsAndLabels;
    for (;oneVal!=NULL;oneVal=oneVal->next)
        {
        if (oneVal != valsAndLabels)
            dyStringAppendC(dyLink,',');
        dyStringAppend(dyLink,mdbPairVal(oneVal));
        }
    dyStringPrintf(dyLink,"' title='Click for details of each \"%s\"' TARGET=ucscVocab>%s</A>",
                          title,title);
    }
return dyStringCannibalize(&dyLink);
}

static int filterBoxesForFilesList(char *db,struct mdbObj *mdbObjs,sortOrder_t *sortOrder)
// Will create filterBoxes for each sortOrder field.  Returns bitmask of sortOrder colums included
{
int count = 0;
int filterableBits = 0;
if (sortOrder != NULL)
    {
    struct dyString *dyFilters = dyStringNew(256);
    int sIx=0;
    for (sIx = 0;sIx<sortOrder->count;sIx++)
        {
        char *var = sortOrder->column[sIx];
        enum cvSearchable searchBy = cvSearchMethod(var);
        if (searchBy == cvNotSearchable || searchBy == cvSearchByFreeText)
            continue; // Free text is not good candidate for filters.  Best is single word/date/int.

        // get all vals for var, then convert to tag/label pairs for filterBys
        struct slName *vals = mdbObjsFindAllVals(mdbObjs, var, CV_LABEL_EMPTY_IS_NONE);
        if (searchBy != cvSearchByMultiSelect && searchBy != cvSearchBySingleSelect)
            {
            // We can't be too ambitious about creating filterboxes on the fly so some limitations:
            // If there are more than 80 options, the filterBy is way too large and of limited use
            // If there is a distinct val for each file in the table, then the filterBy is the
            //    same size as the table and of no help.  Really the number of options should be
            //    half the number of rows but we are being lenient and cutting off at 0.8 not 0.5
            // If there is any non-alphanum char in a value then the filterBy will fail in js code.
            //    Those filterBy's are abandoned a but further down.
            int valCount = slCount(vals);
            if (valCount > 80 || valCount > (slCount(mdbObjs) * 0.8))
                {
                slNameFreeList(&vals);
                continue;
                }
            }
        struct slPair *tagLabelPairs = NULL;
        while (vals != NULL)
            {
            char buf[256];
            struct slName *term = slPopHead(&vals);
            char *tag = (char *)cvTag(var,term->name);
            if (tag == NULL)                           // Does not require cv defined!
                {
                safecpy(buf,sizeof buf,term->name);
                tag = buf;               // Bad news if tag has special chars,
                eraseNonAlphaNum(tag);   // unfortunately this does not pretect us from dups
                // filtering by terms not in cv or regularized should be
                // abandanded at the first sign of trouble!
                if (searchBy != cvSearchByMultiSelect
                &&  searchBy != cvSearchBySingleSelect
                &&  searchBy != cvSearchByDateRange)
                    {
                    if (strlen(term->name) > strlen(tag))
                        {
                        slNameFreeList(&vals);
                        slNameFree(&term);
                        break;
                        }
                    }
                }
            slPairAdd(&tagLabelPairs,tag,cloneString((char *)cvLabel(var,term->name)));
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
            char *dropDownHtml = cgiMakeMultiSelectDropList(var,tagLabelPairs,NULL,"All",
                      extraClasses, "change", "filterTable.filter(this);", "font-size:.9em;", NULL);
            // Note filterBox has classes: filterBy & {var}
            if (dropDownHtml)
                {
                dyStringPrintf(dyFilters,
                             "<td align='left'>\n<B>%s</B>:<BR>\n%s</td><td width=10>&nbsp;</td>\n",
                             labelWithVocabLink(var,sortOrder->title[sIx],tagLabelPairs,TRUE),
                             dropDownHtml);  // TRUE were sending tags, not values
                freeMem(dropDownHtml);
                count++;
                if (sIx < 32) // avoid bit overflow but 32 filterBoxes?  I don't think so
                    filterableBits |= (0x1<<(sIx));
                }
            }
        if (tagLabelPairs != NULL)
            slPairFreeValsAndList(&tagLabelPairs);
        }

    // Finally ready to print the filterBys out
    if (count)
        {
        webIncludeResourceFile("ui.dropdownchecklist.css");
        jsIncludeFile("ui.dropdownchecklist.js",NULL);
        #define FILTERBY_HELP_LINK  "<A HREF=\"../goldenPath/help/multiView.html\" " \
                                    "TARGET=ucscHelp>help</A>"
        cgiDown(0.9);
        printf("<B>Filter files by:</B> (select multiple %sitems - %s)\n"
               "<table><tr valign='bottom'>\n",
               (count >= 1 ? "categories and ":""),FILTERBY_HELP_LINK);
        printf("%s\n",dyStringContents(dyFilters));
        printf("</tr></table>\n");
        jsIncludeFile("ddcl.js",NULL);
        }
    dyStringFree(&dyFilters);
    }
return filterableBits;
}

static void filesDownloadsPreamble(char *db, struct trackDb *tdb, boolean isUnrestricted)
// Replacement for preamble.html which should expose parent dir, files.txt and supplemental, but
// not have any specialized notes per composite.  Specialized notes belong in track description.
{
char *server = hDownloadsServer();
char *subDir = "";
if (hIsBetaHost())
    {
    server = "hgdownload-test.soe.ucsc.edu"; // NOTE: Force this case because beta may think
    subDir = "/beta";                        // it's downloads server is "hgdownload.soe.ucsc.edu"
    }
if (!isUnrestricted)
    {
    cgiDown(0.9);
    puts("<B>Data is <A HREF='../ENCODE/terms.html' TARGET='_BLANK'>RESTRICTED FROM USE</a>");
    puts("in publication  until the restriction date noted for the given data file.</B>");
}
cgiDown(0.7);
puts("Additional resources:");
printf("<BR>&#149;&nbsp;<B><A HREF='http://%s/goldenPath/%s/%s/%s%s/files.txt' "
       "TARGET=ucscDownloads>files.txt</A></B> - lists the name and metadata for each download.\n",
       server,db,ENCODE_DCC_DOWNLOADS, tdb->track, subDir);
printf("<BR>&#149;&nbsp;<B><A HREF='http://%s/goldenPath/%s/%s/%s%s/md5sum.txt' "
       "TARGET=ucscDownloads>md5sum.txt</A></B> - lists the md5sum output for each download.\n",
       server,db,ENCODE_DCC_DOWNLOADS, tdb->track, subDir);
printf("<BR>&#149;&nbsp;<B><A HREF='http://%s/goldenPath/%s/%s/%s%s'>downloads server</A></B> - "
       "alternative access to downloadable files (may include obsolete data).\n",
       server,db,ENCODE_DCC_DOWNLOADS, tdb->track, subDir);

struct fileDb *oneFile = fileDbGet(db, ENCODE_DCC_DOWNLOADS, tdb->track, "supplemental");
if (oneFile != NULL)
    {
    printf("<BR>&#149;&nbsp;<B><A HREF='http://%s/goldenPath/%s/%s/%s%s/supplemental/' "
           "TARGET=ucscDownloads>supplemental materials</A></B> - "
           "any related files provided by the laboratory.\n",
           server,db,ENCODE_DCC_DOWNLOADS, tdb->track, subDir);
    }
}

static int filesPrintTable(char *db, struct trackDb *parentTdb, struct fileDb *fileList,
                           sortOrder_t *sortOrder,int filterable)
// Prints filesList as a sortable table. Returns count
{
if (timeIt)
    uglyTime("Start table");
// Table class=sortable
int columnCount = 0;
int butCount = 0;
int restrictedColumn = 0;
char *nowrap = (sortOrder->setting != NULL ? " nowrap":"");
      // Sort order trackDb setting found so rely on <BR> in titles for wrapping
printf("<TABLE class='sortable' style='border: 2px outset #006600;'>\n");
printf("<THEAD class='sortable'>\n");
printf("<TR class='sortable' valign='bottom'>\n");
printf("<TD align='center' valign='center'>&nbsp;");
int filesCount = slCount(fileList);
if (filesCount > 5)
    printf("<em><span class='filesCount'></span>%d files</em>",filesCount);

// NOTE: This could be done to preserve sort order in cart.
//       However hgFileUi would need form OR changes would need to be ajaxed over
//       AND hgsid would be needed.
//if (sortOrder)
//    printf("<INPUT TYPE=HIDDEN NAME='%s' class='sortOrder' VALUE=\"%s\">",
//           sortOrder->htmlId, sortOrder->sortOrder);
printf("</TD>\n");
columnCount++;

// Now the columns
int curOrder = 0,ix=0;
if (sortOrder)
    {
    curOrder = sortOrder->count;
    for (ix=0;ix<sortOrder->count;ix++)
        {
        char *align = (sameString("labVersion",sortOrder->column[ix])
                    || sameString("softwareVersion",sortOrder->column[ix]) ? " align='left'":"");
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
    server = "hgdownload-test.soe.ucsc.edu"; // NOTE: Force this case because beta may think
    subDir = "/beta";                        // it's downloads server is "hgdownload.soe.ucsc.edu"
    }
struct fileDb *oneFile = fileList;
printf("<TBODY class='sortable sorting'>\n"); // 'sorting' is a fib but it conveniently greys
if (timeIt)                                   // the list till the table is initialized.
    uglyTime("Finished column headers");
for (;oneFile!= NULL;oneFile=oneFile->next)
    {
    oneFile->mdb->next = NULL; // mdbs were in list for generating sortOrder,
    char *field = NULL;        // but list no longer needed

    printf("<TR valign='top'%s>",(filterable != 0) ?" class='filterable'":"");
    // Download button
    printf("<TD nowrap>");
    if (parentTdb)
        field = parentTdb->track;
    else
        {
        field = cloneString(mdbObjFindValue(oneFile->mdb,MDB_VAR_COMPOSITE));
        mdbObjRemoveOneVar(oneFile->mdb,MDB_VAR_COMPOSITE,NULL);
        }
    assert(field != NULL);

    char id[256];
    safef(id, sizeof id, "ftpBut_%d", butCount++);
    printf("<input type='button' id='%s' value='Download' title='Download %s ...'>", id, oneFile->fileName);
    jsOnEventByIdF("click", id, "window.location='http://%s/goldenPath/%s/%s/%s%s/%s';"
           ,server,db,ENCODE_DCC_DOWNLOADS, field, subDir, oneFile->fileName);

#define SHOW_FOLDER_FRO_COMPOSITE_DOWNLOADS
#ifdef SHOW_FOLDER_FRO_COMPOSITE_DOWNLOADS
    if (parentTdb == NULL)
        printf("&nbsp;<A href='../cgi-bin/hgFileUi?db=%s&g=%s' title='Navigate to downloads "
               "page for %s set...'><IMG SRC='../images/folderC.png'></a>&nbsp;", db,field,field);
#endif///def SHOW_FOLDER_FRO_COMPOSITE_DOWNLOADS
    printf("</TD>\n");

    // Each of the pulled out mdb vars
    if (sortOrder)
        {
        for (ix=0;ix<sortOrder->count;ix++)
            {
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

                boolean isFieldEmpty = isEmpty(field);
                struct hash *termHash = NULL;
                if (!isFieldEmpty)
                    {
                    termHash = (struct hash *)cvOneTermHash(sortOrder->column[ix],field);
                    if (termHash && sameString(field,MDB_VAL_ENCODE_EDV_NONE))
                        isFieldEmpty = cvTermIsEmpty(sortOrder->column[ix],field);
                    }
                char class[128];
                class[0] = '\0';

                if (filterable & (0x1<<ix))
                    {
                    char *cleanClass = NULL;
                    char buf[256];
                    if (isFieldEmpty)
                        cleanClass = CV_LABEL_EMPTY_IS_NONE;
                    else
                        {
                        if (termHash)
                            cleanClass = (char *)hashFindVal((struct hash *)termHash,CV_TAG);
                        if (cleanClass == NULL)
                            {
                            // This may not be needed because the filterBy code
                            // already eliminated these.
                            safecpy(buf,sizeof buf,field);
                            cleanClass = buf;
                            eraseNonAlphaNum(cleanClass);
                            }
                        }
                    safef(class,sizeof class," class='%s %s'",sortOrder->column[ix],cleanClass);
                    }

                char *align = (sameString("labVersion",sortOrder->column[ix])
                            || sameString("softwareVersion",sortOrder->column[ix]) ?
                               " align='left'":" align='center'");
                if (sameString("dateUnrestricted",sortOrder->column[ix])
                &&  field
                &&  dateIsOld(field, MDB_ENCODE_DATE_FORMAT))
                    printf("<TD%s nowrap style='color: #BBBBBB;'%s>%s</td>",align,class,field);
                else
                    {
                    // use label
                    if (!isFieldEmpty && termHash)
                        {
                        char *label = hashFindVal(termHash,CV_LABEL);
                        if (label != NULL)
                            field = label;
                        }
                    printf("<TD%s nowrap%s>%s</td>",align,class,isFieldEmpty?" &nbsp;":field);
                    }
                if (!sameString("fileType",sortOrder->column[ix]))
                    mdbObjRemoveOneVar(oneFile->mdb,sortOrder->column[ix],NULL);
                }
            }
        }
#ifndef INCLUDE_FILENAMES
    else
#endif///ndef INCLUDE_FILENAMES
        { // fileName
        printf("<TD nowrap>%s",oneFile->fileName);
        }

    // Extras  grant=Bernstein; lab=Broad; dataType=ChipSeq; setType=exp; control=std;
    field = mdbObjVarValPairsAsLine(oneFile->mdb,TRUE,FALSE);
    printf("<TD nowrap>%s</td>",field?field:" &nbsp;");

    printf("</TR>\n");
    }
if (timeIt)
    uglyTime("Finished files");

printf("</TBODY><TFOOT class='bgLevel1'>\n");
printf("<TR valign='top'>");

// Restriction policy link in first column?
if (restrictedColumn == 1)
    printf("<TH colspan=%d><A HREF='%s' TARGET=BLANK style='font-size:.9em;'>"
           "Restriction Policy</A></TH>", (columnCount - restrictedColumn),
           ENCODE_DATA_RELEASE_POLICY);

printf("<TD colspan=%d>&nbsp;&nbsp;&nbsp;&nbsp;",
       (restrictedColumn > 1 ? (restrictedColumn - 1) : columnCount));

// Total
if (filesCount > 5)
    printf("<em><span class='filesCount'></span>%d files</em>",filesCount);

// Restriction policy link in later column?
if (restrictedColumn > 1)
    printf("</TD><TH colspan=%d align='left'><A HREF='%s' TARGET=BLANK style='font-size:.9em;'>"
           "Restriction Policy</A>", columnCount,ENCODE_DATA_RELEASE_POLICY);

printf("</TD></TR>\n");
printf("</TFOOT></TABLE>\n");

if (parentTdb == NULL)
    jsInline("$(document).ready(function() {"
           "sortTable.initialize($('table.sortable')[0],true,true);});\n");

if (timeIt)
    uglyTime("Finished table");
return filesCount;
}


static int filesFindInDir(char *db, struct mdbObj **pmdbFiles, struct fileDb **pFileList,
                      char *fileType, int limit, boolean *exceededLimit, boolean *isUnrestricted)
// Prints list of files in downloads directories matching mdb search terms. Returns count
{
int fileCount = 0;
if (isUnrestricted != NULL)
    *isUnrestricted = TRUE;

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

    // Are any files still restricted access under ENCODE data policy ?
    if (isUnrestricted != NULL && *isUnrestricted)
        *isUnrestricted = mdbObjEncodeIsUnrestricted(mdbFile);

    struct slName *fileSet = slNameListFromComma(fileName);
    struct slName *md5Set = NULL;
    char *md5sums = mdbObjFindValue(mdbFile,MDB_VAR_MD5SUM);
    if (md5sums != NULL)
        md5Set = slNameListFromComma(md5sums);

    // Could be that "bai" is implicit with "bam"
    if ((slCount(fileSet) == 1) && endsWith(fileSet->name,".bam"))
        {
        char buf[512];
        safef(buf,sizeof(buf),"%s.bai",fileSet->name);
        slNameAddTail(&fileSet, buf);
        }
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
        if (isEmpty(fileType)
        || sameWord(fileType,"Any")
        || (   oneFile->fileType
            && startsWithWordByDelimiter(fileType,'.',oneFile->fileType)))
               // Starts with!  This ensures both bam and bam.bai are found.
            {
            slAddHead(&fileList,oneFile);
            if (found) // if already found then need two mdbObjs (assertable but this is metadata)
                oneFile->mdb = mdbObjClone(mdbFile);  // Yes clone this as differences will occur
            else
                oneFile->mdb = mdbFile;
            if (md5 != NULL)
                mdbObjSetVar(oneFile->mdb,MDB_VAR_MD5SUM,md5->name);
            else
                mdbObjRemoveOneVar(oneFile->mdb,MDB_VAR_MD5SUM,NULL);
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

    // FIXME: This support of fileIndex should be removed when mdb is cleaned up.
    // Now for FileIndexes
    if (limit == 0 || fileCount < limit)
        {
        fileName = mdbObjFindValue(mdbFile,MDB_VAR_FILEINDEX);// This mdb var should be going away.
        if (fileName == NULL)
            continue;

        // Verify existance first
        oneFile = fileDbGet(db, ENCODE_DCC_DOWNLOADS, composite, fileName);
        if (oneFile == NULL) // NOTE: won't be found if already found in comma delimited fileName!
            continue;

        if (isEmpty(fileType) || sameWord(fileType,"Any")
        || (oneFile->fileType && sameWord(fileType,oneFile->fileType))
        || (oneFile->fileType && sameWord(fileType,"bam") && sameWord("bam.bai",oneFile->fileType)))
            {    // TODO: put fileType matching into search.c lib code to segregate index logic.
            slAddHead(&fileList,oneFile);
            if (found) // if already found then need two mdbObjs (assertable but this is metadata)
                oneFile->mdb = mdbObjClone(mdbFile);
            else
                oneFile->mdb = mdbFile;
            mdbObjRemoveOneVar(oneFile->mdb,MDB_VAR_MD5SUM,NULL);
            slAddHead(&mdbFiles,oneFile->mdb);
            fileCount++;
            found = TRUE;
            continue;
            }
        else
            fileDbFree(&oneFile);
        }
    // FIXME: This support of fileIndex should be removed when mdb is cleaned up.

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
timeIt = cartUsualBoolean(cart, "measureTiming",FALSE); // static to file
if (timeIt)
    uglyTime("Starting file search");

boolean debug = cartUsualBoolean(cart,"debug",FALSE);

struct sqlConnection *conn = hAllocConn(db);
char *mdbTable = mdbTableName(conn,TRUE); // Look for sandBox name first
if (mdbTable == NULL)
    errAbort("TABLE NOT FOUND: '%s.%s'.\n",db,MDB_DEFAULT_NAME);

// Get an mdbObj list of all that belong to this track and have a fileName
char buf[256];
safef(buf,sizeof(buf),"%s=%s %s=?",MDB_VAR_COMPOSITE,tdb->track,MDB_VAR_FILENAME);
struct mdbByVar *mdbVars = mdbByVarsLineParse(buf);
struct mdbObj *mdbList = mdbObjsQueryByVars(conn,mdbTable,mdbVars);

// Now get Indexes  But be sure not to duplicate entries in the list!!!
safef(buf,sizeof(buf),"%s=%s %s= %s!=",MDB_VAR_COMPOSITE,tdb->track,
      MDB_VAR_FILEINDEX,MDB_VAR_FILENAME);
mdbVars = mdbByVarsLineParse(buf);
mdbList = slCat(mdbList, mdbObjsQueryByVars(conn,mdbTable,mdbVars));
mdbObjRemoveHiddenVars(mdbList);
hFreeConn(&conn);

if (mdbList)
    (void)mdbObjsFilter(&mdbList,"objStatus","re*",TRUE); // revoked, replaced, renamed

if (slCount(mdbList) == 0)
    {
    warn("No files specified in metadata for: %s\n%s",tdb->track,tdb->longLabel);
    return;
    }
if (timeIt)
    uglyTime("Found %d mdb objects",slCount(mdbList));

// Verify file existance and make fileList of those found
struct fileDb *fileList = NULL; // Will contain found files

boolean isUnrestricted;
int fileCount = filesFindInDir(db, &mdbList, &fileList, NULL, 0, NULL, &isUnrestricted);
if (timeIt)
    uglyTime("Found %d files in dir",fileCount);
assert(fileCount == slCount(fileList));

if (fileCount == 0)
    {
    warn("No downloadable files currently available for: %s\n%s",tdb->track,tdb->longLabel);
    return;  // No files so nothing to do.
    }
if (debug)
    {
    warn("The following files are in goldenPath/%s/%s/%s/ but NOT in the mdb:",
         db,ENCODE_DCC_DOWNLOADS, tdb->track);
    fileDbGet(db, ENCODE_DCC_DOWNLOADS, tdb->track, "listAll");
    }

jsIncludeFile("hui.js",NULL);
jsIncludeFile("ajax.js",NULL);

// standard preamble
filesDownloadsPreamble(db,tdb, isUnrestricted);

// remove these now to get them out of the way
mdbObjRemoveVars(mdbList,MDB_VAR_FILENAME " " MDB_VAR_FILEINDEX " "
                         MDB_VAR_COMPOSITE " " MDB_VAR_PROJECT);
if (timeIt)
    uglyTime("<BR>Removed 4 unwanted vars");

// Now update all files with their sortable fields and sort the list
mdbObjReorderByCv(mdbList,FALSE);// Start with cv defined order for visible vars.
                                 // NOTE: will not need to reorder during print!
sortOrder_t *sortOrder = fileSortOrderGet(cart,tdb,mdbList);
int filterable = 0;
if (sortOrder != NULL)
    {
    int removed = removeCommonMdbVarsNotInSortOrder(mdbList,sortOrder);
    if (removed && debug)
        warn("%d terms are common and were removed",removed);
    if (timeIt)
        uglyTime("Removed %d common vars",removed);

    // Fill in and sort fileList
    fileDbSortList(&fileList,sortOrder);
    if (timeIt)
        uglyTime("Sorted %d files on %d columns",fileCount,sortOrder->count);

    // FilterBoxes
    filterable = filterBoxesForFilesList(db,mdbList,sortOrder);
    if (timeIt && filterable)
        uglyTime("Created filter boxes");
    }

// Print table
filesPrintTable(db,tdb,fileList,sortOrder,filterable);

//fileDbFree(&fileList); // Why bother on this very long running cgi?
//mdbObjsFree(&mdbList);
}

int fileSearchResults(char *db, struct sqlConnection *conn, struct cart *cart,
                      struct slPair *varValPairs, char *fileType)
// Prints list of files in downloads directories matching mdb search terms. Returns count
{
timeIt = cartUsualBoolean(cart, "measureTiming",FALSE); // static to file
if (timeIt)
    uglyTime("Starting file search");

struct sqlConnection *connLocal = conn;
if (conn == NULL)
    connLocal = hAllocConn(db);
struct mdbObj *mdbList = mdbObjRepeatedSearch(connLocal,varValPairs,FALSE,TRUE);
if (conn == NULL)
    hFreeConn(&connLocal);

mdbObjRemoveHiddenVars(mdbList);
if (mdbList)
    (void)mdbObjsFilter(&mdbList,"objStatus","re*",TRUE); // revoked, replaced, renamed

if (slCount(mdbList) == 0)
    {
    printf("<DIV id='filesFound'><BR>No files found.<BR></DIV><BR>\n");
    return 0;
    }
if (timeIt)
    uglyTime("Found %d mdb objects",slCount(mdbList));

// Now sort mdbObjs so that composites will stay together & lookup of files will be most efficient
mdbObjsSortOnVars(&mdbList, MDB_VAR_COMPOSITE);

#define FOUND_FILE_LIMIT 1000
struct fileDb *fileList = NULL; // Will contain found files
int filesExpected = slCount(mdbList);
boolean exceededLimit = FALSE;
int fileCount = filesFindInDir(db, &mdbList, &fileList, fileType, FOUND_FILE_LIMIT, 
                                &exceededLimit, NULL);
if (timeIt)
    uglyTime("Found %d files in dir",fileCount);
assert(fileCount == slCount(fileList));

if (fileCount == 0)
    {
    printf("<DIV id='filesFound'><BR>No files found.<BR></DIV><BR>\n");
    return 0;  // No files so nothing to do.
    }

// remove these now to get them out of the way
mdbObjRemoveVars(mdbList,MDB_VAR_FILENAME " " MDB_VAR_FILEINDEX " "
                         MDB_VAR_PROJECT " " MDB_VAR_TABLENAME);
if (timeIt)
    uglyTime("Removed 4 unwanted vars");

// Now update all files with their sortable fields and sort the list
mdbObjReorderByCv(mdbList,FALSE);// Start with cv defined order for visible vars.
                                 // NOTE: will not need to reorder during print!
sortOrder_t *sortOrder = fileSortOrderGet(NULL,NULL,mdbList); // No cart, no tdb
if (sortOrder != NULL)
    {
    // Fill in and sort fileList
    fileDbSortList(&fileList,sortOrder);
    if (timeIt)
        uglyTime("Sorted %d files on %d columns",fileCount,sortOrder->count);
    }

// Print table
printf("<DIV id='filesFound'>");
if (exceededLimit)
    {
    // What is the expected count?  Difficult to say because of comma delimited list in fileName.
    if (filesExpected <= FOUND_FILE_LIMIT)
        filesExpected = FOUND_FILE_LIMIT + 1;

    printf("<DIV class='redBox' style='width: 380px;'>Too many files found.  Displaying first "
           "%d of at least %d.<BR>Narrow search parameters and try again.</DIV><BR>\n",
           fileCount,filesExpected);
    }
                                                    // 0=No columns 'filtered' on file search page
fileCount = filesPrintTable(db,NULL,fileList,sortOrder,0);
printf("</DIV><BR>\n");

//fileDbFree(&fileList); // Why bother on this very long running cgi?
//mdbObjsFree(&mdbList);

return fileCount;
}

