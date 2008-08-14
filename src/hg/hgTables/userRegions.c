/* identifiers - handle identifier lists: uploading, pasting,
 * and restricting to just things on the list. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "portable.h"
#include "hgTables.h"
#include "trashDir.h"
#include "hui.h"
#include "obscure.h"
#include "web.h"

static char const rcsid[] = "$Id: userRegions.c,v 1.12.14.2 2008/08/14 15:54:11 markd Exp $";

void doSetUserRegions(struct sqlConnection *conn)
/* Respond to set regions button. */
{
char *oldPasted = cartUsualString(cart, hgtaEnteredUserRegions, "");
char *db = cartOptionalString(cart, hgtaUserRegionsDb);
if (db && !sameString(db, database))
    oldPasted = "";
htmlOpen("Enter region definition\n");
hPrintf("<FORM ACTION=\"%s\" METHOD=POST "
    " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n", getScriptName());
cartSaveSession(cart);
hPrintf("<TABLE><TR><TD ALIGN=LEFT>\n");
hPrintf("Paste regions:");
hPrintf("</TD><TD ALIGN=RIGHT>");
hPrintf("Or upload file: <INPUT TYPE=FILE NAME=\"%s\">&nbsp;<BR>\n",
	hgtaEnteredUserRegionFile);
hPrintf("</TD></TR><TR><TD COLSPAN=2 ALIGN=LEFT>\n");
cgiMakeTextArea(hgtaEnteredUserRegions, oldPasted, 10, 70);
hPrintf("</TD></TR><TR><TD COLSPAN=2 ALIGN=LEFT>\n");
cgiMakeButton(hgtaDoSubmitUserRegions, "submit");
hPrintf("&nbsp;");
cgiMakeButton(hgtaDoClearSetUserRegionsText, "clear");
hPrintf("&nbsp;");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</TD></TR></TABLE>");
hPrintf("</FORM><BR>\n");
webIncludeHelpFile("hgTbUserRegionsHelp", FALSE);
htmlClose();
}

static boolean illegalCoordinate(char *chrom, int start, int end)
/* verify start and end are legal for this chrom */
{
int maxEnd = hChromSize(database, chrom);
if (start < 0)
    {
    warn("chromStart (%d) less than zero", start);
    return TRUE;
    }
if (end > maxEnd)
    {
    warn("chromEnd (%d) greater than chrom length (%s:%d)", end, chrom, maxEnd);
    return TRUE;
    }
if (start >= end)
    {
    warn("chromStart (%d) must be less than chromEnd (%s:%d)", start, chrom, end);
    return TRUE;
    }
return FALSE;
}

