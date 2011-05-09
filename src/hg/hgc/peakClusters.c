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
 
#define TO_BE_REMOVED  /* Some code that is brittle we are phasing out */
                       /* In particular this code demanded all tracks cluster was built on
		        * still exist, which isn't realistic given ENCODE data thrash */

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
#include "ra.h"
#include "txCluster.h"

#ifdef TO_BE_REMOVED
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
#endif /* TO_BE_REMOVED */

#ifdef TO_BE_REMOVED
char *mustFindGroupTagVal(struct trackDb *tdb, char *tag)
/* Find value of given tag inside of subgroups field or abort with error message. */ 
{
char *val = findGroupTagVal(tdb, tag);
if (val == NULL)
    errAbort("Couldn't find %s in subGroups tag of %s", tag, tdb->track);
return val;
}
#endif /* TO_BE_REMOVED */

#ifdef TO_BE_REMOVED
char *findGroupLabel(struct trackDb *tdb, char *group)
/* Given name of group, ruffle through all subGroupN tags, looking for one that
 * matches group */
{
char *groupId = mustFindGroupTagVal(tdb, group);
return compositeGroupLabel(tdb, group, groupId);
}
#endif /* TO_BE_REMOVED */

static void printClusterTableHeader(struct slName *otherCols, 
	boolean withAbbreviation, boolean withDescription, boolean withSignal)
/* Print out header fields table of tracks in cluster */
{
webPrintLabelCell("#");
if (withSignal)
    webPrintLabelCell("signal");
if (withAbbreviation)
    webPrintLabelCell("abr");
struct slName *col;
for (col = otherCols; col != NULL; col = col->next)
    webPrintLabelCell(col->name);
if (withDescription)
    webPrintLabelCell("description");
webPrintLabelCell("more info");
}

#ifdef TO_BE_REMOVED
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
webPrintLinkCellStart();
compositeMetadataToggle(database, tdb, "metadata", TRUE, FALSE, trackHash);
webPrintLinkCellEnd();
}
#endif /* TO_BE_REMOVED */

#ifdef TO_BE_REMOVED
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
#endif /* TO_BE_REMOVED */

#ifdef TO_BE_REMOVED
static void showMatchingTrack(char *track, struct bed *cluster, struct sqlConnection *conn,
	struct trackDb *clusterTdb, struct slName *displayGroupList, boolean invert, int *pRowIx)
/* put out a line in an html table that describes the given track. */ 
{
struct trackDb *tdb = hashFindVal(trackHash, track);
if (tdb != NULL)
    {
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
    }
}
#endif /* TO_BE_REMOVED */

static double getSignalAt(char *table, struct bed *cluster)
/* Get (average) signal from table entries that overlap cluster */
{
struct sqlConnection *conn = hAllocConn(database);
int count = 0;
double sum = 0;
if (sqlTableExists(conn, table))  // Table might be withdrawn from data thrash
    {
    int rowOffset;
    struct sqlResult *sr = hRangeQuery(conn, table, cluster->chrom, cluster->chromStart, 
	    cluster->chromEnd, NULL, &rowOffset);
    int signalCol = sqlFieldColumn(sr, "signalValue");
    if (signalCol < 0)
	internalErr();
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	count += 1;
	sum += sqlDouble(row[signalCol]);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }

if (count > 0)
    return sum/count;
else
    return 0;
}

static void printControlledVocabFields(char **row, int fieldCount, 
	struct slName *fieldList, char *vocabFile, struct hash *vocabHash)
/* Print out fields from row, linking them to controlled vocab if need be. */
{
int i;
struct slName *field;
for (i=0, field = fieldList; i<fieldCount; ++i, field = field->next)
    {
    char *fieldVal = row[i];
    if (vocabFile && hashLookup(vocabHash, field->name))
	{
	char *link = controlledVocabLink(vocabFile, "term", 
		fieldVal, fieldVal, fieldVal, "");
	webPrintLinkCell(link);
	}
    else
	webPrintLinkCell(fieldVal);
    }
}

struct hash *getVocabHash(char *fileName)
/* Get vocabulary term hash */
{
struct hash *hash = raTagVals(fileName, "type");
hashAdd(hash, "cellType", NULL);	/* Will the kludge never end, no, never! */
return hash;
}

