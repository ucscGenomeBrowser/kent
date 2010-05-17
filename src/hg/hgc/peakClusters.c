/* Stuff to display details on tracks that are clusters of peaks (items) in other tracks. 
 * In particular peaks in either ENCODE narrowPeak or broadPeak settings, and residing in
 * composite tracks. 
 *
 * These come in two main forms currently:
 *     DNAse hypersensitive clusters - peaks clustered across cell lines stored in bed 5
 *                                     with no special type
 *     Transcription Factor Binding Sites (TFBS) - peaks from transcription factor ChIP-seq
 *                   across a number of transcription factors and cell lines. Stored in bed 15
 *                   plus sourceTable with type factorSource */
 
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "obscure.h"
#include "hCommon.h"
#include "hdb.h"
#include "web.h"
#include "cart.h"
#include "trackDb.h"
#include "hui.h"
#include "hgc.h"
#include "encode/encodePeak.h"
#include "expRecord.h"


char *findGroupTagVal(struct trackDb *tdb, char *tag)
/* Find value of given tag inside of subgroups field. */ 
{
char *subGroups = trackDbSetting(tdb, "subGroups");
struct slPair *el, *list = slPairFromString(subGroups);
char *val = NULL;
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(el->name, tag))
	{
        val = el->val;
	break;
	}
    }
return val;
}

char *mustFindGroupTagVal(struct trackDb *tdb, char *tag)
/* Find value of given tag inside of subgroups field or abort with error message. */ 
{
char *val = findGroupTagVal(tdb, tag);
if (val == NULL)
    errAbort("Couldn't find %s in subGroups tag of %s", tag, tdb->track);
return val;
}

char *findGroupLabel(struct trackDb *tdb, char *group)
/* Given name of group, ruffle through all subGroupN tags, looking for one that
 * matches group */
{
char *groupId = mustFindGroupTagVal(tdb, group);
return compositeGroupLabel(tdb, group, groupId);
}

static void printClusterTableHeader(struct slName *displayGroupList)
/* Print out header fields table */
{
webPrintLabelCell("#");
webPrintLabelCell("signal");
struct slName *displayGroup;
for (displayGroup = displayGroupList; displayGroup != NULL; displayGroup = displayGroup->next)
    {
    webPrintLabelCell(displayGroup->name);
    }
webPrintLabelCell("description");
printf("</TR><TR>\n");
}

static void printTableInfo(struct trackDb *tdb, struct trackDb *clusterTdb,
    struct slName *displayGroupList)
/* Print out info on table. */
{
struct slName *displayGroup;
for (displayGroup = displayGroupList; displayGroup != NULL; displayGroup = displayGroup->next)
    {
    char *label = findGroupLabel(tdb, displayGroup->name);
    char *linkedLabel = compositeLabelWithVocabLink(database, tdb, tdb, displayGroup->name, label);
    webPrintLinkCell(linkedLabel);
    }
webPrintLinkCell(tdb->longLabel);
}

static void showOnePeak(struct trackDb *tdb, struct bed *cluster, struct trackDb *clusterTdb,
	struct encodePeak *peakList, struct slName *displayGroupList, int *pIx)
/* Show info on track and peak.  Peak may be NULL in which case n/a's will be printed
 * as appropriate. */
{
struct encodePeak *peak;
*pIx += 1;
webPrintIntCell(*pIx);
webPrintLinkCellRightStart();
printf("%g", peakList->signalValue);
for (peak = peakList->next; peak != NULL; peak = peak->next)
    printf(",%g", peak->signalValue);
webPrintLinkCellEnd();
printTableInfo(tdb, clusterTdb, displayGroupList);
printf("</TR><TR>\n");
}

static boolean showMatchingTrack(char *track, struct bed *cluster, struct sqlConnection *conn,
	struct trackDb *clusterTdb, struct slName *displayGroupList, int *pRowIx)
