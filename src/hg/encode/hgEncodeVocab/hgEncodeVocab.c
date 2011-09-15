/* hgEncodeVocab - print table of controlled vocabulary from ENCODE configuration files */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "htmshell.h"
#include "ra.h"
#include "hui.h"
#include "cv.h"
#include "web.h"
#include "jsHelper.h"

/* hgEncodeVocab - A CGI script to display the different types of encode controlled vocabulary.
 * usage:
 *   hgEncodeVocab type=[Antibody|"Cell Line"|localization|rnaExtract|"Gene Type"] [tier=(1|2|3)]
 * options:\n"
 *    type=TypeName  : Type to display
 *    tier=N         : If type="Cell Line" then this is the tier to display
 *    bgcolor=RRGGBB : Change background color (hex digits)
 *    term=a[,b,c]   : Display row for a single term [or comma delimited set of terms]
 *    tag=a[,b,c]    : Display row for a single term, using tag as identifier [or comma delimited set of tags]
 *    target=a[,b,c] : Display all antibodies for a single target.  If 'a'[,b,c] is a term, corresponding targets will be looked up and used
 *    label=a[,b,c]  : Display row for a single term with the specific label.  Must use with 'type' or terms must have same type.
 */

//options that apply to all vocab types

#define ORGANISM           "organism"
#define ORG_HUMAN          "human"
#define ORG_MOUSE          "mouse"

#define MAX_TABLE_COLS     10
#define TABLE_COLS_AVAILABLE(rowsUsed) (MAX_TABLE_COLS - (rowsUsed))

static char *termOpt = NULL;
static char *tagOpt = NULL;
static char *typeOpt = NULL;
static char *targetOpt = NULL;
static char *labelOpt = NULL;
static char *organismOpt = NULL; // we default to human if nothing else is set
static char *organismOptLower = NULL; //  version of above used for path names

void documentLink(struct hash *ra, char *term, char *docTerm,char *dir,char *title,boolean genericDoc)
// Compare controlled vocab based on term value
{
char *s;
if(title == NULL)
    title = docTerm;

// add links to protocol doc if it exists
char docUrl[PATH_LEN];
char docFile[PATH_LEN];
// parse setting
s = hashFindVal(ra,docTerm);
if(s != NULL && differentWord(s,"missing"))
    {
    char *docSetting = cloneString(s);
    char *settings=docSetting;
    int count=0;
    while((s = nextWord(&settings)) != NULL)
        {
        char *docTitle = NULL;
        char *fileName = NULL;
        if(strchr(s,':')) // lab Specific setting
            {
            docTitle = strSwapChar(s,':',0);
            fileName = docTitle + strlen(docTitle) + 1;
            }
        else
            {
            docTitle = title;
            fileName = s;
            }
        safef(docUrl,  sizeof(docUrl),  "%s%s", dir, fileName);
        safef(docFile, sizeof(docFile), "%s%s", hDocumentRoot(), docUrl);
        if (count>0)
            printf("<BR>");
        count++;
        docTitle = htmlEncodeText(strSwapChar(docTitle,'_',' '),FALSE);
        printf(" <A TARGET=_BLANK HREF=%s>%s</A>\n", docUrl,docTitle);
        freeMem(docTitle);
        }
    freeMem(docSetting);
    }
else if(genericDoc)
    { // generate a standard name
    safef(docUrl,  sizeof(docUrl),  "%s%s_protocol.pdf", dir, term);
    safef(docFile, sizeof(docFile), "%s%s", hDocumentRoot(), docUrl);
    if (fileExists(docFile))
        printf(" <A TARGET=_BLANK HREF=%s>%s</A>\n", docUrl,title);
    }
}

void printDocumentLink(struct hash *ra, char *term, char *docTerm,char *dir,char *title,boolean genericDoc)
// prints a document link
{
printf("  <TD>");
documentLink(ra,term,docTerm,dir,title,genericDoc);
printf("  &nbsp;</TD>\n");
}

