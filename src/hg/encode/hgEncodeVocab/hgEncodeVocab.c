/* hgEncodeVocab - print table of controlled vocabulary from ENCODE configuration files */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "htmshell.h"
#include "ra.h"
#include "hui.h"

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

#define TERM_FIELD         "term"
#define TAG_FIELD          "tag"
#define TYPE_FIELD         "type"
#define TARGET_FIELD       "target"
#define LABEL_FIELD        "label"
#define TITLE_FIELD        "title"

#define CELL_TYPE          "Cell Line"
#define CELL_TYPE_ALT      "cellType"
#define ANTIBODY_TYPE      "Antibody"
#define ANTIBODY_TYPE_ALT  "antibody"
#define LOCALIZATION_TYPE  "localization"
#define LAB_TYPE           "lab"
#define GRANT_TYPE         "grant"
#define CONTROL_TYPE       "control"
#define DATA_TYPE          "dataType"
#define TYPE_OF_TERM       "typeOfTerm"

#define ORGANISM           "organism"
#define ORG_HUMAN          "human"
#define ORG_MOUSE          "mouse"

#define DESCRIPTION_FIELD  "description"
#define MAX_TABLE_ROWS     10
#define TABLE_ROWS_AVAILABLE(rowsUsed) (MAX_TABLE_ROWS - (rowsUsed))

static char *termOpt = NULL;
static char *tagOpt = NULL;
static char *typeOpt = NULL;
static char *targetOpt = NULL;
static char *labelOpt = NULL;
static char *organismOpt = NULL; // we default to human if nothing else is set
static char *organismOptLower = NULL; //  version of above used for path names
static char *cv_file()
{
/* return default location of cv.ra (can specify as cgi var: ra=cv.ra) */

static char filePath[PATH_LEN];
safef(filePath, sizeof(filePath), "%s/encode/cv.ra", hCgiRoot());
if(!fileExists(filePath))
    errAbort("Error: can't locate cv.ra; %s doesn't exist\n", filePath);
return filePath;
}

void documentLink(struct hash *ra, char *term, char *docTerm,char *dir,char *title,boolean genericDoc)
/* Compare controlled vocab based on term value */
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

int termCmp(const void *va, const void *vb)
/* Compare controlled vocab based on term value */
{
const struct hash *a = *((struct hash **)va);
const struct hash *b = *((struct hash **)vb);
char *typeA = hashMustFindVal((struct hash *)a, TYPE_FIELD);
char *typeB = hashMustFindVal((struct hash *)b, TYPE_FIELD);
int ret = strcasecmp(typeA, typeB);
if(ret != 0)
    return ret;

char *targA = hashFindVal((struct hash *)a, TARGET_FIELD);
char *targB = hashFindVal((struct hash *)b, TARGET_FIELD);
if (targA != NULL && targB != NULL)
    {
    ret = strcasecmp(targA, targB);
    if(ret != 0)
        return ret;
    }

char *termA = hashMustFindVal((struct hash *)a, TERM_FIELD);
char *termB = hashMustFindVal((struct hash *)b, TERM_FIELD);
return (strcasecmp(termA, termB));
}

boolean doTypeDefinition(struct hash *cvHash,char *type,boolean inTable,boolean showType)
// Write out description of type the type if it is known
{
struct hash *ra = NULL;
if (sameWord(type,CELL_TYPE))
    ra = hashFindVal(cvHash,CELL_TYPE_ALT);
else if (sameWord(type,ANTIBODY_TYPE))
    ra = hashFindVal(cvHash,ANTIBODY_TYPE_ALT); // Find by term
else
    ra = hashFindVal(cvHash,type); // Find by term
if (ra == NULL)
    return FALSE;

char *val = hashMustFindVal(ra, TYPE_FIELD);
if (differentWord(val,"typeOfTerm"))
    errAbort("Expected '%s' to be typeOfTerm but was '%s'\n",type,val);

val = hashMustFindVal(ra, DESCRIPTION_FIELD);
char *label = hashFindVal(ra, LABEL_FIELD);

puts("<TR>");
struct dyString *dyDefinition = dyStringNew(256);
if (inTable)
    dyStringPrintf(dyDefinition,"  <td colspan=%d style='background:%s; color:%s;'>&nbsp;",
                   TABLE_ROWS_AVAILABLE(0),COLOR_LTGREEN,COLOR_DARKBLUE); /// border: 3px ridge #AA0000;
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

dyStringPrintf(dyDefinition,"%s",val);
if (inTable)
    dyStringAppend(dyDefinition,"&nbsp;</td>");
else
    dyStringPrintf(dyDefinition,"</div>");
printf("%s\n",dyStringContents(dyDefinition));
dyStringFree(&dyDefinition);

puts("</TR>");
return TRUE;
}

