/* Stuff to display details on tracks that are clusters of peaks (items) in other tracks. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "obscure.h"
#include "hCommon.h"
#include "hdb.h"
#include "web.h"
#include "cart.h"
#include "trackDb.h"
#include "hgc.h"
#include "encode/encodePeak.h"


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

static struct slName *findMatchingSubtracks(struct slName *inTrackList, struct slPair *selGroupList)
/* Look in track and it's descendents for tracks with groups that match all values 
 * in selGroupList. */
{
struct slName *matchList = NULL;
struct slName *inTrack;
for (inTrack = inTrackList; inTrack != NULL; inTrack = inTrack->next)
    {
    struct trackDb *tdb = hashFindVal(trackHash, inTrack->name);
    if (tdb == NULL)
        errAbort("Can't find track %s which is in inputTracks", inTrack->name);
    rAddMatching(tdb,  selGroupList, &matchList);
    }
return matchList;
}

static void printTableInfo(struct trackDb *tdb, struct trackDb *clusterTdb,
    struct slName *displayGroupList)
/* Print out info on table. */
{
webPrintLinkCell(tdb->shortLabel);
webPrintLinkCell(tdb->longLabel);
}

static void showOnePeak(struct trackDb *tdb, struct bed *cluster, struct trackDb *clusterTdb,
	struct encodePeak *peak, struct slName *displayGroupList, int *pIx)
/* Show info on track and peak.  Peak may be NULL in which case n/a's will be printed
 * as appropriate. */
{
*pIx += 1;
webPrintIntCell(*pIx);
if (peak)
    {
    webPrintIntCell(peak->score);
    int overlap = positiveRangeIntersection(peak->chromStart, peak->chromEnd, 
    	cluster->chromStart, cluster->chromEnd);
    double overlapRatio = (double)overlap/(cluster->chromEnd - cluster->chromStart);
    webPrintLinkCellRightStart();
    printf("%4.1f%%", overlapRatio*100);
    webPrintLinkCellEnd();
    }
printTableInfo(tdb, clusterTdb, displayGroupList);
}

static boolean showMatchingTrack(char *track, struct bed *cluster, struct sqlConnection *conn,
	struct trackDb *clusterTdb, struct slName *displayGroupList,
	boolean showIfTrue, boolean showIfFalse, int *pRowIx)
/* put out a line in an html table that describes the given track. */ 
{
struct trackDb *tdb = hashMustFindVal(trackHash, track);
boolean result = FALSE;
char **row;
int rowOffset = 0;
struct sqlResult *sr = hRangeQuery(conn, tdb->table, 
	cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, &rowOffset);
boolean gotData = FALSE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    enum encodePeakType pt = encodePeakInferTypeFromTable(database, tdb->table, tdb->type);
    struct encodePeak *peak = encodePeakGeneralLoad(row + rowOffset, pt);
    if (showIfTrue)
	{
	showOnePeak(tdb, cluster, clusterTdb, peak, displayGroupList, pRowIx);
	result = TRUE;
	}
    gotData = TRUE;
    }
if (!gotData)
    {
    if (showIfFalse)
	{
	showOnePeak(tdb, cluster, clusterTdb, NULL, displayGroupList, pRowIx);
	result = TRUE;
	}
    }
sqlFreeResult(&sr);
return result;
}

void doPeakClusters(struct trackDb *tdb, char *item)
/* Display detailed info about a cluster of peaks from other tracks. */
{
int start = cartInt(cart, "o");
char *table = tdb->table;
int rowOffset = hOffsetPastBin(database, seqName, table);
char query[256];
struct sqlResult *sr;
char **row;
struct bed *cluster = NULL;
struct sqlConnection *conn = hAllocConn(database);

char title[256];
safef(title, sizeof(title), "%s item details", tdb->shortLabel);
cartWebStart(cart, database, title);
sprintf(query,
	"select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	table, item, seqName, start);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    cluster = bedLoadN(row + rowOffset, 5);
sqlFreeResult(&sr);

if (cluster != NULL)
    {
    /* Get list of tracks we'll look through for input. */
    char *inputTracks = trackDbRequiredSetting(tdb, "inputTracks");
    struct slName *inTrackList = stringToSlNames(inputTracks);

    /* Get list of subgroups to select on */
    char *inputTracksSubgroupSelect = trackDbRequiredSetting(tdb, "inputTracksSubgroupSelect");
    struct slPair *selGroupList = slPairFromString(inputTracksSubgroupSelect);

    /* Get list of subgroups to display */
    char *inputTracksSubgroupDisplay = trackDbRequiredSetting(tdb, "inputTracksSubgroupDisplay");
    struct slName *displayGroupList = stringToSlNames(inputTracksSubgroupDisplay);

    /* Get list of tracks that match and make a table out of them. */
    struct slName *matchTrackList = findMatchingSubtracks(inTrackList, selGroupList);
    struct slName *matchTrack;

    printf("<B>Items in Cluster:</B> %s of %d<BR>\n", cluster->name, slCount(matchTrackList));
    printf("<B>Maximum Item Score (out of 1000):</B> %d<BR>\n", cluster->score);
    printPos(cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, TRUE, NULL);


    webPrintLinkTableStart();
    int rowIx = 0;
    for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
        {
	if (showMatchingTrack(matchTrack->name, cluster, conn, tdb, displayGroupList,
		TRUE, FALSE, &rowIx))
	    printf("</TR><TR>\n");
	}
    webPrintLinkTableEnd();


    uglyf("<B>inputTracks:</B> %s<BR>\n", inputTracks);
    uglyf("<B>inputTracksSubgroupSelect:</B> %s<BR>\n", inputTracksSubgroupSelect);
    uglyf("<B>inputTracksSubgroupDisplay:</B> %s<BR>\n", inputTracksSubgroupDisplay);

    uglyf("<B>matchingTracks:</B> %d<BR>\n", slCount(matchTrackList));
    uglyf("<B>displayGroupList:</B> %d<BR>\n", slCount(displayGroupList));


    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

