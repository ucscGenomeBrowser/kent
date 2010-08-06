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
 *   hgEncodeVocab [ra=cv.ra] type=[Antibody|"Cell Line"|localization|rnaExtract|"Gene Type"] [tier=(1|2|3)]
 * options:\n"
 *    ra=cv.ra       : Path to cv.ra file (default cv_file())
 *    type=TypeName  : Type to display
 *    tier=N         : If type="Cell Line" then this is the tier to display
 *    bgcolor=RRGGBB : Change background color (hex digits)
 *    organism=Human|Mouse : If type="Cell Line", then set 'Mouse' to override default Human
 *    term=a         : Display row for a single term
 *    TODO: terms=a,b,c    : Display rows for listed terms.  Must use with 'type'.
 *    tag=a          : Display row for a single term, using tag as identifier
 *    TODO:  tags=a,b,c    : Display rows for listed terms, using tags as identifiers.  Must use with 'type'.
 */

static char const rcsid[] = "$Id: hgEncodeVocab.c,v 1.34 2010/05/28 23:28:06 tdreszer Exp $";

//options that apply to all vocab types

static char *termOpt = NULL;
static char *tagOpt = NULL;
static char *typeOpt = NULL;
static char *organismOpt = "Human"; // default, uses naming convention from dbDb table
static char *organismOptLower; //  version of above used for path names
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
        //if (fileExists(documentFile))
            printf(" <A TARGET=_BLANK HREF=%s>%s</A>\n", docUrl,docTitle);
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
char *typeA = hashMustFindVal((struct hash *)a, "type");
char *typeB = hashMustFindVal((struct hash *)b, "type");
char *termA = hashMustFindVal((struct hash *)a, "term");
char *termB = hashMustFindVal((struct hash *)b, "term");
int ret = strcasecmp(typeA, typeB);
if(ret != 0)
    return ret;
return (strcasecmp(termA, termB));
}

void doTypeHeader(char *type)
{
if (sameString(type,"Cell Line"))
   {
    /* Venkat: To differentiate between the print statments of Mouse and Human Cell Lines */
    if(sameString(organismOpt,"Human"))
         {
   	 printf("  <TH>%s</TH><TH>Tier</TH><TH>Description</TH><TH>Lineage</TH><TH>Karyotype</TH><TH>Sex</TH><TH>Documents</TH><TH>Vendor ID</TH><TH>Term ID</TH>",type);
   	 }
      else 
	 {
    	  printf("  <TH>Source</TH><TH>Description</TH><TH>Category</TH><TH>Sex</TH><TH>Documents</TH><TH>Source Lab </TH><TH>Term ID</TH>");
         // printf("  <TH>%s</TH><TH>Description</TH><TH>Category</TH><TH>Sex</TH><TH>Documents</TH><TH>Source</TH><TH>Term ID</TH>",type)
	 }
    }
else if (sameString(type,"Antibody"))
    {
    printf("  <TH>%s</TH><TH>Target Description</TH><TH>Antibody Description</TH><TH>Vendor ID</TH><TH>Lab</TH><TH>Documents</TH><TH>Lots</TH><TH>Target Link</TH>",type);
    }
else if (sameString(type,"ripAntibody"))
    {
    printf("  <TH>%s</TH><TH>Antibody Description</TH><TH>Target Description</TH><TH>Vendor ID</TH>",type);
    }
else if (sameString(type,"ripTgtProtein"))
    {
    printf("  <TH>%s</TH><TH>Alternative Symbols</TH><TH>Description</TH>",type);
    }
else
    {
    char *caplitalized = cloneString(type);
    toUpperN(caplitalized,1);

    if (sameString(type,"localization"))
        {
        printf("  <TH>%s</TH><TH>Description</TH><TH>GO ID</TH>",caplitalized);
        }
    else
        printf("  <TH>%s</TH><TH>Description</TH>",caplitalized);

    freeMem(caplitalized);
    }
}

boolean doTypeRow(struct hash *ra)
{
char *term = (char *)hashMustFindVal(ra, "term");
char *type = (char *)hashMustFindVal(ra, "type");
char *s, *t, *u;

if (sameString(type,"Antibody"))
    {
    /* if the type is Antibody then
     * print "term targetDescription antibodyDescription" */

    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
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

    puts("</TR>");
    }

    /* Ideally, Cricket will be removing all of the ripAntibody and ripTgtProtien
     * code after this is tested a bit */

else if (sameString(type,"ripAntibody"))
    {
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashFindVal(ra, "antibodyDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "targetDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
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

    puts("</TR>");
    }

else if (sameString(type,"ripTgtProtein"))
    {
    puts("<TR>");
    s = hashFindVal(ra, "url");
    if (s)
	printf("  <TD><A TARGET=_BLANK HREF=\"%s\">%s</A></TD>\n", s, term);
    else
	printf("  <TD>%s</TD>\n", term);
    s = hashFindVal(ra, "alternativeSymbols");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "description");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    puts("</TR>");
    } 

else if (sameString(type,"localization"))
    {
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashMustFindVal(ra, "description");
    printf("  <TD>%s</TD>\n", s);
    s = hashFindVal(ra, "termId");
    u = hashFindVal(ra, "termUrl");
    printf("  <TD>");
    if (u)
        printf("<A TARGET=_BLANK HREF=%s>", u);
    printf("%s", s ? s : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");

    puts("</TR>");
    }
else if (sameString(type,"Cell Line"))
    {
    printf("<!-- Cell Line table: contains links to protocol file and vendor description page -->");
    s = hashFindVal(ra, "organism");
    if (s && differentString(s, organismOpt))
        return FALSE;
    // pathBuffer for new protocols not in human    
    char pathBuffer[PATH_LEN];
    safef(pathBuffer, sizeof(pathBuffer), "/ENCODE/protocols/cell/%s/", organismOptLower);

    if (s && sameString(organismOpt, "Human"))
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
	s = hashFindVal(ra, "description");
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
	puts("</TR>");
	}
    else	// non-human cell type
	{
	puts("<TR>");

	printf("  <TD>%s</TD>\n", term);

	s = hashFindVal(ra, "description");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
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
	puts("</TR>");

	}
    }
else
    {
    s = hashFindVal(ra, "description");
    if(s != NULL)
        {
        puts("<TR>");
        printf("  <TD>%s</TD>\n", term);
        printf("  <TD colspan=40>%s</TD>\n", s); // Control term might be printed with other (e.g. Antibody)
        puts("</TR>");
        }
    else
        {
        printf("<TR>\n  <TD>%s</TD>\n  <TD>Unrecognised term</TD>\n</TR>\n", term);
        errAbort("Error: Unrecognised type (%s)\n", type);
        }
    }
return TRUE;
}

