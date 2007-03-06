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

static char const rcsid[] = "$Id: identifiers.c,v 1.10 2007/03/06 22:39:17 hiram Exp $";


void doPasteIdentifiers(struct sqlConnection *conn)
/* Respond to paste identifiers button. */
{
char *oldPasted = cartUsualString(cart, hgtaPastedIdentifiers, "");
htmlOpen("Paste In Identifiers for %s", curTableLabel());
hPrintf("<FORM ACTION=\"..%s\" METHOD=POST>\n", getScriptName());
cartSaveSession(cart);
hPrintf("Please paste in the identifiers you want to include.<BR>\n");
cgiMakeTextArea(hgtaPastedIdentifiers, oldPasted, 10, 70);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoPastedIdentifiers, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoClearPasteIdentifierText, "clear");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>");
htmlClose();
}

void doUploadIdentifiers(struct sqlConnection *conn)
/* Respond to upload identifiers button. */
{
htmlOpen("Upload Identifiers for %s", curTableLabel());
hPrintf("<FORM ACTION=\"..%s\" METHOD=POST ENCTYPE=\"multipart/form-data\">\n",
	getScriptName());
cartSaveSession(cart);
hPrintf("Please enter the name of a file from your computer that contains a ");
hPrintf("space, tab, or ");
hPrintf("line separated list of the items you want to include.<BR>");
hPrintf("<INPUT TYPE=FILE NAME=\"%s\"> ", hgtaPastedIdentifiers);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoPastedIdentifiers, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>");
htmlClose();
}

void doPastedIdentifiers(struct sqlConnection *conn)
/* Process submit in past identifiers page. */
{
char *idText = trimSpaces(cartString(cart, hgtaPastedIdentifiers));
htmlOpen("Table Browser (Input Identifiers)");
if (idText != NULL && idText[0] != 0)
    {
    /* Write variable to temp file and save temp
     * file name. */
    struct tempName tn;
    FILE *f;
    trashDirFile(&tn, "hgtData", "identifiers", ".key");
    f = mustOpen(tn.forCgi, "w");
    mustWrite(f, idText, strlen(idText));
    carefulClose(&f);
    cartSetString(cart, hgtaIdentifierTable, curTable);
    cartSetString(cart, hgtaIdentifierFile, tn.forCgi);
    if (strlen(idText) > 64 * 1024)
         cartRemove(cart, hgtaPastedIdentifiers);
    }
else
    {
    cartRemove(cart, hgtaIdentifierFile);
    }
mainPageAfterOpen(conn);
htmlClose();
}


char *identifierFileName()
/* File name identifiers are in, or NULL if no such file. */
{
char *fileName = cartOptionalString(cart, hgtaIdentifierFile);
char *identifierTable = cartOptionalString(cart, hgtaIdentifierTable);
if (fileName == NULL)
    return NULL;
if (identifierTable != NULL && !sameString(identifierTable, curTable) &&
    !sameString(connectingTableForTrack(identifierTable), curTable))
    return NULL;
if (fileExists(fileName))
    return fileName;
else
    {
    cartRemove(cart, hgtaIdentifierFile);
    return NULL;
    }
}

struct hash *identifierHash()
/* Return hash full of identifiers. */
{
char *fileName = identifierFileName();
if (fileName == NULL)
    return NULL;
else
    {
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct hash *hash = hashNew(18);
    char *line, *word;
    while (lineFileNext(lf, &line, NULL))
        {
	while ((word = nextWord(&line)) != NULL)
	    hashAdd(hash, word, NULL);
	}
    lineFileClose(&lf);
    return hash;
    }
}

void doClearPasteIdentifierText(struct sqlConnection *conn)
/* Respond to clear within paste identifier page. */
{
cartRemove(cart, hgtaPastedIdentifiers);
doPasteIdentifiers(conn);
}

void doClearIdentifiers(struct sqlConnection *conn)
/* Respond to clear identifiers button. */
{
char *fileName;

htmlOpen("Table Browser (Cleared Identifiers)");
fileName = identifierFileName();
if (fileName != NULL)
    remove(fileName);
cartRemove(cart, hgtaIdentifierFile);
mainPageAfterOpen(conn);
htmlClose();
}


