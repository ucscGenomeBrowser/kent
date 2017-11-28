/* Stuff lifted from hgTables that should be libified. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "cheapcgi.h"
#include "customTrack.h"
#include "grp.h"
#include "hdb.h"
#include "hgFind.h"
#include "hgMaf.h"
#include "hui.h"
#include "joiner.h"

#include "libifyMe.h"
#include "windowsToAscii.h"

static boolean searchPosition(char *range, struct cart *cart, char *cartVar)
/* Try and fill in region via call to hgFind. Return FALSE
 * if it can't find a single position. */
{
struct hgPositions *hgp = NULL;
char retAddr[512];
char position[512];
char *chrom = NULL;
int start=0, end=0;
char *db = cloneString(cartString(cart, "db")); // gets clobbered if position is not found!
safef(retAddr, sizeof(retAddr), "%s", cgiScriptName());
hgp = findGenomePosWeb(db, range, &chrom, &start, &end,
	cart, TRUE, retAddr);
if (hgp != NULL && hgp->singlePos != NULL)
    {
    safef(position, sizeof(position),
	    "%s:%d-%d", chrom, start+1, end);
    cartSetString(cart, cartVar, position);
    return TRUE;
    }
else if (start == 0)	/* Confusing way findGenomePosWeb says pos not found. */
    {
    cartSetString(cart, cartVar, hDefaultPos(db));
    return FALSE;
    }
else
    return FALSE;
}

boolean lookupPosition(struct cart *cart, char *cartVar)
/* Look up position if it is not already seq:start-end.  Return FALSE if it puts
 * up multiple positions. */
{
char *db = cartString(cart, "db");
char *range = windowsToAscii(cartUsualString(cart, cartVar, ""));
boolean isSingle = TRUE;
range = trimSpaces(range);
if (range[0] != 0)
    isSingle = searchPosition(range, cart, cartVar);
else
    cartSetString(cart, cartVar, hDefaultPos(db));
return isSingle;
}

//#*** duplicated many places... htmlshell?
void nbSpaces(int count)
/* Print some non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    printf("&nbsp;");
}

