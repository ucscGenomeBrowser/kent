/* identifiers - handle identifier lists: uploading, pasting,
 * and restricting to just things on the list. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "userRegions.h"
#include "web.h"

static int maxRegions = 1000;
static int maxErrors = 100;

void doSetUserRegionsAfterOpen(struct sqlConnection *conn)
/* Respond to set regions button. */
{
char *oldPasted = cartUsualString(cart, hgtaEnteredUserRegions, "");
char *db = cartOptionalString(cart, hgtaUserRegionsDb);
if (db && !sameString(db, database))
    oldPasted = "";
hPrintf("<FORM ACTION=\"%s\" METHOD=%s "
        " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n", getScriptName(),
        cartUsualString(cart, "formMethod", "POST"));
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

void doSetUserRegions(struct sqlConnection *conn)
/* Respond to set regions button. */
{
htmlOpen("Enter region definition\n");
doSetUserRegionsAfterOpen(conn);
}

static char *limitText(char *text)
/* read text string and limit to maxRegions actual data lines */
{
struct dyString *limitedText = dyStringNew(0);
/* Even if using FALSE for zTerm, lineFile still does a memmove when it hits the end
 * and thus clobbers the string, so call lineFileOnString on a copy: */
char copy[strlen(text)+1];
safecpy(copy, sizeof(copy), text);
struct lineFile *lf = lineFileOnString("limitText", FALSE, copy);
char *lineStart = NULL;
int lineLength = 0;
int legitimateLineCount = 0;
while (legitimateLineCount < maxRegions && lineFileNext(lf, &lineStart, &lineLength))
    {
    char *s, c;
    s = skipLeadingSpaces(lineStart);
    c = s[0];
    if (c != 0 && c != '#')
	++legitimateLineCount;
    dyStringAppendN(limitedText, lineStart, lineLength);
    }
if ((legitimateLineCount == maxRegions) && lineFileNext(lf, &lineStart, &lineLength))
    warn("WARNING: defined regions limit of %d definitions reached at line %d<BR>\n",
         maxRegions, lf->lineIx-1);
lineFileClose(&lf);
return (dyStringCannibalize(&limitedText));
}

static void cartRemoveUserRegions()
/* Remove all cart variables related to storage of user regions. */
{
cartRemove(cart, hgtaEnteredUserRegions);
cartRemove(cart, hgtaEnteredUserRegionFile);
cartRemove(cart, hgtaUserRegionsFile);
cartRemove(cart, hgtaUserRegionsDb);
cartRemove(cart, hgtaRegionType);
}

void doSubmitUserRegions(struct sqlConnection *conn)
/* Process submit in set regions page. */
{
char *idText = trimSpaces(cartString(cart, hgtaEnteredUserRegions));
char *userRegionFile = trimSpaces(cartString(cart, hgtaEnteredUserRegionFile));

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

char *lineLimitText = limitText(idText);
if ( (strlen(lineLimitText) > 0) && (strlen(lineLimitText) != strlen(idText)) )
    {
    idText = lineLimitText;
    cartSetString(cart, hgtaEnteredUserRegions, lineLimitText);
    }
else
    freeMem(lineLimitText);

boolean success = TRUE;
if (isNotEmpty(idText))
    {
    int regionCount = 0;
    char *warnText = NULL;
    char *trashFileName = userRegionsParse(database, idText, maxRegions, maxErrors,
                                           &regionCount, &warnText);
    if (isNotEmpty(warnText))
        {
        success = FALSE;
        warn("%s", warnText);
        }
    if (regionCount == 0)
        {
        success = FALSE;
	warn("No valid regions found in input; see below for formatting instructions");
        }
    cartSetString(cart, hgtaUserRegionsDb, database);
    cartSetString(cart, hgtaUserRegionsFile, trashFileName);
    cartSetString(cart, hgtaRegionType, hgtaRegionTypeUserRegions);
    if (strlen(idText) > 64 * 1024)
         cartRemove(cart, hgtaEnteredUserRegions);
    cartRemove(cart, hgtaEnteredUserRegionFile);
    }
else
    {
    cartRemoveUserRegions();
    }
if (success)
    mainPageAfterOpen(conn);
else
    doSetUserRegionsAfterOpen(conn);
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
    cartRemoveUserRegions();
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
lineFileClose(&lf);
return list;
}

void doClearSetUserRegionsText(struct sqlConnection *conn)
/* Respond to clear within user regions enter page. */
{
char *fileName = userRegionsFileName();
if (fileName != NULL)
    remove(fileName);
cartRemoveUserRegions();
doSetUserRegions(conn);
}

void doClearUserRegions(struct sqlConnection *conn)
/* Respond to clear user regions button. */
{
char *fileName = userRegionsFileName();

htmlOpen("Table Browser (Cleared Region List)");
if (fileName != NULL)
    remove(fileName);
cartRemoveUserRegions();
mainPageAfterOpen(conn);
htmlClose();
}
