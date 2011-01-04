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
#include "bed6FloatScore.h"
#include "txCluster.h"

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
/* Print out header fields table of tracks in cluster */
{
webPrintLabelCell("#");
webPrintLabelCell("signal");
struct slName *displayGroup;
for (displayGroup = displayGroupList; displayGroup != NULL; displayGroup = displayGroup->next)
    {
    webPrintLabelCell(displayGroup->name);
    }
webPrintLabelCell("description");
}

static void printOutOfClusterTableHeader(struct slName *displayGroupList)
/* Print out header fields for table of tracks not in cluster */
{
webPrintLabelCell("#");
struct slName *displayGroup;
for (displayGroup = displayGroupList; displayGroup != NULL; displayGroup = displayGroup->next)
    {
    webPrintLabelCell(displayGroup->name);
    }
webPrintLabelCell("description");
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
webPrintLinkCellStart();
printf("%s ", tdb->longLabel);
compositeMetadataToggle(database, tdb, "...", TRUE, FALSE, trackHash);
webPrintLinkCellEnd();
}

static void showOnePeakOrMiss(struct trackDb *tdb, struct trackDb *clusterTdb,
	struct encodePeak *peakList, struct slName *displayGroupList, int *pIx)
/* Show info on track and peak.  Peak may be NULL in which case fewer columns will be printed. */
{
struct encodePeak *peak;
*pIx += 1;
printf("</TR><TR>\n");
webPrintIntCell(*pIx);
if (peakList)
    {
    webPrintLinkCellRightStart();
    printf("%g", peakList->signalValue);
    for (peak = peakList->next; peak != NULL; peak = peak->next)
	printf(",%g", peak->signalValue);
    webPrintLinkCellEnd();
    }
printTableInfo(tdb, clusterTdb, displayGroupList);
}

static boolean showMatchingTrack(char *track, struct bed *cluster, struct sqlConnection *conn,
	struct trackDb *clusterTdb, struct slName *displayGroupList, boolean invert, int *pRowIx)
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
if (invert)
    {
    if (!peakList)
        showOnePeakOrMiss(tdb, clusterTdb, NULL, displayGroupList, pRowIx);
    }
else
    {
    if (peakList)
	showOnePeakOrMiss(tdb, clusterTdb, peakList, displayGroupList, pRowIx);
    }
sqlFreeResult(&sr);
return result;
}

static struct slName *findMatchingSubtracks(struct trackDb *tdb)
/* Find subtracks that match inputTracks tags. */
{
/* Just list look up tableName in inputTrackTable and return the list. */
char *inputTrackTable = trackDbRequiredSetting(tdb, "inputTrackTable");
struct sqlConnection *conn = hAllocConn(database);
char query[256];
safef(query, sizeof(query), "select tableName from %s order by source", inputTrackTable);
struct slName *matchTrackList = sqlQuickList(conn, query);
hFreeConn(&conn);
return matchTrackList;
}


void doPeakClusterListItemsAssayed()
/* Put up a page that shows all experiments associated with a cluster track. */
{
struct trackDb *clusterTdb = tdbForTableArg();
struct slName *matchTrackList = findMatchingSubtracks(clusterTdb);
struct slName *matchTrack;

cartWebStart(cart, database, "List of items assayed in %s", clusterTdb->shortLabel);

char *inputTracksSubgroupDisplay = trackDbRequiredSetting(clusterTdb, "inputTracksSubgroupDisplay");
struct slName *displayGroupList = stringToSlNames(inputTracksSubgroupDisplay);
webPrintLinkTableStart();
printOutOfClusterTableHeader(displayGroupList);
int rowIx = 0;
for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
    {
    struct trackDb *tdb = hashFindVal(trackHash, matchTrack->name);
    showOnePeakOrMiss(tdb, clusterTdb, NULL, displayGroupList, &rowIx);
    }
webPrintLinkTableEnd();
cartWebEnd();
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

cartWebStart(cart, database, "%s item details", tdb->shortLabel);
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
    /* Get list of subgroups to display */
    char *inputTracksSubgroupDisplay = trackDbRequiredSetting(tdb, "inputTracksSubgroupDisplay");
    struct slName *displayGroupList = stringToSlNames(inputTracksSubgroupDisplay);

    /* Get list of tracks that match criteria. */
    struct slName *matchTrackList = findMatchingSubtracks(tdb);
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
		FALSE, &rowIx);
	}
    webPrintLinkTableEnd();
    }
printf("<A HREF=\"%s&g=htcListItemsAssayed&table=%s\" TARGET_blank>", hgcPathAndSettings(),
	tdb->track);