static void printMetadataForTable(char *table)
/* If table exists, _and_ tdb associated with it exists, print out
 * a metadata link that expands on click.  Otherwise print "unavailable" */
{
webPrintLinkCellStart();
struct trackDb *tdb = hashFindVal(trackHash, table);
if (tdb == NULL)
    printf("%s info n/a", table);
else
    compositeMetadataToggle(database, tdb, "metadata", TRUE, FALSE, trackHash);
webPrintLinkCellEnd();
}

static void printPeakClusterTableHits(struct bed *cluster, struct sqlConnection *conn,
	char *inputTrackTable, struct slName *fieldList, char *vocab)
/* Put out a lines in an html table that shows assayed sources that have hits in this
 * cluster, or if invert is set, that have misses. */
{
char *vocabFile = NULL;
struct hash *vocabHash = NULL;
if (vocab)
    {
    vocabFile = cloneFirstWord(vocab);
    vocabHash = getVocabHash(vocabFile);
    }

/* Make the SQL query to get the table and all other fields we want to show
 * from inputTrackTable. */
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select tableName");
struct slName *field;
for (field = fieldList; field != NULL; field = field->next)
    dyStringPrintf(query, ",%s", field->name);
dyStringPrintf(query, " from %s", inputTrackTable);

int displayNo = 0;
int fieldCount = slCount(fieldList);
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *table = row[0];
    double signal = getSignalAt(table, cluster);
    if (signal != 0)
	{
	printf("</TR><TR>\n");
	webPrintIntCell(++displayNo);
	webPrintDoubleCell(signal);
	printControlledVocabFields(row+1, fieldCount, fieldList, vocabFile, vocabHash);
	printMetadataForTable(table);
	}
    }
sqlFreeResult(&sr);
freez(&vocabFile);
dyStringFree(&query);
}

static void printPeakClusterInputs(struct sqlConnection *conn,
	char *inputTrackTable, struct slName *fieldList, char *vocab)
/* Print out all input tables for clustering. */
{
char *vocabFile = NULL;
struct hash *vocabHash = NULL;
if (vocab)
    {
    vocabFile = cloneFirstWord(vocab);
    vocabHash = getVocabHash(vocabFile);
    }

/* Make the SQL query to get the table and all other fields we want to show
 * from inputTrackTable. */
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select tableName");
struct slName *field;
for (field = fieldList; field != NULL; field = field->next)
    dyStringPrintf(query, ",%s", field->name);
dyStringPrintf(query, " from %s", inputTrackTable);

int displayNo = 0;
int fieldCount = slCount(fieldList);
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("</TR><TR>\n");
    webPrintIntCell(++displayNo);
    printControlledVocabFields(row+1, fieldCount, fieldList, vocabFile, vocabHash);
    printMetadataForTable(row[0]);
    }


sqlFreeResult(&sr);
freez(&vocabFile);
dyStringFree(&query);
}

static void printFactorSourceTableHits(struct bed *cluster, struct sqlConnection *conn,
	char *sourceTable, char *inputTrackTable, 
	struct slName *fieldList, boolean invert, char *vocab)
/* Put out a lines in an html table that shows assayed sources that have hits in this
 * cluster, or if invert is set, that have misses. */
{
char *vocabFile = NULL;
if (vocab)
    {
    vocabFile = cloneFirstWord(vocab);
    }

/* Make the monster SQL query to get all assays*/
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select %s.id,%s.name,%s.tableName", sourceTable, sourceTable, 
	inputTrackTable);
struct slName *field;
for (field = fieldList; field != NULL; field = field->next)
    dyStringPrintf(query, ",%s.%s", inputTrackTable, field->name);
dyStringPrintf(query, " from %s,%s ", inputTrackTable, sourceTable);
dyStringPrintf(query, " where %s.source = %s.description", inputTrackTable, sourceTable);
dyStringPrintf(query, " and factor='%s' order by %s.source", cluster->name, inputTrackTable);

int displayNo = 0;
int fieldCount = slCount(fieldList);
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int sourceId = sqlUnsigned(row[0]);
    double signal = cluster->expScores[sourceId];
    boolean hit = (signal > 0);
    if (hit ^ invert)
        {
	printf("</TR><TR>\n");
	webPrintIntCell(++displayNo);
	if (!invert)
	    webPrintDoubleCell(signal);
	webPrintLinkCell(row[1]);
	int i;
	for (i=0; i<fieldCount; ++i)
	    {
	    char *fieldVal = row[i+3];
	    if (vocab)
	        {
		char *link = controlledVocabLink(vocabFile, "term", 
			fieldVal, fieldVal, fieldVal, "");
		webPrintLinkCell(link);
		}
	    else
		webPrintLinkCell(fieldVal);
	    }
	printMetadataForTable(row[2]);
	}
    }