static char *normalizeType(char *type)
/* Strips any quotation marks and converts common synonyms */
{
(void)stripChar(type,'\"');
if ((sameWord(type,"Cell Line"))
||  (sameWord(type,"cellLine" ))
||  (sameWord(type,"Cell Type"))
||  (sameWord(type,"cellType" )))
    return cloneString("Cell Line");
else if (sameWord(type,"Factor"))
    return cloneString("Antibody");

return type;
}

static char *findType(struct hash *cvHash,char **requested,int requestCount,char *termOrTag)
/* returns the type that was requested or else the type associated with the term requested */
{
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct hash *ra;
char *type = typeOpt;

if (requested != NULL) // if no type, finds it from requested terms.  Will validate that terms match type
    {
    while ((hEl = hashNext(&hc)) != NULL)
        {
        ra = (struct hash *)hEl->val;
        int ix = stringArrayIx(hashMustFindVal(ra, termOrTag),requested,requestCount);
        if(ix != -1) // found
            {
            char *thisType = hashMustFindVal(ra, "type");
            if(type == NULL)
                type = thisType;
            else if(differentWord(type,thisType) && differentWord("control",thisType))  // ignores terms not in hash, but catches this:
                errAbort("Error: Requested %ss of type '%s'.  But '%s' has type '%s'\n",
                         termOrTag,type,requested[ix],thisType);
            }
        }
    }
if (type == NULL)    // Still not type? abort
    errAbort("Error: Required 'type', 'tag', or 'term' argument not found\n");
return normalizeType(type);
}

void doMiddle()
{
struct hash *cvHash = raReadAll(cgiUsualString("ra", cv_file()), "term");
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct slList *termList = NULL;
struct hash *ra;
char *type;
int totalPrinted = 0;

// Prepare an array of selected terms (if any)
int requestCount = 0;
char **requested = NULL;
char *requestVal = termOpt;
char *termOrTag = "term";
if (tagOpt)
    {
    requestVal = tagOpt;
    termOrTag = "tag";
    }
if (requestVal)
    {
    (void)stripChar(requestVal,'\"');
    requestCount = chopCommas(requestVal,NULL);
    requested = needMem(requestCount * sizeof(char *));
    chopByChar(requestVal,',',requested,requestCount);
    }

puts("<TABLE BORDER=1 BGCOLOR=#FFFEE8 CELLSPACING=0 CELLPADDING=2>");
puts("<TR style=\"background:#D9E4F8\">");
type = findType(cvHash,requested,requestCount,termOrTag);
doTypeHeader(type);
puts("</TR>");

// Get just the terms that match type and requested, then sort them
while ((hEl = hashNext(&hc)) != NULL)
    {
    ra = (struct hash *)hEl->val;
    char *thisType = hashMustFindVal(ra,"type");
    if(differentWord(thisType,type) && (requested == NULL || differentWord(thisType,"control")))
        continue;
    // Skip all rows that do not match term or tag if specified
    if(requested && -1 == stringArrayIx(hashMustFindVal(ra, termOrTag),requested,requestCount))
        continue;
    slAddTail(&termList, ra);
    }
slSort(&termList, termCmp);

// Print out the terms
while((ra = slPopHead(&termList)) != NULL)
    {
    if(doTypeRow( ra ))
        totalPrinted++;
    }
puts("</TABLE><BR>");
if(totalPrinted > 1)
    printf("Total = %d\n", totalPrinted);
}

int main(int argc, char *argv[])
/* Process command line */
{
cgiSpoof(&argc, argv);
termOpt = cgiOptionalString("term");
tagOpt = cgiOptionalString("tag");
typeOpt = cgiOptionalString("type");
organismOpt = cgiUsualString("organism", organismOpt);
organismOptLower=cloneString(organismOpt);
strLower(organismOptLower);
char *bgColor = cgiOptionalString("bgcolor");
if (bgColor)
    htmlSetBgColor(strtol(bgColor, 0, 16));
htmlSetStyle(htmlStyleUndecoratedLink);
htmShell("ENCODE Controlled Vocabulary", doMiddle, "get");
return 0;
}