printf("List all items assayed");
printf("</A><BR>\n");
webNewSection("Track Description");
printTrackHtml(tdb);
cartWebEnd();
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
char *motifTable = NULL;
#ifdef TXCLUSTER_MOTIFS_TABLE
motifTable = TXCLUSTER_MOTIFS_TABLE;
#endif

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
    struct dnaMotif *motif = NULL;
    struct dnaSeq **seqs = NULL;
    struct bed6FloatScore *hits = NULL;

    if(motifTable != NULL && sqlTableExists(conn, motifTable))
        {
        struct sqlResult *sr;
        int rowOffset;
        char where[256];

        motif = loadDnaMotif(item, "transRegCodeMotif");
        safef(where, sizeof(where), "name = '%s'", item);
        sr = hRangeQuery(conn, "wgEncodeRegTfbsClusteredMotifs", cluster->chrom, cluster->chromStart,
                         cluster->chromEnd, where, &rowOffset);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            struct bed6FloatScore *hit = NULL;
            AllocVar(hit);
            hit->chromStart = sqlUnsigned(row[rowOffset + 1]);
            hit->chromEnd = sqlUnsigned(row[rowOffset + 2]);
            hit->score = sqlFloat(row[rowOffset + 4]);
            hit->strand[0] = row[rowOffset + 5][0];
            slAddHead(&hits, hit);
            }
        sqlFreeResult(&sr);
        }
    
    printf("<B>Factor:</B> %s<BR>\n", cluster->name);
    printf("<B>Cluster Score (out of 1000):</B> %d<BR>\n", cluster->score);
    if(motif != NULL && hits != NULL)
        {
        struct bed6FloatScore *hit = NULL;
        int i;
        seqs = needMem(sizeof(struct dnaSeq *) * slCount(hits));
        for (hit = hits, i = 0; hit != NULL; hit = hit->next, i++)
            {
            char query[256];
            float maxScore = -1;

            safef(query, sizeof(query), "select max(score) from %s where name = '%s'", "wgEncodeRegTfbsClusteredMotifs", item);
            sr = sqlGetResult(conn, query);
            if ((row = sqlNextRow(sr)) != NULL)
                {
                if(!isEmpty(row[0]))
                    {
                    maxScore = sqlFloat(row[0]);
                    }
                }
            sqlFreeResult(&sr);

            struct dnaSeq *seq = hDnaFromSeq(database, seqName, hit->chromStart, hit->chromEnd, dnaLower);
            if(hit->strand[0] == '-')
                reverseComplement(seq->dna, seq->size);
            seqs[i] = seq;
            printf("<B>Motif Score #%d:</B>  %.2f (max: %.2f)<BR>\n", i + 1, hit->score, maxScore);
            }
        }
    printPos(cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, TRUE, NULL);

    if(seqs != NULL)
        {
        motifMultipleHitsSection(seqs, slCount(hits), motif);
        }

    /* Get list of tracks we'll look through for input. */
    char *inputTrackTable = trackDbRequiredSetting(tdb, "inputTrackTable");
    safef(query, sizeof(query), "select tableName from %s where factor='%s' order by source", inputTrackTable, cluster->name);
    struct slName *matchTrackList = sqlQuickList(conn, query);
    struct slName *matchTrack;

    /* Get list of subgroups to display */
    char *inputTracksSubgroupDisplay = trackDbRequiredSetting(tdb, "inputTracksSubgroupDisplay");
    struct slName *displayGroupList = stringToSlNames(inputTracksSubgroupDisplay);

    /* In a new section put up list of hits. */
    webNewSection("List of %s Items in Cluster", cluster->name);
    webPrintLinkTableStart();
    printClusterTableHeader(displayGroupList);
    int rowIx = 0;
    for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
        {
	showMatchingTrack(matchTrack->name, cluster, conn, tdb, displayGroupList,
		FALSE, &rowIx);
	}
    webPrintLinkTableEnd();


    webNewSection("List of cells assayed with %s but without hits in cluster", cluster->name);
    webPrintLinkTableStart();
    printOutOfClusterTableHeader(displayGroupList);
    rowIx = 0;
    for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
        {
	showMatchingTrack(matchTrack->name, cluster, conn, tdb, displayGroupList,
		TRUE, &rowIx);
	}
    webPrintLinkTableEnd();

    webNewSection("Table of abbreviations for cells");
    char *sourceTable = trackDbRequiredSetting(tdb, "sourceTable");
    hPrintAbbreviationTable(conn, sourceTable, "Cell Type");

    webNewSection("Track Description");
    }
}

