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

static char const rcsid[] = "$Id: userRegions.c,v 1.2 2007/03/06 22:14:35 hiram Exp $";

void doSetUserRegions(struct sqlConnection *conn)
/* Respond to set regions button. */
{
char helpName[PATH_LEN], *helpBuf;
char *oldPasted = cartUsualString(cart, hgtaEnteredUserRegions, "");
htmlOpen("Enter region definition\n");
hPrintf("<FORM ACTION=\"..%s\" METHOD=POST "
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
safef(helpName, 256, "%s%s/%s.html", hDocumentRoot(), HELP_DIR, "hgTbUserRegionsHelp");
if (fileExists(helpName))
    readInGulp(helpName, &helpBuf, NULL);
else
    {
    char missingHelp[512];
    safef(missingHelp, ArraySize(missingHelp),
	"<P>(missing help text file in %s)</P>\n", helpName);
    helpBuf = cloneString(missingHelp);
    }
puts(helpBuf);
htmlClose();
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
 * altering them like that.
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

if (hasData)
    {
    /* Write variable to temp file and save temp
     * file name. */
    int itemCount = 0;
    struct bed *bedList = NULL;
    struct tempName tn;
    struct bed *bedEl;
    int wordCount;
    char *words[5];
    struct lineFile *lf;
    FILE *f;
    lf = lineFileOnString("userData", TRUE, idText);
    while (0 != (wordCount = lineFileChopNext(lf, words, ArraySize(words))))
	{
	if (!((3 == wordCount) || (4 == wordCount)))
	    {
	    errAbort("%s %s %s<BR>\n"
	    "illegal bed size, expected 3 or 4 fields, found %d at line %d\n",
			words[0], words[1], words[2], wordCount, lf->lineIx );
	    }
	++itemCount;
	if (itemCount > 1000)
	    {
	    warn("limit 1000 region definitions reached<BR>\n");
	    break;
	    }
	AllocVar(bedEl);
	bedEl->chrom = cloneString(words[0]);
	bedEl->chromStart = sqlUnsigned(words[1]);
	bedEl->chromEnd = sqlUnsigned(words[2]);
	if (wordCount > 3)
	    bedEl->name = cloneString(words[3]);
	else
	    bedEl->name = NULL;
#ifdef NOT
	    {
	    char name[128];
	    safef(name, ArraySize(name), "item_%04d", ++itemCount);
	    bedEl->name = cloneString(name);
	    }
#endif
	slAddHead(&bedList, bedEl);
	}
    lineFileClose(&lf);
    slSort(&bedList, bedCmp);

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
return list;
}

void doClearSetUserRegionsText(struct sqlConnection *conn)
/* Respond to clear within user regions enter page. */
{
cartRemove(cart, hgtaEnteredUserRegions);
cartRemove(cart, hgtaEnteredUserRegionFile);
cartRemove(cart, hgtaRegionType);
doSetUserRegions(conn);
}

void doClearUserRegions(struct sqlConnection *conn)
/* Respond to clear user regions button. */
{
char *fileName;

htmlOpen("Table Browser (Cleared Region List)");
fileName = userRegionsFileName();
if (fileName != NULL)
    remove(fileName);
cartRemove(cart, hgtaUserRegionsFile);
cartRemove(cart, hgtaRegionType);
mainPageAfterOpen(conn);
htmlClose();
}