static struct bed *parseRegionInput(char *inputString)
/* scan the user region definition, turn into a bed list */
{
int itemCount = 0;
struct bed *bedList = NULL;
struct bed *bedEl;
int wordCount;
char *words[5];
struct lineFile *lf;

lf = lineFileOnString("userData", TRUE, inputString);
while (0 != (wordCount = lineFileChopNext(lf, words, ArraySize(words))))
    {
    char *chromName = NULL;
    int chromStart = 0;
    int chromEnd = 0;
    char *regionName = NULL;
    /*	might be something of the form: chrom:start-end optionalRegionName */
    if (((1 == wordCount) || (2 == wordCount)) &&
	    hgParseChromRange(NULL, words[0], &chromName,
		&chromStart, &chromEnd))
	{
	if (2 == wordCount)
	    regionName = cloneString(words[1]);
	}
    else if (!((3 == wordCount) || (4 == wordCount)))
	{
	int i;
	struct dyString *errMessage = dyStringNew(0);
	for (i = 0; i < wordCount; ++i)
	    dyStringPrintf(errMessage, "%s ", words[i]);
	errAbort("line %d: '%s'<BR>\n"
	"illegal bed size, expected 3 or 4 fields, found %d\n",
		    lf->lineIx, dyStringCannibalize(&errMessage), wordCount);
	}
    else
	{
	chromName = hgOfficialChromName(database, words[0]);
	chromStart = sqlSigned(words[1]);
	chromEnd = sqlSigned(words[2]);
	if (wordCount > 3)
	    regionName = cloneString(words[3]);
	}
    ++itemCount;
    if (itemCount > 1000)
	{
	warn("limit 1000 region definitions reached at line %d<BR>\n",
		lf->lineIx);
	break;
	}
    AllocVar(bedEl);
    bedEl->chrom = chromName;
    if (NULL == bedEl->chrom)
	errAbort("at line %d, chrom name '%s' %s %s not recognized in this assembly %d",
	    lf->lineIx, words[0], words[1], words[2], wordCount);
    bedEl->chromStart = chromStart;
    bedEl->chromEnd = chromEnd;
    if (illegalCoordinate(bedEl->chrom, bedEl->chromStart, bedEl->chromEnd))
	errAbort("illegal input at line %d: %s %d %d",
		lf->lineIx, bedEl->chrom, bedEl->chromStart, bedEl->chromEnd);
    if (wordCount > 3)
	bedEl->name = regionName;
    else
	bedEl->name = NULL;
/* if we wanted to give artifical names to each item */
#ifdef NOT
	{
	char name[128];
	safef(name, ArraySize(name), "item_%04d", itemCount);
	bedEl->name = cloneString(name);
	}
#endif
    slAddHead(&bedList, bedEl);
    }
lineFileClose(&lf);
//    slSort(&bedList, bedCmp);	/* this would do chrom,chromStart order */
slReverse(&bedList);	/* with no sort, it is in order as user entered */
return (bedList);
}

static char *limitText(char *text)
/* read text string and limit to 1000 actual data lines */
{
struct dyString *limitedText = dyStringNew(0);
/* yes, opening with FALSE so as not to destroy the original string */
struct lineFile *lf = lineFileOnString("limitText", FALSE, text);
char *lineStart = NULL;
int lineLength = 0;
int legitimateLineCount = 0;
while (legitimateLineCount < 1000 && lineFileNext(lf, &lineStart, &lineLength))
    {
    char *s, c;
    s = skipLeadingSpaces(lineStart);
    c = s[0];
    if (c != 0 && c != '#')
	++legitimateLineCount;
    dyStringAppendN(limitedText, lineStart, lineLength);
    }
if ((legitimateLineCount == 1000) && lineFileNext(lf, &lineStart, &lineLength))
    warn("WARNING: defined regions limit of 1000 definitions reached at line %d<BR>\n",
		lf->lineIx-1);
lineFileClose(&lf);
return (dyStringCannibalize(&limitedText));
}

