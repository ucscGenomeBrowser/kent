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
#include "hui.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: compositeTrack.c,v 1.11 2008/08/30 16:28:49 tdreszer Exp $";

/* We keep two copies of variables, so that we can
 * cancel out of the page. */
static char *curVars[] = {hgtaSubtrackMergePrimary,
			  hgtaSubtrackMergeOp,
			  hgtaSubtrackMergeMoreThreshold,
			  hgtaSubtrackMergeLessThreshold,
			  hgtaSubtrackMergeWigOp,
			  hgtaSubtrackMergeRequireAll,
			  hgtaSubtrackMergeUseMinScore,
			  hgtaSubtrackMergeMinScore,
	};
static char *nextVars[] = {hgtaNextSubtrackMergePrimary,
			   hgtaNextSubtrackMergeOp,
			   hgtaNextSubtrackMergeMoreThreshold,
			   hgtaNextSubtrackMergeLessThreshold,
			   hgtaNextSubtrackMergeWigOp,
			   hgtaNextSubtrackMergeRequireAll,
			   hgtaNextSubtrackMergeUseMinScore,
			   hgtaNextSubtrackMergeMinScore,
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

static void makeWigOpButton(char *val, char *selVal)
/* Make merge-wiggle-op radio button. */
{
cgiMakeRadioButton(hgtaNextSubtrackMergeWigOp, val, sameString(val, selVal));
}

static void showWiggleMergeOptions()
/* Show subtrack merge operation options for wiggle/bedGraph tables. */
{
char *setting = cartUsualString(cart, hgtaNextSubtrackMergeWigOp, "average");
makeWigOpButton("average", setting);
printf("Average (at each position) of all selected subtracks' scores<BR>\n");
makeWigOpButton("sum", setting);
printf("Sum (at each position) of all selected subtracks' scores<BR>\n");
makeWigOpButton("product", setting);
printf("Product (at each position) of all selected subtracks' scores<BR>\n");
makeWigOpButton("min", setting);
printf("Minimum (at each position) of all selected subtracks' scores<BR>\n");
makeWigOpButton("max", setting);
printf("Maximum (at each position) of all selected subtracks' scores<P>\n");

cgiMakeCheckBox(hgtaNextSubtrackMergeRequireAll,
		cartUsualBoolean(cart, hgtaSubtrackMergeRequireAll, FALSE));
printf("Discard scores for positions at which one or more selected subtracks "
       "have no data.<BR>\n");
cgiMakeCheckBox(hgtaNextSubtrackMergeUseMinScore,
		cartUsualBoolean(cart, hgtaSubtrackMergeUseMinScore, FALSE));
printf("Discard scores less than \n");
setting = cartCgiUsualString(cart, hgtaNextSubtrackMergeMinScore, "0.0");
cgiMakeTextVar(hgtaNextSubtrackMergeMinScore, setting, 5);
printf(" after performing the above operation.<P>\n");
}

static void makeOpButton(char *val, char *selVal)
/* Make merge-bed-op radio button. */
{
cgiMakeRadioButton(hgtaNextSubtrackMergeOp, val, sameString(val, selVal));
}

static void showBedMergeOptions()
/* Show subtrack merge operation options for tables that are distilled 
 * to BED (not wiggle/bedGraph). */
{
char *op = cartUsualString(cart, hgtaNextSubtrackMergeOp, "any");
char *setting = NULL;
hPrintf("These combinations will maintain the gene/alignment structure "
	"(if any) of %s: <P>\n", curTable);

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
}

static struct trackDb *getPrimaryTdb()
/* Return a pointer to the subtrack tdb for the primary subtrack. */
{
struct trackDb *primary = NULL, *sTdb = NULL;
for (sTdb = curTrack->subtracks; sTdb != NULL; sTdb = sTdb->next)
    {
    if (sameString(curTable, sTdb->tableName))
	{
	primary = sTdb;
	break;
	}
    }
return primary;
}

void doSubtrackMergeMore(struct sqlConnection *conn)
/* Respond to subtrack merge create/edit button */
{
struct trackDb *primary = getPrimaryTdb();
char *dbTable = getDbTable(database, curTable);

htmlOpen("Merge subtracks of %s (%s)",
	 curTrack->tableName, curTrack->longLabel);

hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=%s>\n",
	cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
/* Currently selected subtrack table will be the primary subtrack in the 
 * merge. */
cgiMakeHiddenVar(hgtaNextSubtrackMergePrimary, dbTable);

hPrintf("<H3>Select a subset of subtracks to merge:</H3>\n");
hCompositeUi(cart, curTrack, curTable, hgtaDoSubtrackMergePage, "mainForm",database);

hPrintf("<H3>Select a merge operation:</H3>\n");
if (isWiggle(database, curTable) || isBedGraph(curTable))
    showWiggleMergeOptions(primary->longLabel);
else
    showBedMergeOptions();
hPrintf("If a filter is specified on the main Table Browser page, it will "
	"be applied only to %s, not to any other selected subtrack.  ",
	primary->longLabel);
hPrintf("If an intersection is specified on the main page, it will be applied "
	"to the result of this merge.<P>\n");
hPrintf("<P>\n");
cgiMakeButton(hgtaDoSubtrackMergeSubmit, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>\n");
htmlClose();
}

char *describeSubtrackMerge(char *linePrefix)
/* Return a multi-line string that describes the specified subtrack merge, 
 * with each line beginning with linePrefix. */
{
struct dyString *dy = dyStringNew(512);
struct trackDb *primary = getPrimaryTdb(), *tdb = NULL;
dyStringAppend(dy, linePrefix);
dyStringPrintf(dy, "Subtrack merge, primary table = %s (%s)\n",
	       curTable, primary->longLabel);
dyStringAppend(dy, linePrefix);
dyStringPrintf(dy, "Subtrack merge operation: ");
if (isWiggle(database, curTable) || isBedGraph(curTable))
    {
    char *op = cartString(cart, hgtaSubtrackMergeWigOp);
    dyStringPrintf(dy, "%s of %s and selected subtracks:\n", op, curTable);
    }
else
    {
    char *op = cartString(cart, hgtaSubtrackMergeOp);
    if (sameString(op, "any"))
	dyStringPrintf(dy, "All %s records that have any overlap with "
		       "any other selected subtrack:\n",
		       curTable);
    else if (sameString(op, "none"))
	dyStringPrintf(dy, "All %s records that have no overlap with "
		       "any other selected subtrack:\n",
		       curTable);
    else if (sameString(op, "more"))
	{
	dyStringPrintf(dy, "All %s records that have at least %s ",
		       curTable,
		       cartString(cart, hgtaNextSubtrackMergeMoreThreshold));
	dyStringPrintf(dy, " %% overlap with any other selected subtrack:\n");
	}
    else if (sameString(op, "less"))
	{
	dyStringPrintf(dy, "All %s records that have at most %s ",
		       curTable,
		       cartString(cart, hgtaNextSubtrackMergeLessThreshold));
	dyStringPrintf(dy, " %% overlap with any other selected subtrack:\n");

	}
    else if (sameString(op, "cat"))
	dyStringPrintf(dy, "All %s records, as well as all records from "
		       "all other selected subtracks:\n", curTable);
    else if (sameString(op, "and"))
	dyStringPrintf(dy, "Base-pair-wise intersection (AND) of %s and "
		       "other selected subtracks:\n", curTable);
    else if (sameString(op, "or"))
	dyStringPrintf(dy, "Base-pair-wise union (OR) of %s and other "
		       "selected subtracks:\n",
       curTable);
    else
	errAbort("describeSubtrackMerge: unrecognized op %s", op);
    }
for (tdb=curTrack->subtracks;  tdb != NULL;  tdb = tdb->next)
    {
    if (!sameString(tdb->tableName, curTable) &&
	isSubtrackMerged(tdb->tableName) &&
	sameString(tdb->type, primary->type))
	{
	dyStringAppend(dy, linePrefix);
	dyStringPrintf(dy, "  %s (%s)\n", tdb->tableName, tdb->longLabel);
	}
    }
return dyStringCannibalize(&dy);
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

