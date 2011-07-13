/* hgEncodeMatrix - print high-level matrix view of experient table

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "htmshell.h"
#include "ra.h"
#include "hui.h"
#include "cv.h"
#include "encode/encodeExp.h"

/* hgEncodeMatrix - A CGI script to display the different types of encode controlled vocabulary.
 * usage:
 *   hgEncodeMatrix tier=(1+2|3)]
 * options:\n"
 *    tier=1+2|3 
 *    bgcolor=RRGGBB : Change background color (hex digits)
 */

//options that apply to all vocab types

#ifdef BAD

#define ORGANISM           "organism"

#define MAX_TABLE_ROWS     10
#define TABLE_ROWS_AVAILABLE(rowsUsed) (MAX_TABLE_ROWS - (rowsUsed))

static char *termOpt = NULL;
static char *tagOpt = NULL;
static char *typeOpt = NULL;
static char *targetOpt = NULL;
static char *labelOpt = NULL;
static char *organismOpt = NULL; // default to human if nothing else is set
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

boolean doTypeDefinition(char *type,boolean inTable,boolean showType)
// Write out description of type the type if it is known
{
struct hash *typeHash = (struct hash *)cvTermTypeHash();

struct hash *ra = hashFindVal(typeHash,cvTermNormalized(type)); // Find by term
if (ra == NULL)
    return FALSE;

char *val = hashMustFindVal(ra, CV_DESCRIPTION);
char *label = hashFindVal(ra, CV_LABEL);

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

#endif

void doHeader(struct slName *types)
/* Print header row of data types */
{
printf("<TR style='background:%s;'>\n",COLOR_BG_HEADER_LTBLUE);
while (sl(slName = PopHead(types)) != NULL)_
//printf("  <TH>%s</TH><TH>Tier</TH><TH>Description</TH><TH>Lineage</TH><TH>Karyotype</TH><TH>Sex</TH><TH>Documents</TH><TH>Vendor ID</TH><TH>Term ID</TH><TH><I>Label</I></TH>",type);
puts("\TR");
}

void doRow(char *term)
/* Print row for a cell type */
{
puts("<TR>");
printf("  <TD>%s</TD>\n", term);
puts("</TR>");
}

void doMiddle()
{
struct hash *cellTypeHash, dataTypeHash;
struct hashCookie hc;
struct hashEl *hel;
struct slName *cellTypes = NULL, dataTypes = NULL;

/* get list of data types types from controlled vocab */
dataTypeHash = cvTermHash(CV_TERM_DATA_TYPE);
hc = hashFirst(dataTypeHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    struct hash *dataTypeHash = (struct hash *)hel->val;
    slAddHead(&dataTypes, slNameNew(hel->name));
    }
slNameSort(&dataTypes, slNameCmp);

/* get list of tier1 and tier2 cell types from controlled vocab */
cellTypeHash = cvTermHash(CV_TERM_CELL);
hc = hashFirst(cellTypeHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    struct hash *cellTypeHash = (struct hash *)hel->val;
    // TODO;select by organism
    if ((tier = hashFindVal("tier")) != NULL) &&
        (sameString(tier, "1") || sameString(tier, "2"))
            slAddHead(&cellTypes, slNameNew(hel->name));
    }
slNameSort(&cellTypes, slNameCmp);

// Prepare an array of selected terms (if any)
char *queryBy = CV_TERM;

char *org = organismOptLower;
if (org == NULL)
    org = ENCODE_EXP_ORGANISM_HUMAN;

printf("<TABLE BORDER=1 BGCOLOR=%s CELLSPACING=0 CELLPADDING=2>\n",COLOR_BG_DEFAULT);
doHeader(dataTypes);
while ((term = slPopHead(&cellTypes)) != NULL)
    doRow(term)
puts("</TABLE><BR>");
}

int main(int argc, char *argv[])
/* Process command line */
{
cgiSpoof(&argc, argv);
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
htmShell("ENCODE Experiment Matrix View ", doMiddle, "get");
return 0;
}