int termCmp(const void *va, const void *vb)
/* Compare controlled vocab based on term value */
{
const struct hash *a = *((struct hash **)va);
const struct hash *b = *((struct hash **)vb);
char *typeA = hashMustFindVal((struct hash *)a, CV_TYPE);
char *typeB = hashMustFindVal((struct hash *)b, CV_TYPE);
int ret = strcasecmp(typeA, typeB);
if(ret != 0)
    return ret;

char *targA = hashFindVal((struct hash *)a, CV_TARGET);
char *targB = hashFindVal((struct hash *)b, CV_TARGET);
if (targA != NULL && targB != NULL)
    {
    ret = strcasecmp(targA, targB);
    if(ret != 0)
        return ret;
    }

char *termA = hashMustFindVal((struct hash *)a, CV_TERM);
char *termB = hashMustFindVal((struct hash *)b, CV_TERM);
return (strcasecmp(termA, termB));
}

char *getDescription(struct hash *ra,char *alternateSetting)
// Returns allocated string that is description.  Will include DEPRECATED if appropriate
{
struct dyString *dyDescription = dyStringNew(256);
char *deprecated = hashFindVal(ra, "deprecated");
if (deprecated != NULL)
    dyStringPrintf(dyDescription,"DEPRECATED - %s",deprecated);
char *description = hashFindVal(ra, CV_DESCRIPTION);
if (description == NULL && alternateSetting != NULL)
    description = hashFindVal(ra, alternateSetting);
if (description == NULL)
    description = hashFindVal(ra, CV_TITLE);
if (description == NULL)
    description = hashFindVal(ra, CV_LABEL);
if (description != NULL)
    {
    if (dyStringLen(dyDescription) > 0)
        dyStringAppend(dyDescription,"<BR>");
    dyStringAppend(dyDescription,description);
    }
if (dyStringLen(dyDescription) > 0)
    return dyStringCannibalize(&dyDescription);
dyStringFree(&dyDescription);
return NULL;
}

void printDescription(struct hash *ra,char *alternateSetting,int colsUsed)
{
char *description = getDescription(ra,alternateSetting);
if (colsUsed >= 0)
    printf("  <TD colspan=%d>", TABLE_COLS_AVAILABLE(colsUsed) );
else
    printf("  <TD>");
if (description != NULL)
    {
    printf("%s</TD>\n", description );
    freeMem(description);
    }
else
    puts("&nbsp;</TD>\n");
}

void printLabel(struct hash *ra,char *term)
{
char *label = hashFindVal(ra, CV_LABEL);
if (label != NULL)
    printf("  <TD><I>%s</I></TD>\n", label );
else
    printf("  <TD>%s</TD>\n", term );
}

char *printTerm(struct hash *ra)
{
char *term = (char *)hashMustFindVal(ra, CV_TERM);
printf("  <TD>%s</TD>\n", term);
return term;
}

void printSetting(struct hash *ra,char *setting)
{
char *val = hashFindVal(ra, setting);
printf("  <TD>%s</TD>\n", val?val:"&nbsp;");
}

