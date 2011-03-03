/* Handle details page for ENCODE tracks. */

#include "common.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "customTrack.h"
#include "web.h"
#include "encode/encodePeak.h"
#include "peptideMapping.h"

#ifdef UNUSED
static boolean pairInList(struct slPair *pair, struct slPair *list)
/* Return TRUE if pair is in list. */
{
struct slPair *el;
for (el = list; el != NULL; el = el->next)
    if (sameString(pair->name, el->name) && sameString(pair->val, el->val))
        return TRUE;
return FALSE;
}

static boolean selGroupListMatch(struct trackDb *tdb, struct slPair *selGroupList)
/* Return TRUE if tdb has match to every item in selGroupList */
{
char *subGroups = trackDbSetting(tdb, "subGroups");
if (subGroups == NULL)
    return FALSE;
struct slPair *groupList = slPairFromString(subGroups);
struct slPair *selGroup;
for (selGroup = selGroupList; selGroup != NULL; selGroup = selGroup->next)
    {
    if (!pairInList(selGroup, groupList))
        return FALSE;
    }
return TRUE;
}

static void rAddMatching(struct trackDb *tdb, struct slPair *selGroupList, struct slName **pList)
/* Add track and any descendents that match selGroupList to pList */
{
if (selGroupListMatch(tdb, selGroupList))
    slNameAddHead(pList, tdb->track);
struct trackDb *sub;
for (sub = tdb->subtracks; sub != NULL; sub = sub->next)
    rAddMatching(sub, selGroupList, pList);
}
#endif//def UNUSED

void doEncodePeak(struct trackDb *tdb, struct customTrack *ct)
/*  details for encodePeak type tracks. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
enum encodePeakType peakType;
char **row;
char *db;
char *table = tdb->table;
char *chrom = cartString(cart,"c");
int start = cgiInt("o");
int end = cgiInt("t");
int rowOffset;
/* connect to DB */
if (ct)
    {
    db = CUSTOM_TRASH;
    table = ct->dbTableName;
    }
else
    db = database;
conn = hAllocConn(db);
peakType = encodePeakInferTypeFromTable(db, table, tdb->type);
if (peakType == 0)
    errAbort("unrecognized peak type from table %s", tdb->table);
genericHeader(tdb, NULL);
sr = hOrderedRangeQuery(conn, table, chrom, start, end,
			NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    char **rowPastOffset = row + rowOffset;
    if ((sqlUnsigned(rowPastOffset[1]) != start) ||  (sqlUnsigned(rowPastOffset[2]) != end))
	continue;

    float signal = -1;
    float pValue = -1;
    float qValue = -1;
    /* Name */
    if (rowPastOffset[3][0] != '.')
	printf("<B>Name:</B> %s<BR>\n", rowPastOffset[3]);
    /* Position */
    printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       hgTracksPathAndSettings(), database, chrom, start+1, end, chrom, start+1, end);
    /* Print peak base */
    if ((peakType == narrowPeak) || (peakType == encodePeak))
	{
	int peak = sqlSigned(rowPastOffset[9]);
	if (peak > -1)
	    printf("<B>Peak point:</B> %d<BR>\n", start + peak + 1); // one based
	}
    /* Strand, score */
    if (rowPastOffset[5][0] != '.')
	    printf("<B>Strand:</B> %c<BR>\n", rowPastOffset[5][0]);
    printf("<B>Score:</B> %d<BR>\n", sqlUnsigned(rowPastOffset[4]));
    /* signalVal, pVal */
    if (peakType != gappedPeak)
	{
	signal = sqlFloat(rowPastOffset[6]);
	pValue = sqlFloat(rowPastOffset[7]);
	qValue = sqlFloat(rowPastOffset[8]);
	}
    else
	{
	signal = sqlFloat(rowPastOffset[12]);
	pValue = sqlFloat(rowPastOffset[13]);
	qValue = sqlFloat(rowPastOffset[14]);
	}
    if (signal >= 0)
	printf("<B>Signal value:</B> %.3f<BR>\n", signal);
    if (pValue >= 0)
	printf("<B>P-value (-log10):</B> %.3f<BR>\n", pValue);
    if (qValue >= 0)
	printf("<B>Q-value (FDR): </B> %.3f<BR>\n", qValue);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

int encodeFiveCInterCmp(const void *va, const void *vb)
/* reverse sort on bed nine's reserved field which in this */
/* case is the where the strength of the interaction is stored */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
return b->itemRgb - a->itemRgb;
}

