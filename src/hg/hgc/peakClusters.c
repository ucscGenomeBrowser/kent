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
#include "ra.h"
#include "txCluster.h"


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
    compositeMetadataToggle(database, tdb, "metadata", TRUE, FALSE);
webPrintLinkCellEnd();
}

static void queryInputTrackTable(struct dyString *query, char *inputTrackTable,
                                struct slName *fieldList)
/* Construct query in dyString to return contents of inputTrackTable ordered appropriately */
{
struct dyString *fields = dyStringNew(0);
struct slName *field;
dyStringPrintf(query, "select tableName ");
for (field = fieldList; field != NULL; field = field->next)
    dyStringPrintf(fields, ",%s", field->name);
dyStringPrintf(query, "%s from %s", fields->string, inputTrackTable);
if (fieldList != NULL)
    // skip leading comma
    dyStringPrintf(query, " order by %s", fields->string+1);
dyStringFree(&fields);
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
queryInputTrackTable(query, inputTrackTable, fieldList);

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
queryInputTrackTable(query, inputTrackTable, fieldList);

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

void doPeakClusterListItemsAssayed()
/* Put up a page that shows all experiments associated with a cluster track. */
{
struct trackDb *clusterTdb = tdbForTableArg();
cartWebStart(cart, database, "List of items assayed in %s", clusterTdb->shortLabel);
struct sqlConnection *conn = hAllocConn(database);

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
else
    errAbort("Missing required trackDb setting %s for track %s", "inputTableFieldDisplay", 
                clusterTdb->track);
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
    else
	errAbort("Missing required trackDb setting %s for track %s",
	    "inputTableFieldDisplay", tdb->track);
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

struct factorSourceInfo 
/* Cell type and description */
    {
    struct factorSourceInfo *next;
    char *name;
    char *description;
    };

int factorSourceInfoCmp(const void *va, const void *vb)
/* Compare two factorSourceInfo's, sorting on name and then description fields */
{
static char bufA[64], bufB[64];
const struct factorSourceInfo *a = *((struct factorSourceInfo **)va);
const struct factorSourceInfo *b = *((struct factorSourceInfo **)vb);
safef(bufA, 64, "%s+%s", a->name, a->description);
safef(bufB, 64, "%s+%s", b->name, b->description);
return strcmp(bufA, bufB);
}

void factorSourceAbbreviationTable(struct sqlConnection *conn, char *sourceTable, boolean cellsOnly)
/* Print out table of abbreviations. Optionally, extract cell name only (before '+') and uniqify */
{
char *label = "Cell Type";
if (!cellsOnly)
    {
    hPrintAbbreviationTable(conn, sourceTable, label);
    return;
    }
char query[256];
safef(query, sizeof(query), "select name,description from %s order by name", sourceTable);
struct sqlResult *sr = sqlGetResult(conn, query);
webPrintLinkTableStart();
webPrintLabelCell("Symbol");
webPrintLabelCell(label);
char **row;
char *plus;
struct factorSourceInfo *source = NULL, *sources = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    char *description = row[1];
    if (cellsOnly)
        {
        // truncate description to just the cell type
        if ((plus = strchr(description, '+')) != NULL)
            *plus = 0;
        }
    AllocVar(source);
    source->name = cloneString(name);
    source->description = cloneString(description);
    slAddHead(&sources, source);
    }
slUniqify(&sources, factorSourceInfoCmp, NULL);
int count = 0;
/*
while ((source = slPopHead(&sources)) != NULL)
    {
    printf("</TR><TR>\n");
    webPrintLinkCell(source->name);
    webPrintLinkCell(source->description);
    count++;
    }
*/
while ((source = slPopHead(&sources)) != NULL)
    {
    printf("</TR><TR>\n");
    webPrintLinkCell(source->name);
    webPrintLinkCellStart();
    puts(source->description);
    count++;
    while (sources && sameString(sources->name, source->name))
        {
        source = slPopHead(&sources);
        puts(", ");
        puts(source->description);
        count++;
        }
    webPrintLinkCellEnd();
    }
sqlFreeResult(&sr);
webPrintLinkTableEnd();
printf("Total: %d\n", count);
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

    /* Next do the lists of hits and misses.  We have the hits from the non-zero signals in
     * cluster->expScores.  We need to figure out the sources actually assayed though
     * some other way.  We'll do this by one of two techniques. */
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

	webNewSection("List of cells assayed for %s but without hits in cluster", cluster->name);
	webPrintLinkTableStart();
	printClusterTableHeader(fieldList, TRUE, FALSE, FALSE);
	printFactorSourceTableHits(cluster, conn, sourceTable, 
		inputTrackTable, fieldList, TRUE, vocab);
	webPrintLinkTableEnd();
	}
    else
        {
	errAbort("Missing required trackDb setting %s for track %s",
	    "inputTableFieldDisplay", tdb->track);
	}

    webNewSection("Table of abbreviations for cells");
    boolean cellsOnly = (trackDbSetting(tdb, "sourceAbbrevCellsOnly") != NULL);
    factorSourceAbbreviationTable(conn, sourceTable, cellsOnly);
    webNewSection("Track Description");
    }
}