void printSettingsWithUrls(struct hash *ra,char *urlSetting,char *nameSetting,char *idSetting)
// will print one or more urls with name and optional id.  Only Name is required!
// If more than one, then should add same number of slots to each ("fred;ethyl" & " ;wife")
{
char *names = hashFindVal(ra, nameSetting);
struct slName *nameList = slNameListFromString (names, ';');;

char *urls = NULL;
struct slName *urlList = NULL;
char *ids = NULL;
struct slName *idList = NULL;
if (idSetting != NULL)
    ids = hashFindVal(ra, idSetting);
if (ids != NULL)
    {
    idList = slNameListFromString (ids, ';');
    if (slCount(idList) > slCount(nameList))
        {
        if (slCount(nameList) == 1)
            {
            while(slCount(nameList) < slCount(idList))
                slAddHead(&nameList,slNameNew(nameList->name));
            }
        else
            errAbort("The number of of items in %s and %s must match for term %s",
                 nameSetting,idSetting,(char *)hashMustFindVal(ra,CV_TERM));
        }
    }

if (urlSetting != NULL)
    urls = hashFindVal(ra, urlSetting);
if (urls != NULL)
    {
    if (slCount(nameList) == 1)
        urlList = slNameNew(urls); // It is the case that singleton URLs sometimes have ';'!
    else
        {
        urlList = slNameListFromString (urls, ';');
        if (slCount(urlList) > slCount(nameList))
            errAbort("The number of of items in %s and %s must match for term %s",
                    nameSetting,urlSetting,(char *)hashMustFindVal(ra,CV_TERM));
            }
    }

printf("  <TD>");

    // while there are items in the list of vendorNames, print the vendorName and vendorID together with the url if present
struct slName *curName = NULL;
struct slName *curId;
struct slName *curUrl;

for (curName=nameList,curId=idList,curUrl=urlList; curName != NULL; curName=curName->next)
    {
    if (curName!=nameList)  // Break between links
        printf("<BR>\n      ");

    // if there is a url, add it as a link
    char *url = NULL;
    if (curUrl != NULL)
        {
        url = trimSpaces(curUrl->name);
        if (isNotEmpty(url))
            printf("<A TARGET=_BLANK HREF=%s>", url);
        curUrl=curUrl->next;
        }

    printf("%s", curName->name);
    if (curId != NULL)
        {
        char *id = trimSpaces(curId->name);
        if (isNotEmpty(id))
            printf(" %s", id );
        curId=curId->next;
        }

    if (isNotEmpty(url))
        printf("</A>");

    }
puts("</TD>");

// Free the memory
slFreeList(&nameList);
slFreeList(&idList);
slFreeList(&urlList);
}

boolean doTypeDefinition(char *type,boolean inTable,boolean showType)
// Write out description of the type if it is known
{
struct hash *typeHash = (struct hash *)cvTermTypeHash();

struct hash *ra = hashFindVal(typeHash,(char *)cvTermNormalized(type)); // Find by term
if (ra == NULL)
    return FALSE;

char *label = hashFindVal(ra, CV_LABEL);

puts("<TR>");
struct dyString *dyDefinition = dyStringNew(256);
if (inTable)
    dyStringPrintf(dyDefinition,"  <td colspan=%d style='background:%s; color:%s;'>&nbsp;",
                   TABLE_COLS_AVAILABLE(0),COLOR_LTGREEN,COLOR_DARKBLUE); /// border: 3px ridge #AA0000;
else
    dyStringPrintf(dyDefinition,"<div style='max-width:900px;'>");
if (label != NULL)
    {
    dyStringPrintf(dyDefinition,"<B><em>%s</em></B>",label);
    if (showType)
        dyStringPrintf(dyDefinition,"&nbsp;[%s]",type);
    dyStringAppend(dyDefinition,":&nbsp;");
    }
else if (showType)
    dyStringPrintf(dyDefinition,"<B>%s</B>:&nbsp;",type);

char *val = getDescription(ra,NULL);
dyStringPrintf(dyDefinition,"%s",val);
freeMem(val);
if (inTable)
    dyStringAppend(dyDefinition,"&nbsp;</td>");
else
    dyStringPrintf(dyDefinition,"</div>");
printf("%s\n",dyStringContents(dyDefinition));
dyStringFree(&dyDefinition);

puts("</TR>");
return TRUE;
}

void printColHeader(boolean emphasis,char *title,int sortOrder,char *extra,int colSpan)
{
printf("<TH");
if (sortOrder > 0)
    printf(" class='sortable sort%d'",sortOrder);
if (colSpan > 1)
    printf(" colspan=%d",colSpan);
if (extra)
    printf(" %s",extra);

printf(">");
if (emphasis)
    printf("<em>");
printf("%s",title);
if (emphasis)
    printf("</em>");
printf("</TH>");
}