void doTypeHeader(char *type, char *cellOrg)
{
if ((organismOptLower != NULL) && !sameWord(cellOrg, organismOptLower))
    errAbort("specified organism %s not consistent with cell type which is org %s\n",
        organismOpt, cellOrg);

// NOTE:  All tables must have the same number of columns in order to allow 'control' to be swapped in  Use colSapn= on description column

printf("<TR style='background:%s;'>\n",COLOR_BG_HEADER_LTBLUE);
if (sameWord(type,CELL_TYPE))
   {
    printf("<!-- Cell Line table: contains links to protocol file and vendor description page -->");

    /* Venkat: To differentiate between the print statments of Mouse and Human Cell Lines */
    if(sameWord(cellOrg,ORG_HUMAN))
         {
   	 printf("  <TH>%s</TH><TH>Tier</TH><TH>Description</TH><TH>Lineage</TH><TH>Karyotype</TH><TH>Sex</TH><TH>Documents</TH><TH>Vendor ID</TH><TH>Term ID</TH><TH><I>Label</I></TH>",type);
   	 }
      else
	 {
    	  printf("  <TH>Source</TH><TH colspan=%d>Description</TH><TH>Category</TH><TH>Sex</TH><TH>Documents</TH><TH>Source Lab </TH><TH>Term ID</TH><TH><I>Label</I>",TABLE_ROWS_AVAILABLE(7));
         // printf("  <TH>%s</TH><TH>Description</TH><TH>Category</TH><TH>Sex</TH><TH>Documents</TH><TH>Source</TH><TH>Term ID</TH>",type)
	 }
    }
else if (sameWord(type,ANTIBODY_TYPE))
    {
    printf("  <TH>%s</TH><TH>Target</TH><TH>Target Description</TH><TH>Antibody Description</TH><TH>Vendor ID</TH><TH>Lab</TH><TH>Documents</TH><TH>Lots</TH><TH>Target Link</TH><TH><I>Label</I></TH>",type);
    }
else if(sameWord(type,LAB_TYPE))
    {
    printf("  <TH>%s</TH><TH colspan=%d>Institution</TH><TH>Lab PI</TH><TH>Grant PI</TH><TH>Organism</TH><TH><I>Label</I></TH>",type,TABLE_ROWS_AVAILABLE(5));
    }
else if(sameWord(type,LAB_TYPE))
    {
    printf("  <TH>%s</TH><TH colspan=%d>Institution</TH><TH><I>Label</I></TH>",type,TABLE_ROWS_AVAILABLE(2));
    }
else if(sameWord(type,DATA_TYPE))
    {
    printf("  <TH>Data Type</TH><TH colspan=%d>Description</TH><TH><I>Label</I></TH>",TABLE_ROWS_AVAILABLE(2));
    }
else
    {
    char *caplitalized = cloneString(type);
    toUpperN(caplitalized,1);

    if (sameWord(type,LOCALIZATION_TYPE))
        {
        printf("  <TH>%s</TH><TH colspan=%d>Description</TH><TH>GO ID</TH><TH><I>Label</I></TH>",caplitalized,TABLE_ROWS_AVAILABLE(3));
        }
    else
        printf("  <TH>%s</TH><TH colspan=%d>Description</TH><TH><I>Label</I></TH>",caplitalized,TABLE_ROWS_AVAILABLE(2));

    freeMem(caplitalized);
    }
puts("</TR>");
}

