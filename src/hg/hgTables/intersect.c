/* intersect - handle intersecting beds. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "portable.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: intersect.c,v 1.1 2004/07/21 00:30:23 kent Exp $";

/* We keep two copies of variables, so that we can
 * cancel out of the page. */

static char *curVars[] = {hgtaIntersectGroup, hgtaIntersectTrack};
static char *nextVars[] = {hgtaNextIntersectGroup, hgtaNextIntersectTrack};

boolean anyIntersection()
/* Return TRUE if there's an intersection to do. */
{
return cartVarExists(cart, hgtaIntersectTrack);
}

static char *onChangeGroup()
/* Get group-changing javascript. */
{
return "onChange=\"document.hiddenForm.submit();\"";
}

void doIntersectMore(struct sqlConnection *conn)
/* Continue working in intersect page. */
{
htmlOpen("Intersect with %s", curTrack->shortLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);
hPrintf("<TABLE BORDER=0>\n");
/* Print group and track line. */
showGroupTrackRow(hgtaNextIntersectGroup, onChangeGroup(), 
	hgtaNextIntersectTrack, conn);
hPrintf("</TABLE>\n");
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoIntersectSubmit, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      hgtaDoIntersectMore, hgtaNextIntersectGroup, };
    createHiddenForm(saveVars, ArraySize(saveVars));
    }

htmlClose();
}

void removeCartVars(struct cart *cart, char **vars, int varCount)
/* Remove array of variables from cart. */
{
int i;
for (i=0; i<varCount; ++i)
    cartRemove(cart, vars[i]);
}

void copyCartVars(struct cart *cart, char **source, char **dest, int count)
/* Copy from source to dest. */
{
int i;
for (i=0; i<count; ++i)
    {
    char *s = cartOptionalString(cart, source[i]);
    if (s != NULL)
        cartSetString(cart, dest[i], s);
    else
        cartRemove(cart, dest[i]);
    }
}


void cartRemove(struct cart *cart, char *var);
/* Remove variable from cart. */

void doIntersectPage(struct sqlConnection *conn)
/* Respond to intersect create/edit button */
{
copyCartVars(cart, curVars, nextVars, ArraySize(curVars));
doIntersectMore(conn);
}

void doClearIntersect(struct sqlConnection *conn)
/* Respond to click on clear intersection. */
{
removeCartVars(cart, curVars, ArraySize(nextVars));
doMainPage(conn);
}

void doIntersectSubmit(struct sqlConnection *conn)
/* Respond to submit on intersect page. */
{
copyCartVars(cart, nextVars, curVars, ArraySize(curVars));
doMainPage(conn);
}