sqlFreeResult(&sr);
freez(&vocabFile);
dyStringFree(&query);
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
cartWebStart(cart, database, "List of items assayed in %s", clusterTdb->shortLabel);
struct sqlConnection *conn = hAllocConn(database);

char *inputTracksSubgroupDisplay = trackDbSetting(clusterTdb, "inputTracksSubgroupDisplay");
char *inputTableFieldDisplay = trackDbSetting(clusterTdb, "inputTableFieldDisplay");
webPrintLinkTableStart();
if (inputTableFieldDisplay)
    {
    struct slName *fieldList = stringToSlNames(inputTableFieldDisplay);
    printClusterTableHeader(fieldList, FALSE, FALSE, FALSE);
    char *vocab = trackDbSetting(clusterTdb, "controlledVocabulary");
    char *inputTrackTable = trackDbRequiredSetting(clusterTdb, "inputTrackTable");
    printPeakClusterInputs(conn, inputTrackTable, fieldList, vocab);
    }
#ifdef TO_BE_REMOVED
else if (inputTracksSubgroupDisplay)
    {
    struct slName *matchTrackList = findMatchingSubtracks(clusterTdb);
    struct slName *matchTrack;

    struct slName *displayGroupList = stringToSlNames(inputTracksSubgroupDisplay);
    printClusterTableHeader(displayGroupList, FALSE, TRUE, FALSE);
    int rowIx = 0;
    for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
	{
	struct trackDb *tdb = hashFindVal(trackHash, matchTrack->name);
	showOnePeakOrMiss(tdb, clusterTdb, NULL, displayGroupList, &rowIx);
	}
    }
#endif /* TO_BE_REMOVED */
else
    errAbort("Missing required trackDb setting %s or %s for track %s",
	"inputTracksSubgroupDisplay", "inputTableFieldDisplay", clusterTdb->track);
webPrintLinkTableEnd();
hFreeConn(&conn);
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
    char *inputTracksSubgroupDisplay = trackDbSetting(tdb, "inputTracksSubgroupDisplay");
    char *inputTableFieldDisplay = trackDbSetting(tdb, "inputTableFieldDisplay");
    if (inputTableFieldDisplay != NULL)
        {
	struct slName *fieldList = stringToSlNames(inputTableFieldDisplay);
	char *vocab = trackDbSetting(tdb, "controlledVocabulary");
	char *inputTrackTable = trackDbRequiredSetting(tdb, "inputTrackTable");

	/* Print out some information about the cluster overall. */
	printf("<B>Items in Cluster:</B> %s of %d<BR>\n", cluster->name, 
	    sqlRowCount(conn, inputTrackTable));
	printf("<B>Cluster Score (out of 1000):</B> %d<BR>\n", cluster->score);
	printPos(cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, TRUE, NULL);

	/* In a new section put up list of hits. */
	webNewSection("List of Items in Cluster");
	webPrintLinkTableStart();
	printClusterTableHeader(fieldList, FALSE, FALSE, TRUE);
	printPeakClusterTableHits(cluster, conn, inputTrackTable, fieldList, vocab);
	}
#ifdef TO_BE_REMOVED
    else if (inputTracksSubgroupDisplay != NULL)
	{
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
	printClusterTableHeader(displayGroupList, FALSE, TRUE, TRUE);
	int rowIx = 0;
	for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
	    {
	    showMatchingTrack(matchTrack->name, cluster, conn, tdb, displayGroupList,
		    FALSE, &rowIx);
	    }
	}