void doTypeHeader(char *type, char *cellOrg,boolean sortable)
{
if ((organismOptLower != NULL) && !sameWord(cellOrg, organismOptLower))
    errAbort("specified organism %s not consistent with cell type which is org %s\n",
        organismOpt, cellOrg);

// NOTE:  All tables must have the same number of columns in order to allow 'control' to be swapped in  Use colSapn= on description column

printf("<THEAD><TR valign='bottom' style='background:%s;'>\n",COLOR_BG_HEADER_LTBLUE);
int sortOrder = (sortable ? 1: -999); // hint: -999 will keep sortOrtder++ < 0
if (sameWord(type,CV_TERM_CELL))
   {
   printf("<!-- Cell Line table: contains links to protocol file and vendor description page -->");

   /* Venkat: To differentiate between the print statments of Mouse and Human Cell Lines */
   if(sameWord(cellOrg,ORG_HUMAN))
        {
        printColHeader(FALSE,type,         sortOrder++,NULL,1);
        printColHeader(FALSE,"Tier",       sortOrder++,NULL,1);
        printColHeader(FALSE,"Description",sortOrder++,NULL,1);
        printColHeader(FALSE,"Lineage",    sortOrder++,NULL,1);
        printColHeader(FALSE,"Karyotype",  sortOrder++,NULL,1);
        printColHeader(FALSE,"Sex",        sortOrder++,NULL,1);
        printColHeader(FALSE,"Documents",  sortOrder++,NULL,1);
        printColHeader(FALSE,"Vendor ID",  sortOrder++,NULL,1);
        printColHeader(FALSE,"Term ID",    sortOrder++,NULL,1);
        printColHeader(TRUE ,"Label",      sortOrder++,NULL,1);
        }
    else
        {
        printColHeader(FALSE,"Source",     sortOrder++,NULL,1);
        printColHeader(FALSE,"Description",sortOrder++,NULL,TABLE_COLS_AVAILABLE(7));
        printColHeader(FALSE,"Category",   sortOrder++,NULL,1);
        printColHeader(FALSE,"Sex",        sortOrder++,NULL,1);
        printColHeader(FALSE,"Documents",  sortOrder++,NULL,1);
        printColHeader(FALSE,"Source Lab", sortOrder++,NULL,1);
        printColHeader(FALSE,"Term ID",    sortOrder++,NULL,1);
        printColHeader(TRUE ,"Label",      sortOrder++,NULL,1);
        }
    }
else if (sameWord(type,CV_TERM_ANTIBODY))
    {
    printColHeader(FALSE,type,                  sortOrder++,NULL,1);
    printColHeader(FALSE,"Antibody Description",sortOrder++,NULL,1);
    printColHeader(FALSE,"Target",              sortOrder++,NULL,1);
    printColHeader(FALSE,"Target Description",  sortOrder++,"style='min-width:600px;'",1);
    printColHeader(FALSE,"Vendor ID",           sortOrder++,NULL,1);
    printColHeader(FALSE,"Lab",                 sortOrder++,NULL,1);
    printColHeader(FALSE,"Documents",           sortOrder++,NULL,1);
    printColHeader(FALSE,"Lots",                sortOrder++,NULL,1);
    printColHeader(FALSE,"Target Link",         sortOrder++,NULL,1);
    printColHeader(TRUE ,"Label",               sortOrder++,NULL,1);
    }
else
    {
    char *caplitalized = NULL;
    if (sameWord(type,CV_TERM_DATA_TYPE))
        caplitalized = cloneString("Data Type");
    else
        {
        caplitalized = cloneString(type);
        toUpperN(caplitalized,1);
        }

    printColHeader(FALSE,caplitalized,sortOrder++,NULL,1);
    if (sameWord(type,CV_TERM_LOCALIZATION))
        {
        printColHeader(FALSE,"Description",sortOrder++,NULL,TABLE_COLS_AVAILABLE(3));
        printColHeader(FALSE,"GO ID",      sortOrder++,NULL,1);
        }
    else if(sameWord(type,CV_TERM_LAB))
        {
        printColHeader(FALSE,"Institution",sortOrder++,NULL,TABLE_COLS_AVAILABLE(5));
        printColHeader(FALSE,"Lab PI",     sortOrder++,NULL,1);
        printColHeader(FALSE,"Grant PI",   sortOrder++,NULL,1);
        printColHeader(FALSE,"Organism",   sortOrder++,NULL,1);
        }
    else
        printColHeader(FALSE,"Description",sortOrder++,NULL,TABLE_COLS_AVAILABLE(2));

    printColHeader(TRUE ,"Label",sortOrder++,NULL,1);
    freeMem(caplitalized);
    }
puts("</TR></THEAD><TBODY>");
}