void doSubmitUserRegions(struct sqlConnection *conn)
/* Process submit in set regions page. */
{
char *idText = trimSpaces(cartString(cart, hgtaEnteredUserRegions));
char *userRegionFile = trimSpaces(cartString(cart, hgtaEnteredUserRegionFile));
boolean hasData = (idText != NULL && idText[0] != 0) ||
    (userRegionFile != NULL && userRegionFile[0] != 0);

/* beware, the string pointers from cartString() point to strings in the
 * cart hash.  If they are manipulated and changed, they will get saved
 * back to the cart in their changed form.  You don't want to be
 * altering them like that.  Thus, the idText is duplicated below with
 * the cloneString(idText)
 */
htmlOpen("Table Browser (Region definitions)");

/* presence of fileName text overrides previously existing text area
 *	contents
 */
if (userRegionFile != NULL && userRegionFile[0] != 0)
    {
    idText = cloneString(userRegionFile);
    cartRemove(cart, hgtaEnteredUserRegions);
    cartRemove(cart, hgtaUserRegionsFile);
    cartSetString(cart, hgtaEnteredUserRegions, idText);
    }
else
    idText = cloneString(idText);

char *lineLimitText = limitText(idText);
if ( (strlen(lineLimitText) > 0) && (strlen(lineLimitText) != strlen(idText)) )
    {
    freeMem(idText);
    idText = lineLimitText;
    cartSetString(cart, hgtaEnteredUserRegions, lineLimitText);
    }
else
    freeMem(lineLimitText);

if (hasData)
    {
    struct tempName tn;
    FILE *f;
    struct bed *bedEl;
    struct bed *bedList = parseRegionInput(idText);

    if (NULL == bedList)
	errAbort("no valid data points found in input");

    trashDirFile(&tn, "hgtData", "user", ".region");
    f = mustOpen(tn.forCgi, "w");
    for (bedEl = bedList; bedEl; bedEl = bedEl->next )
	{
	if (bedEl->name)
	    fprintf(f, "%s\t%d\t%d\t%s\n",
		bedEl->chrom, bedEl->chromStart, bedEl->chromEnd, bedEl->name);
	else
	    fprintf(f, "%s\t%d\t%d\n",
		bedEl->chrom, bedEl->chromStart, bedEl->chromEnd);
	}
    carefulClose(&f);
    cartSetString(cart, hgtaUserRegionsDb, database);
    cartSetString(cart, hgtaUserRegionsTable, curTable);
    cartSetString(cart, hgtaUserRegionsFile, tn.forCgi);
    cartSetString(cart, hgtaRegionType, hgtaRegionTypeUserRegions);
    if (strlen(idText) > 64 * 1024)
         cartRemove(cart, hgtaEnteredUserRegions);
    }
else
    {
    cartRemove(cart, hgtaUserRegionsFile);
    cartRemove(cart, hgtaEnteredUserRegionFile);
    cartRemove(cart, hgtaRegionType);
    }
mainPageAfterOpen(conn);
htmlClose();
}

char *userRegionsFileName()
/* File name defined regions are in, or NULL if no such file. */
{
char *fileName = cartOptionalString(cart, hgtaUserRegionsFile);
char *db = cartOptionalString(cart, hgtaUserRegionsDb);
if (db && !sameString(database, db))
    return NULL;
if (fileName == NULL)
    return NULL;
if (fileExists(fileName))
    return fileName;
else
    {
    cartRemove(cart, hgtaUserRegionsFile);
    cartRemove(cart, hgtaRegionType);
    return NULL;
    }
}

struct region *getUserRegions(char *fileName)
/* Get user defined regions from fileName. */
{
struct region *list = NULL, *region;
struct lineFile *lf;
char *words[4];
int wordCount;

lf = lineFileOpen(fileName, TRUE); /* TRUE == replace CR with 0 */
while (0 != (wordCount = lineFileChopNext(lf, words, ArraySize(words))))
    {
    AllocVar(region);
    region->chrom = cloneString(words[0]);
    region->start = atoi(words[1]);
    region->end = atoi(words[2]);
    if (wordCount > 3)
	region->name = cloneString(words[3]);
    else
	region->name = NULL;
    slAddHead(&list, region);
    }
slReverse(&list);
return list;
}

void doClearSetUserRegionsText(struct sqlConnection *conn)
/* Respond to clear within user regions enter page. */
{
char *fileName = userRegionsFileName();
if (fileName != NULL)
    remove(fileName);
cartRemove(cart, hgtaEnteredUserRegions);
cartRemove(cart, hgtaEnteredUserRegionFile);
cartRemove(cart, hgtaUserRegionsFile);
cartRemove(cart, hgtaRegionType);
doSetUserRegions(conn);
}

void doClearUserRegions(struct sqlConnection *conn)
/* Respond to clear user regions button. */
{
char *fileName = userRegionsFileName();

htmlOpen("Table Browser (Cleared Region List)");
if (fileName != NULL)
    remove(fileName);
cartRemove(cart, hgtaEnteredUserRegions);
cartRemove(cart, hgtaEnteredUserRegionFile);
cartRemove(cart, hgtaUserRegionsFile);
cartRemove(cart, hgtaRegionType);
mainPageAfterOpen(conn);
htmlClose();
}