/* put out a line in an html table that describes the given track. */ 
{
struct trackDb *tdb = hashMustFindVal(trackHash, track);
boolean result = FALSE;
char **row;
int rowOffset = 0;
struct sqlResult *sr = hRangeQuery(conn, tdb->table, 
	cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, &rowOffset);
struct encodePeak *peakList = NULL;
struct slDouble *slDoubleNew(double x);
while ((row = sqlNextRow(sr)) != NULL)
    {
    enum encodePeakType pt = encodePeakInferTypeFromTable(database, tdb->table, tdb->type);
    struct encodePeak *peak = encodePeakGeneralLoad(row + rowOffset, pt);
    slAddTail(&peakList, peak);
    }
if (peakList)
    showOnePeak(tdb, cluster, clusterTdb, peakList, displayGroupList, pRowIx);
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
safef(query, sizeof(query),
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

    /* Get list of tracks that match criteria. */
    struct slName *matchTrackList = encodeFindMatchingSubtracks(inTrackList, selGroupList);
    struct slName *matchTrack;

    /* Print out some information about the cluster overall. */
    printf("<B>Items in Cluster:</B> %s of %d<BR>\n", cluster->name, slCount(matchTrackList));
    printf("<B>Cluster Score (out of 1000):</B> %d<BR>\n", cluster->score);
    printPos(cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, TRUE, NULL);

    /* In a new section put up list of hits. */
    webNewSection("List of Items in Cluster");
    webPrintLinkTableStart();
    printClusterTableHeader(displayGroupList);
    int rowIx = 0;
    for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
        {
	showMatchingTrack(matchTrack->name, cluster, conn, tdb, displayGroupList,
		&rowIx);
	}
    webPrintLinkTableEnd();
    }
webNewSection("Track Description");
printTrackHtml(tdb);
hFreeConn(&conn);
}

char *findFactorId(struct slName *trackList, char *label)
/* Given factor label, find factor id. */
{
struct slName *track;
for (track = trackList; track != NULL; track = track->next)
    {
    struct trackDb *tdb = hashMustFindVal(trackHash, track->name);
    char *factorId = compositeGroupId(tdb, "factor", label);
    if (factorId != NULL)
        return factorId;
    }
errAbort("Couldn't find factor labeled %s", label);
return NULL;
}

void doFactorSource(struct sqlConnection *conn, struct trackDb *tdb, char *item, int start)
/* Display detailed info about a cluster of peaks from other tracks. */
{
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
char **row;
struct sqlResult *sr;
char query[256];
safef(query, sizeof(query),
	"select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	tdb->table, item, seqName, start);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
struct bed *cluster = NULL;
if (row != NULL)
    cluster = bedLoadN(row + rowOffset, 15);
sqlFreeResult(&sr);


if (cluster != NULL)
    {
    printf("<B>Factor:</B> %s<BR>\n", cluster->name);
    printf("<B>Cluster Score (out of 1000):</B> %d<BR>\n", cluster->score);
    printPos(cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, TRUE, NULL);

    char *sourceTable = trackDbRequiredSetting(tdb, "sourceTable");
    uglyf("<B>sourceTable:</B> %s<BR>\n", sourceTable);

    /* Get list of tracks we'll look through for input. */
    char *inputTracks = trackDbRequiredSetting(tdb, "inputTracks");
    struct slName *inTrackList = stringToSlNames(inputTracks);
    uglyf("<B>inputTracks:</B> %s (%d)<BR>\n", inputTracks, slCount(inTrackList));

    /* Get list of subgroups to select on */
    char *inputTracksSubgroupSelect = trackDbRequiredSetting(tdb, "inputTracksSubgroupSelect");
    struct slPair *selGroupList = slPairFromString(inputTracksSubgroupSelect);
    uglyf("<B>inputTracksSubgroupSelect:</B> %s (%d)<BR>\n", inputTracksSubgroupSelect, slCount(selGroupList));

    /* Figure out factor ID and add it as selection criteria*/
    char *factorId = findFactorId(inTrackList, cluster->name);
    uglyf("<B>cluster->id:</B> %s  <B>factorId:</B> %s<BR>\n", cluster->name, factorId);
    struct slPair *factorSel = slPairNew("factor", cloneString(factorId));
    slAddHead(&selGroupList, factorSel);

    /* Get list of tracks that match criteria. */
    struct slName *matchTrackList = encodeFindMatchingSubtracks(inTrackList, selGroupList);
    // struct slName *matchTrack;
    uglyf("<B>matchTrackList:</B> %d elements<BR>\n", slCount(matchTrackList));

    }
}

