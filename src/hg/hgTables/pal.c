#include "common.h"
#include "trackDb.h"
#include "cart.h"
#include "hgTables.h"
#include "pal.h"

static char const rcsid[] = "$Id: pal.c,v 1.15 2008/10/16 15:58:12 ann Exp $";

boolean isPalCompatible(struct sqlConnection *conn,
    struct trackDb *track, char *table)
/* Return TRUE if table is genePred and there is a maf. */
{
char setting[128], *p = setting;

if (track == NULL)
    return FALSE;
if (isEmpty(track->type))
    return FALSE;
safecpy(setting, sizeof setting, track->type);
char *type = nextWord(&p);

if (!sameString(type, "genePred"))
    return FALSE;

if (!sameString(track->tableName, table))
    return FALSE;

/* we also check for a maf table */
struct slName *list = hTrackTablesOfType(conn, "wigMaf%%");

if (list != NULL)
    {
    slFreeList(&list);
    return TRUE;
    }

return FALSE;
}

void doGenePredPal(struct sqlConnection *conn)
/* Output genePred protein alignment. */
{
if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
    {
    sendParamsToGalaxy(hgtaDoPalOut, "submit");
    return;
    }

/* get rid of pesky cookies that would bring us back here */
cartRemove(cart, hgtaDoPal);
cartRemove(cart, hgtaDoPalOut);
cartSaveSession(cart);

if (anyIntersection() && intersectionIsBpWise())
    errAbort("Can't do CDS FASTA output when bit-wise intersection is on. "
    "Please go back and select another output type, or clear the intersection.");

struct lm *lm = lmInit(64*1024);
int fieldCount;
struct bed *bedList = cookedBedsOnRegions(conn, curTable, getRegions(),
	lm, &fieldCount);

//lmCleanup(&lm);

textOpen();

int outCount = palOutPredsInBeds(conn, cart, bedList, curTable);

/* Do some error diagnostics for user. */
if (outCount == 0)
    explainWhyNoResults(NULL);
}

void addOurButtons()
/* callback from options dialog to add navigation buttons */
{
printf("For information about output data format see the"
  "<A HREF=\"../goldenPath/help/hgTablesHelp.html#FASTA\">User's Guide</A><BR>");
cgiMakeButton(hgtaDoPalOut, "get output");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("<input type=\"hidden\" name=\"%s\" value=\"\">\n", hgtaDoPal);
}

void doOutPalOptions(struct sqlConnection *conn)
/* Output pal page. */
{
htmlOpen("Select multiple alignment and options for %s", curTrack->shortLabel);
palOptions(cart, conn, addOurButtons, hgtaDoPal);
htmlClose();
}