boolean doCellRow(struct hash *ra, char *org)
// print one cell row
{
char *s;

    s = hashFindVal(ra, ORGANISM);
    if (s != NULL)
        {
        char *cellOrg = cloneString(s);
        strLower(cellOrg);
        if (differentString(cellOrg, org))
            return FALSE;
        }

    // pathBuffer for new protocols not in human
    char pathBuffer[PATH_LEN];
    safef(pathBuffer, sizeof(pathBuffer), "/ENCODE/protocols/cell/%s/",org);

    if (sameWord(org, ORG_HUMAN))
        {
        if (cgiOptionalInt("tier",0))
            {
            if (hashFindVal(ra,"tier") == NULL)
                return FALSE;
            if (atoi(hashFindVal(ra,"tier"))!=cgiOptionalInt("tier",0))
                return FALSE;
            }
        if (cgiOptionalString("tiers"))
            {
            if (hashFindVal(ra,"tier") == NULL)
                return FALSE;
            boolean found=FALSE;
            char *tiers=cloneString(cgiOptionalString("tiers"));
            char *tier;
            (void)strSwapChar(tiers,',',' ');
            while((tier=nextWord(&tiers)))
                {
                if (atoi(hashFindVal(ra,"tier"))==atoi(tier))
                    {
                    found=TRUE;
                    break;
                    }
                }
            if(!found)
                return FALSE;
            }
        puts("<TR>");
        char *term = printTerm(ra);

        printSetting(ra, "tier");
        printDescription(ra,NULL,-1);
        printSetting(ra,"lineage");
        printSetting(ra,"karyotype");
        printSetting(ra,"sex");
        printDocumentLink(ra,term,"protocol",pathBuffer,NULL,TRUE);
        printSettingsWithUrls(ra,"orderUrl","vendorName","vendorId");
        printSettingsWithUrls(ra,"termUrl","termId",NULL);
        printLabel(ra,term);
        puts("</TR>");
        }
    else        // non-human cell type
        {
        puts("<TR>");
        char *term = printTerm(ra);

        printDescription(ra,NULL,7);
        printSetting(ra,"category");
        printSetting(ra,"sex");
        //printSetting(ra,"karyotype");
        printDocumentLink(ra,term,"protocol",pathBuffer,NULL,TRUE);
        printSettingsWithUrls(ra,"orderUrl","vendorName","vendorId");
        printSettingsWithUrls(ra,"termUrl","termId",NULL);
        printLabel(ra,term);
        puts("</TR>");

        }
return TRUE;
}

boolean doAntibodyRow(struct hash *ra, char *org)
// print one antibody row
{
    puts("<TR>");
    char *term = printTerm(ra);
    printDescription(ra,"antibodyDescription",-1);
    printSetting(ra,"target");                  // target is NOT first but still is major sort order
    printSetting(ra,"targetDescription");
    printSettingsWithUrls(ra,"orderUrl","vendorName","vendorId");
    printSetting(ra,"lab");
    printDocumentLink(ra,term,"validation","/ENCODE/validation/antibodies/",NULL,FALSE);
    printSetting(ra,"lots");
    printSettingsWithUrls(ra,"targetUrl","targetId",NULL);
    printLabel(ra,term);
    puts("</TR>");

return TRUE;
}