boolean doTypeRow(struct hash *ra, char *org)
{
char *term = (char *)hashMustFindVal(ra, TERM_FIELD);
char *type = (char *)hashMustFindVal(ra, TYPE_FIELD);
char *s, *t, *u;

if (sameWord(type,CELL_TYPE))
    {
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

	printf("  <TD>%s</TD>\n", term);

	s = hashFindVal(ra, "tier");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	s = hashFindVal(ra, DESCRIPTION_FIELD);
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	s = hashFindVal(ra, "lineage");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	s = hashFindVal(ra, "karyotype");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	s = hashFindVal(ra, "sex");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );

	// add links to protocol doc if it exists
	printf("  <TD>");
	documentLink(ra,term,"protocol",pathBuffer,NULL,TRUE);
	printf("  &nbsp;</TD>\n");

	s = hashFindVal(ra, "vendorName");
	t = hashFindVal(ra, "vendorId");
	u = hashFindVal(ra, "orderUrl");
	printf("  <TD>");
	if (u)
	    printf("<A TARGET=_BLANK HREF=%s>", u);
	printf("%s %s", s ? s : "&nbsp;", t ? t : "&nbsp;");
	if (u)
	    printf("</A>");
	puts("</TD>");

	s = hashFindVal(ra, "termId");
	u = hashFindVal(ra, "termUrl");
	printf("  <TD>");
	if (u)
	    printf("<A TARGET=_BLANK HREF=%s>", u);
	printf("%s", s ? s : "&nbsp;");
	if (u)
	    printf("</A>");
	puts("</TD>");
        s = hashFindVal(ra, LABEL_FIELD);
        if (s != NULL)
            printf("  <TD><I>%s</I></TD>\n", s );
        else
            printf("  <TD>%s</TD>\n", term );
	puts("</TR>");
	}
    else	// non-human cell type
	{
	puts("<TR>");

	printf("  <TD>%s</TD>\n", term);

	s = hashFindVal(ra, DESCRIPTION_FIELD);
	printf("  <TD colspan=%d>%s</TD>\n", TABLE_ROWS_AVAILABLE(7), s ? s : "&nbsp;" );
	s = hashFindVal(ra, "category");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	s = hashFindVal(ra, "sex");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	//s = hashFindVal(ra, "karyotype");
	//printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	//s = hashFindVal(ra, "sex");
	//printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );

	// add links to protocol doc if it exists
	printf("  <TD>");
	documentLink(ra,term,"protocol",pathBuffer,NULL,TRUE);
	printf("  &nbsp;</TD>\n");

	s = hashFindVal(ra, "vendorName");
	t = hashFindVal(ra, "vendorId");
	u = hashFindVal(ra, "orderUrl");
	printf("  <TD>");
	if (u)
	    printf("<A TARGET=_BLANK HREF=%s>", u);
	printf("%s %s", s ? s : "&nbsp;", t ? t : "&nbsp;");
	if (u)
	    printf("</A>");
	puts("</TD>");

	s = hashFindVal(ra, "termId");
	u = hashFindVal(ra, "termUrl");
	printf("  <TD>");
	if (u)
	    printf("<A TARGET=_BLANK HREF=%s>", u);
	printf("%s", s ? s : "&nbsp;");
	if (u)
	    printf("</A>");
	puts("</TD>");
        s = hashFindVal(ra, LABEL_FIELD);
        if (s != NULL)
            printf("  <TD><I>%s</I></TD>\n", s );
        else
            printf("  <TD>%s</TD>\n", term );
	puts("</TR>");

	}
    }