void doEncodeFiveC(struct sqlConnection *conn, struct trackDb *tdb)
/* Print details for 5C track */
{
char *interTable = trackDbRequiredSetting(tdb, "interTable");
char *interTableKind = trackDbRequiredSetting(tdb, "interTableKind");
char **row;
char *chrom = cartString(cart,"c");
int start = cgiInt("o");
int end = cgiInt("t");
int rowOffset;
int outCount = 0;
struct sqlResult *sr;
struct bed *interList = NULL, *inter;
genericHeader(tdb, NULL);
sr = hOrderedRangeQuery(conn, interTable, chrom, start, end, NULL, &rowOffset);
printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       hgTracksPathAndSettings(), database, chrom, start+1, end, chrom, start+1, end);
while ((row = sqlNextRow(sr)) != NULL)
    {
    inter = bedLoadN(row + rowOffset, 9);
    slAddHead(&interList, inter);
    }
slSort(&interList, encodeFiveCInterCmp);
webNewSection("Top %s interations", interTableKind);
webPrintLinkTableStart();
webPrintLabelCell("Position");
webPrintLabelCell("5C signal");
webPrintLabelCell("Distance");
webPrintLinkTableNewRow();
for (inter = interList; inter != NULL; inter = inter->next)
    {
    char s[1024];
    int distance = 0;
    safef(s, sizeof(s), "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A>",
       hgTracksPathAndSettings(), database, chrom, inter->thickStart+1, inter->thickEnd, chrom, inter->thickStart+1, inter->thickEnd);
    webPrintLinkCell(s);
    safef(s, sizeof(s), "%d", inter->itemRgb);
    webPrintLinkCell(s);
    if (start > inter->thickStart)
	distance = inter->thickEnd - start;
    else
	distance = inter->thickStart - end;
    safef(s, sizeof(s), "%d", distance);
    webPrintLinkCell(s);
    if (++outCount == 50)
	break;
    if (inter->next != NULL)
	webPrintLinkTableNewRow();
    }
webPrintLinkTableEnd();
sqlFreeResult(&sr);
}

void doPeptideMapping(struct sqlConnection *conn, struct trackDb *tdb, char *item)
/* Print details for a peptideMapping track.  */
{
char *chrom = cartString(cart,"c");
int start = cgiInt("o");
int end = cgiInt("t");
char **row;
struct sqlResult *sr;
struct peptideMapping *pos = NULL;
int rowOffset;
genericHeader(tdb, NULL);
/* Just get the current item. */
sr = hOrderedRangeQuery(conn, tdb->track, chrom, start, end, NULL, &rowOffset);
if ((row = sqlNextRow(sr)) != NULL)
    {
    pos = peptideMappingLoad(row + rowOffset);
    sqlFreeResult(&sr);
    }
else
    {
    errAbort("No items in range");
    }
printf("<B>Item:</B> %s<BR>\n", pos->name);
printf("<B>Score:</B> %d<BR>\n", pos->score);
printPos(pos->chrom, pos->chromStart, pos->chromEnd, pos->strand, TRUE, item);
printf("<B>Raw Score:</B> %f<BR>\n", pos->rawScore);
printf("<B>Peptide Rank:</B> %d<BR>\n", pos->peptideRank);
printf("<B>Peptide Repeat Count:</B> %d<BR>\n", pos->peptideRepeatCount);
printf("<B>Spectrum ID:</B> %s<BR>\n", pos->spectrumId);
if (pos->peptideRepeatCount > 1)
    {
    char query[256];
    struct peptideMapping anotherPos;
    safef(query, sizeof(query), "select * from %s where name=\'%s\' and not (chrom=\'%s\' and chromStart=%d and chromEnd=%d)", 
	  tdb->track, pos->name, pos->chrom, pos->chromStart, pos->chromEnd);
    printf("<BR>\n");
    webPrintLinkTableStart();
    webPrintLabelCell("Other genomic loci");
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char s[1024];
	peptideMappingStaticLoad(row + rowOffset, &anotherPos);
	safef(s, sizeof(s), "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A>",
	      hgTracksPathAndSettings(), database, anotherPos.chrom, anotherPos.chromStart+1, 
	      anotherPos.chromEnd, anotherPos.chrom, anotherPos.chromStart+1, anotherPos.chromEnd);
	webPrintLinkTableNewRow();
	webPrintLinkCell(s);
	}
    webPrintLinkTableEnd();
    sqlFreeResult(&sr);
    }    
peptideMappingFree(&pos);
}
