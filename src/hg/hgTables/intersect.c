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

static char const rcsid[] = "$Id: intersect.c,v 1.4 2004/07/21 04:15:01 kent Exp $";

/* We keep two copies of variables, so that we can
 * cancel out of the page. */

static char *curVars[] = {hgtaIntersectGroup, hgtaIntersectTrack,
	hgtaIntersectOp, hgtaMoreThreshold, hgtaLessThreshold,
	hgtaInvertTable, hgtaInvertTable2,
	};
static char *nextVars[] = {hgtaNextIntersectGroup, hgtaNextIntersectTrack,
	hgtaNextIntersectOp, hgtaNextMoreThreshold, hgtaNextLessThreshold,
	hgtaNextInvertTable, hgtaNextInvertTable2,
	};

boolean anyIntersection()
/* Return TRUE if there's an intersection to do. */
{
return cartVarExists(cart, hgtaIntersectTrack);
}

static char *onChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
jsDropDownCarryOver(dy, hgtaNextIntersectGroup);
jsDropDownCarryOver(dy, hgtaNextIntersectTrack);
jsTrackedVarCarryOver(dy, hgtaNextIntersectOp, "op");
jsTextCarryOver(dy, hgtaNextMoreThreshold);
jsTextCarryOver(dy, hgtaNextLessThreshold);
jsTrackedVarCarryOver(dy, hgtaNextInvertTable, "invertTable");
jsTrackedVarCarryOver(dy, hgtaNextInvertTable2, "invertTable2");
return dy;
}

static char *onChangeEither()
/* Get group-changing javascript. */
{
struct dyString *dy = onChangeStart();
return onChangeEnd(&dy);
}

void makeOpButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
jsMakeTrackingRadioButton(hgtaNextIntersectOp, "op", val, selVal);
}

void doIntersectMore(struct sqlConnection *conn)
/* Continue working in intersect page. */
{
struct trackDb *iTrack;
char *name = curTrack->shortLabel;
char *iName;
char *onChange = onChangeEither();
char *op, *setting;
htmlOpen("Intersect with %s", name);


hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);
hPrintf("<TABLE BORDER=0>\n");
/* Print group and track line. */
iTrack = showGroupTrackRow(hgtaNextIntersectGroup, onChange, 
	hgtaNextIntersectTrack, onChange, conn);
iName = iTrack->shortLabel;
hPrintf("</TABLE>\n");
hPrintf("<BR>\n");

hPrintf("These combinations will maintain the gene/alignment structure (if any) of %s: <P>\n",
       name);
op = cartUsualString(cart, hgtaNextIntersectOp, "any");
jsTrackingVar("op", op);
makeOpButton("any", op);
printf("All %s records that have any overlap with %s <BR>\n",
       name, iName);
makeOpButton("none", op);
printf("All %s records that have no overlap with %s <BR>\n",
       name, iName);
makeOpButton("more", op);
printf("All %s records that have at least ",
       name);
setting = cartCgiUsualString(cart, hgtaNextMoreThreshold, "80");
cgiMakeTextVar(hgtaNextMoreThreshold, setting, 3);
printf(" %% overlap with %s <BR>\n", iName);
makeOpButton("less", op);
printf("All %s records that have at most ",
       name);
setting = cartCgiUsualString(cart, hgtaNextLessThreshold, "80");
cgiMakeTextVar(hgtaNextLessThreshold, setting, 3);
printf(" %% overlap with %s <P>\n", iName);

printf("These combinations will discard the gene/alignment structure (if any) of %s and produce a simple list of position ranges.\n",
       name);
puts("To complement a table means to consider a position included if it \n"
     "is <I>not</I> included in the table. <P>");
makeOpButton("and", op);
printf("Base-pair-wise intersection (AND) of %s and %s <BR>\n",
       name, iName);
makeOpButton("or", op);
printf("Base-pair-wise union (OR) of %s and %s <P>\n",
       name, iName);
jsMakeTrackingCheckBox(hgtaNextInvertTable, "invertTable", FALSE);
printf("Complement %s before intersection/union <BR>\n", name);
jsMakeTrackingCheckBox(hgtaNextInvertTable2, "invertTable2", FALSE);
printf("Complement %s before intersection/union <P>\n", iName);

cgiMakeButton(hgtaDoIntersectSubmit, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[32];
    int varCount = ArraySize(nextVars);
    memcpy(saveVars, nextVars, varCount * sizeof(saveVars[0]));
    saveVars[varCount] = hgtaDoIntersectMore;
    jsCreateHiddenForm(saveVars, varCount+1);
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