boolean doTypeOfTermRow(struct hash *ra, char *org)
// print one typeOfTerm row
{
char *term = (char *)hashMustFindVal(ra, CV_TERM);

    if (sameString(term,cvTypeNormalized(CV_TERM_CELL)))
        term = CV_TERM_CELL;
    else if (sameString(term,cvTypeNormalized(CV_TERM_ANTIBODY)))
        term = CV_TERM_ANTIBODY;

    puts("<TR>");
    printf("  <TD><A HREF='hgEncodeVocab?type=%s' title='%s details' TARGET=ucscVocabChild>%s</a></TD>\n", term, term, term);
    printDescription(ra,NULL,2);
    printLabel(ra,term);
    puts("</TR>");
    if (sameString(term,CV_TERM_CELL))
        {
        puts("<TR>");
        printf("  <TD><A HREF='hgEncodeVocab?type=%s&organism=Mouse' title='Mouse %s details' TARGET=ucscVocabChild>%s</a> <em>(for mouse)</em></TD>\n", term, term, term);
        char *s = getDescription(ra,NULL);
        printf("  <TD colspan=%d>%s <em>(for mouse)</em></TD>\n", TABLE_COLS_AVAILABLE(2), s?s:"&nbsp;");
        freeMem(s);
        printLabel(ra,term);
        puts("</TR>");
        }

return TRUE;
}

boolean doTypeRow(struct hash *ra, char *org)
{
char *type = (char *)cvTermNormalized(hashMustFindVal(ra, CV_TYPE));

if (sameWord(type,CV_TOT))
    return doTypeOfTermRow(ra,org);
else if (sameWord(type,CV_TERM_CELL))
    return doCellRow(ra,org);
else if (sameWord(type,CV_TERM_ANTIBODY))
    return doAntibodyRow(ra,org);
else if(sameWord(type,CV_TERM_LAB))
    {
    puts("<TR>");
    char *term = printTerm(ra);
    printDescription(ra,"labInst",5);
    printSetting(ra,"labPiFull");
    printSetting(ra,"grantPi");
    printSetting(ra,"organism");
    printLabel(ra,term);
    puts("</TR>");
    }
else if(sameWord(type,CV_TERM_GRANT))
    {
    puts("<TR>");
    char *term = printTerm(ra);
    printDescription(ra,"grantInst",2);
    printLabel(ra,term);
    puts("</TR>");
    }
else if (sameWord(type,CV_TERM_LOCALIZATION))
    {
    puts("<TR>");
    char *term = printTerm(ra);
    printDescription(ra,NULL,3);
    printSettingsWithUrls(ra,"termUrl","termId",NULL);
    printLabel(ra,term);
    puts("</TR>");
    }
else  // generic term: term, description, label
    {
    puts("<TR>");
    char *term = printTerm(ra);
    printDescription(ra,NULL,2);
    printLabel(ra,term);
    puts("</TR>");
    }
return TRUE;
}

static char **convertAntibodiesToTargets(struct hash *cvHash,char **requested,int requestCount)
/* Convers requested terms from antibodies to their corresponding targets */
{
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct hash *ra;
char **targets = needMem(sizeof(char *)*requestCount);
int ix = 0;

assert(requested != NULL);

while ((hEl = hashNext(&hc)) != NULL)
    {
    ra = (struct hash *)hEl->val;
    ix = stringArrayIx(hashMustFindVal(ra, CV_TERM),requested,requestCount);
    if (ix != -1 && targets[ix] == NULL) // but not yet converted to antibody
        {
        targets[ix] = cloneString(hashMustFindVal(ra, CV_TARGET)); // Must have a target
        }
    }

// At this point every term should have been found
for(ix=0;ix<requestCount;ix++)
    {
    if (targets[ix] == NULL)
        errAbort("Failed to find antibody %s=%s\n",CV_TERM,requested[ix]);
    }

return targets;
}

