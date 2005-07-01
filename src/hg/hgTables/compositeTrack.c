/* compositeTrack -- handle composite tracks / subtrack merge. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "bed.h"
#include "hdb.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: compositeTrack.c,v 1.2 2005/07/01 00:56:32 angie Exp $";

/* We keep two copies of variables, so that we can
 * cancel out of the page. */
static char *curVars[] = {hgtaSubtrackMergePrimary,
			  hgtaSubtrackMergeOp,
			  hgtaSubtrackMergeMoreThreshold,
			  hgtaSubtrackMergeLessThreshold,
	};
static char *nextVars[] = {hgtaNextSubtrackMergePrimary,
			   hgtaNextSubtrackMergeOp,
			   hgtaNextSubtrackMergeMoreThreshold,
			   hgtaNextSubtrackMergeLessThreshold,
	};

boolean anySubtrackMerge(char *db, char *table)
/* Return TRUE if a subtrack merge has been specified on db.table. */
{
char *smp = cartOptionalString(cart, hgtaSubtrackMergePrimary);
if (smp == NULL)
    return FALSE;
else
    {
    char *dbTable = getDbTable(db, table);
    boolean curTableHasMerge = sameString(smp, dbTable);
    freez(&dbTable);
    return curTableHasMerge;
    }
}

boolean subtrackMergeIsBpWise()
/* Return TRUE if the subtrack merge operation is base pair-wise. */
{
char *op = cartUsualString(cart, hgtaSubtrackMergeOp, "any");
return (sameString(op, "and") || sameString(op, "or"));
}

boolean isSubtrackMerged(char *tableName)
/* Return true if tableName has been selected for subtrack merge. */
{
char option[64];
safef(option, sizeof(option), "%s_sel", tableName);
return cartUsualBoolean(cart, option, FALSE);
}

static void makeOpButton(char *val, char *selVal)
/* Make merge-op radio button. */
{
cgiMakeRadioButton(hgtaNextSubtrackMergeOp, val, sameString(val, selVal));
}

void doSubtrackMergeMore(struct sqlConnection *conn)
/* Respond to subtrack merge create/edit button */
{
struct trackDb *subtrack = NULL;
char *dbTable = getDbTable(database, curTable);
char *op = NULL;
char *primaryLongLabel = NULL, *primaryType = NULL;
char *setting = NULL;

htmlOpen("Merge subtracks of %s (%s)",
	 curTrack->tableName, curTrack->longLabel);

hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);
/* Currently selected subtrack table will be the primary subtrack in the 
 * merge. */
cgiMakeHiddenVar(hgtaNextSubtrackMergePrimary, dbTable);

for (subtrack = curTrack->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (sameString(curTable, subtrack->tableName))
	{
	primaryLongLabel = subtrack->longLabel;
	primaryType = subtrack->type;
	break;
	}
    }
hPrintf("<H3>Select a subset of subtracks to merge with %s:</H3>\n",
	primaryLongLabel);
hPrintf("<TABLE>\n");
slSort(&(curTrack->subtracks), trackDbCmp);
for (subtrack = curTrack->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (! sameString(curTable, subtrack->tableName) &&
	sameString(primaryType, subtrack->type))
	{
	char option[64];
	char *words[2];
	boolean alreadySet = TRUE;
	puts("<TR><TD>");
	safef(option, sizeof(option), "%s_sel", subtrack->tableName);
	if ((setting = trackDbSetting(curTrack, "subTrack")) != NULL)
	    if (chopLine(cloneString(setting), words) >= 2)
		alreadySet = differentString(words[1], "off");
	alreadySet = cartUsualBoolean(cart, option, alreadySet);
	cgiMakeCheckBox(option, alreadySet);
	printf ("%s", subtrack->longLabel);
	puts("</TD></TR>");
	}
    }
hPrintf("</TABLE>\n");

hPrintf("<H3>Select a merge operation:</H3>\n");
hPrintf("These combinations will maintain the gene/alignment structure "
	"(if any) of %s: <P>\n", curTable);

op = cartUsualString(cart, hgtaNextSubtrackMergeOp, "any");
makeOpButton("any", op);
printf("All %s records that have any overlap with any other selected subtrack<BR>\n",
       curTable);
makeOpButton("none", op);
printf("All %s records that have no overlap with any other selected subtrack<BR>\n",
       curTable);

makeOpButton("more", op);
printf("All %s records that have at least ",
       curTable);
setting = cartCgiUsualString(cart, hgtaNextSubtrackMergeMoreThreshold, "80");
cgiMakeTextVar(hgtaNextSubtrackMergeMoreThreshold, setting, 3);
printf(" %% overlap with any other selected subtrack<BR>\n");
makeOpButton("less", op);
printf("All %s records that have at most ",
       curTable);
setting = cartCgiUsualString(cart, hgtaNextSubtrackMergeLessThreshold, "80");
cgiMakeTextVar(hgtaNextSubtrackMergeLessThreshold, setting, 3);
printf(" %% overlap with any other selected subtrack<BR>\n");

makeOpButton("cat", op);
printf("All %s records, as well as all records from all other selected subtracks<P>\n",
       curTable);

printf("These combinations will discard the gene/alignment structure (if any) "
       "of %s and produce a simple list of position ranges.<P>\n",
       curTable);
makeOpButton("and", op);
printf("Base-pair-wise intersection (AND) of %s and other selected subtracks<BR>\n",
       curTable);
makeOpButton("or", op);
printf("Base-pair-wise union (OR) of %s and other selected subtracks<P>\n",
       curTable);
hPrintf("<P>\n");
cgiMakeButton(hgtaDoSubtrackMergeSubmit, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>\n");
htmlClose();
}

static void copyCartVars(struct cart *cart, char **source, char **dest,
			 int count)
/* Copy from source to dest (arrays of cart variable names). */
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

void doSubtrackMergePage(struct sqlConnection *conn)
/* Respond to subtrack merge create/edit button */
{
if (ArraySize(curVars) != ArraySize(nextVars))
    internalErr();
copyCartVars(cart, curVars, nextVars, ArraySize(curVars));
doSubtrackMergeMore(conn);
}

void doClearSubtrackMerge(struct sqlConnection *conn)
/* Respond to click on clear subtrack merge. */
{
cartRemovePrefix(cart, hgtaSubtrackMergePrefix);
cartRemovePrefix(cart, hgtaNextSubtrackMergePrefix);
doMainPage(conn);
}

void doSubtrackMergeSubmit(struct sqlConnection *conn)
/* Respond to submit on subtrack merge page. */
{
copyCartVars(cart, nextVars, curVars, ArraySize(curVars));
doMainPage(conn);
}