else if (sameWord(type,ANTIBODY_TYPE))
    {
    /* if the type is Antibody then
     * print "term target targetDescription antibodyDescription" */

    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashFindVal(ra, "target");                  // target is NOT first but still is major sort order
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "targetDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "antibodyDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");

    /* In the antibody cv, there may be multiple sources of the antibody.  In this text we allow the vendorName,
     * vendorId, and vendorUrls to be a semi-colon separated list to account for this in the display */

    struct slName *sList;
    struct slName *tList;
    struct slName *uList;
    struct slName *currentS;
    struct slName *currentT;
    struct slName *currentU;


    /* For vendorName, vendorId, and orderUrl, grab the string and separate it into a list based on ';' */
    s = hashFindVal(ra, "vendorName");
    sList =  slNameListFromString (s, ';') ;

    t = hashFindVal(ra, "vendorId");
    tList =  slNameListFromString (t, ';') ;

    u = hashFindVal(ra, "orderUrl");
    uList =  slNameListFromString (u, ';') ;

    /* if the number of vendorNames and vendorId's do not match, error */
    if (slCount( sList) != slCount( tList))
        errAbort("The number of antibody vendors must equal number of antibody vender ID's");
    printf("  <TD>");

    /* while there are items in the list of vendorNames, print the vendorName and vendorID together with the url if present */
    for (currentS=sList, currentT=tList, currentU=uList; (currentS != NULL) ; currentS = currentS->next, currentT = currentT->next) {
        /* if there is a url, add it as a link */
        if (currentU != NULL)
            printf("<A TARGET=_BLANK HREF=%s>", currentU->name);
        /* print the current vendorName - vendorId pair */
        printf("%s %s", currentS->name , currentT->name );
        /* if there is a url, finish the link statement and increment the currentU counter */
        if (currentU)
            {
            printf("</A>");
            currentU=currentU->next;
            }
        puts("<BR>");

        }
    puts("</TD>");

    /* Free the memory */
    slFreeList (&sList);
    slFreeList (&tList);
    slFreeList (&uList);


    s = hashFindVal(ra, "lab");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");

    // add links to validation doc if it exists
    printf("  <TD>");
    documentLink(ra,term,"validation","/ENCODE/validation/antibodies/",NULL,FALSE);
    printf("  &nbsp;</TD>\n");

    s = hashFindVal(ra, "lots");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");

    t = hashFindVal(ra, "targetId");
    u = hashFindVal(ra, "targetUrl");
    printf("  <TD>");
    if (u)
        printf("<A TARGET=_BLANK HREF=%s>", u);
    printf("%s", t ? t : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");

    s = hashFindVal(ra, LABEL_FIELD);
    if (s != NULL)
        printf("  <TD><I>%s</I></TD>\n", s );
    else
        printf("  <TD>%s</TD>\n", term );
    puts("</TR>");
    }
else if(sameWord(type,LAB_TYPE))
    {
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashFindVal(ra, "labInst");
    printf("  <TD colspan=%d>%s</TD>\n", TABLE_ROWS_AVAILABLE(5), s?s:"&nbsp;");
    s = hashFindVal(ra, "labPiFull");
    printf("  <TD>%s</TD>\n", s?s:"&nbsp;");
    s = hashFindVal(ra, "grantPi");
    printf("  <TD>%s</TD>\n", s?s:"&nbsp;");
    s = hashFindVal(ra, "organism");
    printf("  <TD>%s</TD>\n", s?s:"&nbsp;");
    s = hashFindVal(ra, LABEL_FIELD);
    if (s != NULL)
        printf("  <TD><I>%s</I></TD>\n", s );
    else
        printf("  <TD>%s</TD>\n", term );
    }
else if(sameWord(type,GRANT_TYPE))
    {
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashFindVal(ra, "grantInst");
    printf("  <TD colspan=%d>%s</TD>\n", TABLE_ROWS_AVAILABLE(2), s?s:"&nbsp;");
    s = hashFindVal(ra, LABEL_FIELD);
    if (s != NULL)
        printf("  <TD><I>%s</I></TD>\n", s );
    else
        printf("  <TD>%s</TD>\n", term );
    }
else if (sameWord(type,LOCALIZATION_TYPE))
    {
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashMustFindVal(ra, DESCRIPTION_FIELD);
    printf("  <TD colspan=%d>%s</TD>\n", TABLE_ROWS_AVAILABLE(3), s);
    s = hashFindVal(ra, "termId");
    u = hashFindVal(ra, "termUrl");
    printf("  <TD>");
    if (u)
        printf("<A TARGET=_BLANK HREF=%s>", u);
    printf("%s", s ? s : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");

    s = hashFindVal(ra, LABEL_FIELD);
    if (s != NULL)
        printf("  <TD><I>%s</I></TD>\n", s );
    else
        printf("  <TD>%s</TD>\n", term );
    puts("</TR>");
    }
else if (sameWord(type,TYPE_OF_TERM))
    {
    s = hashFindVal(ra, DESCRIPTION_FIELD);

    puts("<TR>");
    printf("  <TD><A HREF='hgEncodeVocab?type=%s' title='%s details' TARGET=ucscVocabChild>%s</a></TD>\n", term, term, term);
    printf("  <TD colspan=%d>%s</TD>\n", TABLE_ROWS_AVAILABLE(2), s?s:"&nbsp;");
    s = hashFindVal(ra, LABEL_FIELD);
    if (s != NULL)
        printf("  <TD><I>%s</I></TD>\n", s );
    else
        printf("  <TD>%s</TD>\n", term );
    if (sameString(term,CELL_TYPE_ALT))
        {
        puts("<TR>");
        printf("  <TD><A HREF='hgEncodeVocab?type=%s&organism=Mouse' title='Mouse %s details' TARGET=ucscVocabChild>%s</a> <em>(for mouse)</em></TD>\n", term, term, term);
        s = hashFindVal(ra, DESCRIPTION_FIELD);
        printf("  <TD colspan=%d>%s <em>(for mouse)</em></TD>\n", TABLE_ROWS_AVAILABLE(2), s?s:"&nbsp;");
        s = hashFindVal(ra, LABEL_FIELD);
        if (s != NULL)
            printf("  <TD><I>%s</I></TD>\n", s );
        else
            printf("  <TD>%s</TD>\n", term );
        }
    }
else
    {
    s = hashFindVal(ra, DESCRIPTION_FIELD);
    if(s == NULL)
        s = hashFindVal(ra, TITLE_FIELD);
    if(s == NULL)
        s = hashFindVal(ra, LABEL_FIELD);

    //printf("  <TH>%s</TH><TH>Description</TH>",type);
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    printf("  <TD colspan=%d>%s</TD>\n", TABLE_ROWS_AVAILABLE(2), s?s:"&nbsp;");
    s = hashFindVal(ra, LABEL_FIELD);
    if (s != NULL)
        printf("  <TD><I>%s</I></TD>\n", s );
    else
        printf("  <TD>%s</TD>\n", term );
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
    ix = stringArrayIx(hashMustFindVal(ra, TERM_FIELD),requested,requestCount);
    if (ix != -1 && targets[ix] == NULL) // but not yet converted to antibody
        {
        targets[ix] = cloneString(hashMustFindVal(ra, TARGET_FIELD)); // Must have a target
        }
    }

// At this point every term should have been found
for(ix=0;ix<requestCount;ix++)
    {
    if (targets[ix] == NULL)
        errAbort("Failed to find antibody %s=%s\n",TERM_FIELD,requested[ix]);
    }

return targets;
}

static char *normalizeType(char *type)
/* Strips any quotation marks and converts common synonyms */
{
if (type != NULL)
{
    (void)stripChar(type,'\"');
    if ((sameWord(type,"Cell Line"))
    ||  (sameWord(type,"cellLine" ))
    ||  (sameWord(type,"Cell Type"))
    ||  (sameWord(type,CELL_TYPE_ALT))
    ||  (sameWord(type,"Cell" )))  // sameWord is case insensitive
        return cloneString(CELL_TYPE);
    else if (sameWord(type,"Factor"))
        return cloneString(ANTIBODY_TYPE);
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
    while ((hEl = hashNext(&hc)) != NULL)
        {
        ra = (struct hash *)hEl->val;
        char *val = hashFindVal(ra, *queryBy);
        if (val != NULL)
            {
            int ix = stringArrayIx(val,requested,requestCount);
            if(ix != -1) // found
                {
                char *thisType = hashMustFindVal(ra, TYPE_FIELD);
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
                    if(sameWord(CONTROL_TYPE,type))
                        type = thisType;
                    else if (differentWord(CONTROL_TYPE,thisType))
                        errAbort("Error: Requested %s of type '%s'.  But '%s' has type '%s'\n",
                                *queryBy,type,requested[ix],thisType);
                    }
                }
            }
        }
    }
if (type == NULL && sameWord(*queryBy,TERM_FIELD))    // Special case of term becoming target
    {
    char *queryByTarget = TARGET_FIELD;
    type = findType(cvHash,requested,requestCount,&queryByTarget,org,TRUE); // silent here
    if (type != NULL)
        *queryBy = queryByTarget;
    }
if (type == NULL && !silent)    // Still not type? abort
    errAbort("Error: Required %s=%s ['%s', '%s', '%s', '%s' or '%s'] argument not found\n",
             *queryBy,requested != NULL?*requested:"?", TYPE_FIELD, TERM_FIELD, TAG_FIELD, TARGET_FIELD, LABEL_FIELD);

return normalizeType(type);
}