static char *normalizeType(char *type)
/* Strips any quotation marks and converts common synonyms */
{
if (type != NULL)
    {
    (void)stripChar(type,'\"');
    if (sameWord(type,"Factor"))
        return cloneString(CV_TERM_ANTIBODY);

    char *cleanType = cloneString(cvTermNormalized(type));
    if (differentString(cleanType,type))
        return cleanType;
    freeMem(cleanType);
    /*
    if ((sameWord(type,"Cell Line"))
    ||  (sameWord(type,"cellLine" ))
    ||  (sameWord(type,"Cell Type"))
    ||  (sameWord(type,"Cell Type"))
    ||  (sameWord(type,"Cell" )))  // sameWord is case insensitive
        return cloneString(CV_TERM_CELL);
    else if (sameWord(type,"Factor"))
         ||  (sameString(type,"Antibody")))
        return cloneString(CV_TERM_ANTIBODY);
    */
    }
return type;
}

static char *findType(struct hash *cvHash,char **requested,int requestCount,char **queryBy, char **org,boolean silent)
/* returns the type that was requested or else the type associated with the term requested */
{
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct hash *ra;
char *type = typeOpt;

if (requested != NULL) // if no type, find it from requested terms.  Will validate that terms match type
{                  // NOTE: Enter here even if there is a type, to confirm the type
    while ((hEl = hashNext(&hc)) != NULL)  // FIXME: This should be using mdbCv APIs to get hashes.  One per "request[]"
        {
        ra = (struct hash *)hEl->val;
        if (sameWord(hashMustFindVal(ra, CV_TYPE),CV_TOT)) // TOT = typeOfTerm
            continue;
        char *val = hashFindVal(ra, *queryBy);
        if (val != NULL)
            {
            int ix = stringArrayIx(val,requested,requestCount);
            if(ix != -1) // found
                {
                char *thisType = hashMustFindVal(ra, CV_TYPE);
                char *thisOrg = hashFindVal(ra, ORGANISM);
                if(type == NULL)
                    {
                    if (thisOrg != NULL)
                        {
                        *org = strLower(cloneString(thisOrg));
                        }
                    type = thisType;
                    }
                else if(differentWord(type,thisType))
                    {
                    if(sameWord(CV_TERM_CONTROL,type))
                        type = thisType;
                    else if (differentWord(CV_TERM_CONTROL,thisType))
                        errAbort("Error: Requested %s of type '%s'.  But '%s' has type '%s'\n",
                                *queryBy,type,requested[ix],thisType);
                    }
                }
            }
        }
    }
if (type == NULL && sameWord(*queryBy,CV_TERM))    // Special case of term becoming target
    {
    char *queryByTarget = CV_TARGET;
    type = findType(cvHash,requested,requestCount,&queryByTarget,org,TRUE); // silent here
    if (type != NULL)
        *queryBy = queryByTarget;
    }
if (type == NULL && !silent)    // Still not type? abort
    errAbort("Error: Required %s=%s ['%s', '%s', '%s', '%s' or '%s'] argument not found\n",
             *queryBy,requested != NULL?*requested:"?", CV_TYPE, CV_TERM, CV_TAG, CV_TARGET, CV_LABEL);

return normalizeType(type);
}