#endif /* TO_BE_REMOVED */
    else
	errAbort("Missing required trackDb setting %s or %s for track %s",
	    "inputTracksSubgroupDisplay", "inputTableFieldDisplay", tdb->track);
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
    char *sourceTable = trackDbRequiredSetting(tdb, "sourceTable");
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
    
    char *factorLink = cluster->name;
    char *vocab = trackDbSetting(tdb, "controlledVocabulary");
    if (vocab != NULL)
	{
	char *file = cloneFirstWord(vocab);
	factorLink = controlledVocabLink(file, "term", factorLink, factorLink, factorLink, "");
	}
    printf("<B>Factor:</B> %s<BR>\n", factorLink);
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

            safef(query, sizeof(query), 
	    	"select max(score) from %s where name = '%s'", 
		"wgEncodeRegTfbsClusteredMotifs", item);
            sr = sqlGetResult(conn, query);
            if ((row = sqlNextRow(sr)) != NULL)
                {
                if(!isEmpty(row[0]))
                    {
                    maxScore = sqlFloat(row[0]);
                    }
                }
            sqlFreeResult(&sr);

            struct dnaSeq *seq = hDnaFromSeq(database, 
	    	seqName, hit->chromStart, hit->chromEnd, dnaLower);
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
    safef(query, sizeof(query), 
    	"select tableName from %s where factor='%s' order by source", inputTrackTable, 
	cluster->name);
    struct slName *matchTrackList = sqlQuickList(conn, query);
    struct slName *matchTrack;

    /* Next do the lists of hits and misses.  We have the hits from the non-zero signals in
     * cluster->expScores.  We need to figure out the sources actually assayed though
     * some other way.  We'll do this by one of two techniques.
     * If the inputTracksSubgroupDisplay is set, we'll try and figure out what was
     * assayed by looking at the subgroup stuff in trackDb, which works if everythings
     * part of a composite.  If not, we'll use the inputTrackTable. */
    /* Get list of subgroups to display */
    char *inputTracksSubgroupDisplay = trackDbSetting(tdb, "inputTracksSubgroupDisplay");
    char *inputTableFieldDisplay = trackDbSetting(tdb, "inputTableFieldDisplay");
    if (inputTableFieldDisplay != NULL)
        {
	struct slName *fieldList = stringToSlNames(inputTableFieldDisplay);
	char *vocab = trackDbSetting(tdb, "controlledVocabulary");

	/* In a new section put up list of hits. */
	webNewSection("List of %s Items in Cluster", cluster->name);
	webPrintLinkTableStart();
	printClusterTableHeader(fieldList, TRUE, FALSE, TRUE);
	printFactorSourceTableHits(cluster, conn, sourceTable, 
		inputTrackTable, fieldList, FALSE, vocab);
	webPrintLinkTableEnd();

	webNewSection("List of cells assayed with %s but without hits in cluster", cluster->name);
	webPrintLinkTableStart();
	printClusterTableHeader(fieldList, TRUE, FALSE, FALSE);
	printFactorSourceTableHits(cluster, conn, sourceTable, 
		inputTrackTable, fieldList, TRUE, vocab);
	webPrintLinkTableEnd();
	}
#ifdef TO_BE_REMOVED
    else if (inputTracksSubgroupDisplay != NULL)
	{
	struct slName *displayGroupList = stringToSlNames(inputTracksSubgroupDisplay);

	/* In a new section put up list of hits. */
	webNewSection("List of %s Items in Cluster", cluster->name);
	webPrintLinkTableStart();
	printClusterTableHeader(displayGroupList, FALSE, TRUE, TRUE);
	int rowIx = 0;
	for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
	    {
	    showMatchingTrack(matchTrack->name, cluster, conn, tdb, displayGroupList,
		    FALSE, &rowIx);
	    }
	webPrintLinkTableEnd();


	webNewSection("List of cells assayed with %s but without hits in cluster", cluster->name);
	webPrintLinkTableStart();
	printClusterTableHeader(displayGroupList, FALSE, TRUE, FALSE);
	rowIx = 0;
	for (matchTrack = matchTrackList; matchTrack != NULL; matchTrack = matchTrack->next)
	    {
	    showMatchingTrack(matchTrack->name, cluster, conn, tdb, displayGroupList,
		    TRUE, &rowIx);
	    }
	webPrintLinkTableEnd();
	}
#endif /* TO_BE_REMOVED */
    else
        {
	errAbort("Missing required trackDb setting %s or %s for track %s",
	    "inputTracksSubgroupDisplay", "inputTableFieldDisplay", tdb->track);
	}

    webNewSection("Table of abbreviations for cells");
    hPrintAbbreviationTable(conn, sourceTable, "Cell Type");

    webNewSection("Track Description");
    }
}