void doMiddle()
{
struct hash *cvHash = raReadAll(cv_file(), TERM_FIELD);  // cv_file is no longer passed as an option
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct slList *termList = NULL;
struct hash *ra;
int totalPrinted = 0;

// Prepare an array of selected terms (if any)
int requestCount = 0;
char **requested = NULL;
char *requestVal = termOpt;
char *queryBy = TERM_FIELD;
if (tagOpt)
    {
    requestVal = tagOpt;
    queryBy = TAG_FIELD;
    }
else if (targetOpt)
    {
    requestVal = targetOpt;
    queryBy = TERM_FIELD;  // request target is special case: look up term, convert to target, display target
    }
else if (labelOpt)
    {
    requestVal = labelOpt;
    queryBy = LABEL_FIELD;
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
if (targetOpt && requestCount > 0 && sameWord(queryBy,TERM_FIELD) && sameWord(type,ANTIBODY_TYPE))
    {
    // Several antibodies may have same target.
    // requested target={antibody} and found antibody
    // Must now convert each of the requested terms to is target before displaying all targets with this term
    char **targets = convertAntibodiesToTargets(cvHash,requested,requestCount);
    if (targets != NULL)
        {
        freeMem(requested);
        requested = targets;
        queryBy = TARGET_FIELD;
        }
    }

// Get just the terms that match type and requested, then sort them
if (differentWord(type,TYPE_OF_TERM) || typeOpt != NULL )  // If type resolves to typeOfTerm and typeOfTerm was not requested, then just show definition
    {
    while ((hEl = hashNext(&hc)) != NULL)
        {
        ra = (struct hash *)hEl->val;
        char *thisType = hashMustFindVal(ra,TYPE_FIELD);
        if(differentWord(thisType,type) && (requested == NULL || differentWord(thisType,CONTROL_TYPE)))
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

boolean described = doTypeDefinition(cvHash,type,FALSE,(slCount(termList) == 0));
printf("<TABLE BORDER=1 BGCOLOR=%s CELLSPACING=0 CELLPADDING=2>\n",COLOR_BG_DEFAULT);
//boolean described = doTypeDefinition(cvHash,type,TRUE,(slCount(termList) == 0));
if (slCount(termList) > 0)
    {
    doTypeHeader(type, org);

    // Print out the terms
    while((ra = slPopHead(&termList)) != NULL)
        {
        if(doTypeRow( ra, org ))
            totalPrinted++;
        }
    }
puts("</TABLE><BR>");
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
termOpt = cgiOptionalString(TERM_FIELD);
tagOpt = cgiOptionalString(TAG_FIELD);
typeOpt = cgiOptionalString(TYPE_FIELD);
targetOpt = cgiOptionalString(TARGET_FIELD);
labelOpt = cgiOptionalString(LABEL_FIELD);
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