void doMiddle()
{
struct hash *cvHash = raReadAll((char *)cvFile(), CV_TERM);
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct slList *termList = NULL;
struct hash *ra;
int totalPrinted = 0;

// Prepare an array of selected terms (if any)
int requestCount = 0;
char **requested = NULL;
char *requestVal = termOpt;
char *queryBy = CV_TERM;
if (tagOpt)
    {
    requestVal = tagOpt;
    queryBy = CV_TAG;
    }
else if (targetOpt)
    {
    requestVal = targetOpt;
    queryBy = CV_TERM;  // request target is special case: look up term, convert to target, display target
    }
else if (labelOpt)
    {
    requestVal = labelOpt;
    queryBy = CV_LABEL;
    }
if (requestVal)
    {
    (void)stripChar(requestVal,'\"');
    requestCount = chopCommas(requestVal,NULL);
    requested = needMem(requestCount * sizeof(char *));
    chopByChar(requestVal,',',requested,requestCount);
    }

char *org = NULL;
// if the org is specified in the type (eg. cell line)
// then use that for the org, otherwise use the command line option,
// otherwise use human.
char *type = findType(cvHash,requested,requestCount,&queryBy, &org, FALSE);
if (org == NULL)
    org = organismOptLower;
if (org == NULL)
    org = ORG_HUMAN;

// Special logic for requesting antibody by target
if (targetOpt && requestCount > 0 && sameWord(queryBy,CV_TERM) && sameWord(type,CV_TERM_ANTIBODY))
    {
    // Several antibodies may have same target.
    // requested target={antibody} and found antibody
    // Must now convert each of the requested terms to is target before displaying all targets with this term
    char **targets = convertAntibodiesToTargets(cvHash,requested,requestCount);
    if (targets != NULL)
        {
        freeMem(requested);
        requested = targets;
        queryBy = CV_TARGET;
        }
    }
//warn("Query by: %s = '%s' type:%s",queryBy,requestVal?requestVal:"all",type);

// Get just the terms that match type and requested, then sort them
if (differentWord(type,CV_TOT) || typeOpt != NULL )  // If type resolves to typeOfTerm and typeOfTerm was not requested, then just show definition
    {
    while ((hEl = hashNext(&hc)) != NULL)
        {
        ra = (struct hash *)hEl->val;
        char *thisType = (char *)cvTermNormalized(hashMustFindVal(ra,CV_TYPE));
        if(differentWord(thisType,type) && (requested == NULL || differentWord(thisType,CV_TERM_CONTROL)))
            continue;
        // Skip all rows that do not match queryBy param if specified
        if(requested)
            {
            char *val = hashFindVal(ra, queryBy);
            if (val == NULL)
                continue;
            if (-1 == stringArrayIx(val,requested,requestCount))
                continue;
            }
        slAddTail(&termList, ra);
        }
    }
slSort(&termList, termCmp);

boolean described = doTypeDefinition(type,FALSE,(slCount(termList) == 0));
boolean sortable = (slCount(termList) > 5);
if (sortable)
    {
    webIncludeResourceFile("HGStyle.css");
    jsIncludeFile("jquery.js",NULL);
    jsIncludeFile("utils.js",NULL);
    printf("<TABLE class='sortable' border=1 CELLSPACING=0 style='border: 2px outset #006600; background-color:%s;'>\n",COLOR_BG_DEFAULT);
    }
else
    printf("<TABLE BORDER=1 BGCOLOR=%s CELLSPACING=0 CELLPADDING=2>\n",COLOR_BG_DEFAULT);
if (slCount(termList) > 0)
    {
    doTypeHeader(type, org,sortable);

    // Print out the terms
    while((ra = slPopHead(&termList)) != NULL)
        {
        if(doTypeRow( ra, org ))
            totalPrinted++;
        }
    }
puts("</TBODY></TABLE><BR>");
if (sortable)
    puts("<script type='text/javascript'>{$(document).ready(function() {sortTableInitialize($('table.sortable')[0],true,true);});}</script>");
if (totalPrinted == 0)
    {
    if (!described)
        warn("Error: Unrecognised type (%s)\n", type);
    }
else if(totalPrinted > 1)
    printf("Total = %d\n", totalPrinted);
}

int main(int argc, char *argv[])
/* Process command line */
{
cgiSpoof(&argc, argv);
termOpt = cgiOptionalString(CV_TERM);
tagOpt = cgiOptionalString(CV_TAG);
typeOpt = cgiOptionalString(CV_TYPE);
targetOpt = cgiOptionalString(CV_TARGET);
labelOpt = cgiOptionalString(CV_LABEL);
organismOpt = cgiUsualString(ORGANISM, organismOpt);
if (organismOpt != NULL)
    {
    organismOptLower=cloneString(organismOpt);
    strLower(organismOptLower);
    }
char *bgColor = cgiOptionalString("bgcolor");
if (bgColor)
    htmlSetBgColor(strtol(bgColor, 0, 16));
htmlSetStyle(htmlStyleUndecoratedLink);
htmShell("ENCODE Controlled Vocabulary", doMiddle, "get");
return 0;
}

