/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* Stuff to display details on tracks that are clusters of peaks (items) in other tracks. 
 * In particular peaks in either ENCODE narrowPeak or broadPeak settings, and residing in
 * composite tracks. 
 *
 * These come in two main forms currently:
 *     DNAse hypersensitive clusters - peaks clustered across cell lines stored in bed 5
 *                                     with no special type
 *     Transcription Factor Binding Sites (TFBS) - peaks from transcription factor ChIP-seq
 *                   across a number of transcription factors and cell lines. Stored in bed 15
 *                   (or BED5+ factorSource format)
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
#include "jsHelper.h"
#include "factorSource.h"
#include "cv.h"

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
    }
hFreeConn(&conn);
if (count > 0)
    return sum/count;
else
    return 0;
}

static void printControlledVocabFields(char **row, int fieldCount, 
	struct slName *fieldList, char *vocabFile, struct hash *vocabHash)
/* Print out fields from row, linking them to controlled vocab if need be. 
 * If vocabFile is NULL, the vocabHash is not ENCODE, so links + mouseovers are
 * generated differently */
{
int i;
struct slName *field;

for (i=0, field = fieldList; i<fieldCount; ++i, field = field->next)
    {
    char *link = NULL;
    char *fieldVal = row[i];
    if (vocabFile && hashLookup(vocabHash, field->name))
        {
        // controlled vocabulary
        link = wgEncodeVocabLink("term", fieldVal, fieldVal, fieldVal, "");
        }
    else if (!vocabFile && vocabHash != NULL)
        {
        // meta tables
        struct hash *fieldHash = hashFindVal(vocabHash, field->name);
        if (fieldHash != NULL)
            link = vocabLink(fieldHash, fieldVal, fieldVal);

        }
    webPrintLinkCell(link != NULL ? link : fieldVal);
    }
}

static void printMetadataForTable(char *table)
/* If table exists, _and_ tdb associated with it exists, print out
 * a metadata link that expands on click.  Otherwise print "unavailable" */
{
webPrintLinkCellStart();
struct trackDb *tdb = hashFindVal(trackHash, table);
if (tdb == NULL)
    printf("%s info n/a", table);
else if (!compositeMetadataToggle(database, tdb, "metadata", TRUE, FALSE))
    {
    /* no metadata, but there is a track table to point TB at */
    struct trackDb *parent = trackDbTopLevelSelfOrParent(tdb);
    printf("<A target='_blank' title='browse table' "
                "href='%s?db=%s&hgta_table=%s&hgta_group=%s&hgta_track=%s'>%s</A>",
                    hgTablesName(), database, table, parent->grp, parent->track, table);
    }
webPrintLinkCellEnd();
}

static void queryInputTrackTable(struct dyString *query, char *inputTrackTable,
                                struct slName *fieldList)
/* Construct query in dyString to return contents of inputTrackTable ordered appropriately */
{
sqlDyStringPrintf(query, "select tableName ");
struct slName *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    sqlDyStringPrintf(query, ",%s", field->name);
    }
sqlDyStringPrintf(query, " from %s", inputTrackTable);
if (fieldList != NULL)
    {
    sqlDyStringPrintf(query, " order by ");
    for (field = fieldList; field != NULL; field = field->next)
	{
	sqlDyStringPrintf(query, "%s", field->name);
	if (field->next) 
	    sqlDyStringPrintf(query, ",");
	}
    }
}

static struct hash *getVocabHash(char *fileName)
/* Get vocabulary term hash */
{
struct hash *hash = raTagVals(fileName, "type");
hashAdd(hash, "cellType", NULL);	/* Will the kludge never end, no, never! */
return hash;
}

static void getVocab(struct trackDb *tdb, struct cart *cart, 
                        char **vocabFile, struct hash **vocabHash)
/* Get vocabulary info from trackDb settings (CV or vocab tables) */
{

char *file = NULL;
struct hash *hash = NULL;
char *vocab = trackDbSetting(tdb, "controlledVocabulary");
if (vocabSettingIsEncode(vocab))
    {
    file = cloneFirstWord(vocab);
    if (!sameString(file, "encode/cv.ra"))
	errAbort("expected encode/cv.ra in trackDb settings for controlledVocabulary.");
    file = (char *)cvFile();  // use the library location
    hash = getVocabHash(file);
    }
else
    {
    hash = vocabBasicFromSetting(tdb, cart);
    }
if (vocabFile != NULL)
    *vocabFile = file;
if (hash != NULL)
    *vocabHash = hash;
}

static void printPeakClusterInfo(struct trackDb *tdb, struct cart *cart,
                                struct sqlConnection *conn, char *inputTrackTable, 
                                struct slName *fieldList, struct bed *cluster)
/* Print an HTML table showing sources with hits in the cluster, along with signal.
   If cluster is NULL, show all sources assayed */
{
/* Make the SQL query to get the table and all other fields we want to show
 * from inputTrackTable. */
struct dyString *query = dyStringNew(0);
queryInputTrackTable(query, inputTrackTable, fieldList);

char *vocabFile = NULL;
struct hash *vocabHash = NULL;
getVocab(tdb, cart, &vocabFile, &vocabHash);

int displayNo = 0;
int fieldCount = slCount(fieldList);
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    double signal = 0;
    if (cluster != NULL)
        {
        char *table = row[0];
        signal = getSignalAt(table, cluster);
        if (signal == 0)
            continue;
        }
    printf("</TR><TR>\n");
    webPrintIntCell(++displayNo);
    if (signal != 0)
	webPrintDoubleCell(signal);
    printControlledVocabFields(row+1, fieldCount, fieldList, vocabFile, vocabHash);
    printMetadataForTable(row[0]);
    }
sqlFreeResult(&sr);
dyStringFree(&query);
}

static char *factorSourceVocabLink(char *fieldName, char *fieldVal)
/* Add link to show controlled vocabulary entry for term.
 * Handles 'target' (factor) which is a special case, derived from Antibody entries */
{
char *vocabType = (sameString(fieldName, "target") || sameString(fieldName, "factor")) ?
                    "target" : "term";
return wgEncodeVocabLink(vocabType, fieldVal, fieldVal, fieldVal, "");
}

static void printFactorSourceTableHits(struct factorSource *cluster, struct sqlConnection *conn,
	char *sourceTable, char *inputTrackTable, 
	struct slName *fieldList, boolean invert, char *vocab, struct hash *fieldToUrl)
/* Put out a lines in an html table that shows assayed sources that have hits in this
 * cluster, or if invert is set, that have misses. */
{

/* Make the monster SQL query to get all assays*/
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select %s.id,%s.name,%s.tableName", sourceTable, sourceTable, 
	inputTrackTable);
struct slName *field;
for (field = fieldList; field != NULL; field = field->next)
    sqlDyStringPrintf(query, ",%s.%s", inputTrackTable, field->name);
sqlDyStringPrintf(query, " from %s,%s ", inputTrackTable, sourceTable);
sqlDyStringPrintf(query, " where %s.source = %s.description", inputTrackTable, sourceTable);
sqlDyStringPrintf(query, " and factor='%s' order by %s.source", cluster->name, inputTrackTable);

boolean encodeStanford = FALSE;
if (startsWith("enc", sourceTable))
    encodeStanford = TRUE;

int displayNo = 0;
int fieldCount = slCount(fieldList);
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int sourceId = sqlUnsigned(row[0]);
    boolean hit = FALSE;
    int i;
    double signal = 0.0;
    for (i=0; i<cluster->expCount; i++)
        {
        if (cluster->expNums[i] == sourceId)
            {
            hit = TRUE;
            signal = cluster->expScores[i];
            break;
            }
        }
    if (hit ^ invert)
        {
	printf("</TR><TR>\n");
	webPrintIntCell(++displayNo);
	if (!invert)
	    webPrintDoubleCell(signal);
	webPrintLinkCell(row[1]);
	int i = 0;
        // find position of CV metadata in field list
        int offset = 3;
        struct slName *field = fieldList;
	for (i=0; i<fieldCount && field != NULL; ++i, field = field->next)
	    {
	    char *fieldVal = row[i+offset];
            char *link = NULL;
	    if (vocab)
	        {
                link = cloneString(factorSourceVocabLink(field->name, fieldVal));
		}
            else if (fieldToUrl != NULL)
                {
                // (outside) links on vocab items
                char *urlPattern = hashFindVal(fieldToUrl, field->name);
                if (urlPattern != NULL)
                    {
                    char *url = replaceInUrl(urlPattern, fieldVal, cart, database, seqName, 
                                                winStart, winEnd, NULL, TRUE, NULL);
                    if (url != NULL)
                        {
                        struct dyString *ds = dyStringCreate("<a target='_blank' href='%s'>%s</a>\n",
                                                                url, fieldVal);
                        link = dyStringCannibalize(&ds);
                        }
                    }
                }
            webPrintLinkCell(link ? link : fieldVal);
	    }
        char *table = row[2];
        if (encodeStanford)
            {
            char *file = stringIn("ENCFF", table);
            if (!file)
                webPrintLinkCell(table);
            else
                {
                webPrintLinkCellStart();
                printf("<A target='_blank'"
                        "href='https://www.encodeproject.org/files/%s'>%s</A>", file, file);
                webPrintLinkCellEnd();
               } 
            }
        else
            printMetadataForTable(table);
	}
    }
sqlFreeResult(&sr);
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
    char *inputTrackTable = trackDbRequiredSetting(clusterTdb, "inputTrackTable");
    printPeakClusterInfo(clusterTdb, cart, conn, inputTrackTable, fieldList, NULL);
    }
else
    errAbort("Missing required trackDb setting %s for track %s", "inputTableFieldDisplay", 
                clusterTdb->track);
webPrintLinkTableEnd();
hFreeConn(&conn);
}

void doPeakClusters(struct trackDb *tdb, char *item)
/* Display detailed info about a cluster of DNase peaks from other tracks. */
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
sqlSafef(query, sizeof(query),
	"select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	table, item, seqName, start);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    cluster = bedLoadN(row+rowOffset, 5);
sqlFreeResult(&sr);

if (cluster != NULL)
    {
    /* Get list of subgroups to display */
    char *inputTableFieldDisplay = trackDbSetting(tdb, "inputTableFieldDisplay");
    if (inputTableFieldDisplay != NULL)
        {
	struct slName *fieldList = stringToSlNames(inputTableFieldDisplay);
	char *inputTrackTable = trackDbRequiredSetting(tdb, "inputTrackTable");
	char queryTblSafe[1024];
	sqlSafef(queryTblSafe, sizeof queryTblSafe, "%s", inputTrackTable);

	/* Print out some information about the cluster overall. */
	printf("<B>Items in Cluster:</B> %s of %d<BR>\n", cluster->name, 
	    sqlRowCount(conn, queryTblSafe));
	printf("<B>Cluster Score (out of 1000):</B> %d<BR>\n", cluster->score);
	printPos(cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, TRUE, NULL);

	/* In a new section put up list of hits. */
	webNewSection("List of Items in Cluster");
	webPrintLinkTableStart();
	printClusterTableHeader(fieldList, FALSE, FALSE, TRUE);
	printPeakClusterInfo(tdb, cart, conn, inputTrackTable, fieldList, cluster);
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
hFreeConn(&conn);
}

void doClusterMotifDetails(struct sqlConnection *conn, struct trackDb *tdb, 
                                struct factorSource *cluster)
/* Display details about TF binding motif(s) in cluster */
{
char *motifTable = trackDbSetting(tdb, "motifTable");         // localizations
char *motifPwmTable = trackDbSetting(tdb, "motifPwmTable");   // PWM used to draw sequence logo
char *motifMapTable = trackDbSetting(tdb, "motifMapTable");   // map target to motif
struct slName *motifNames = NULL, *mn; // list of canonical motifs for the factor
struct dnaMotif *motif = NULL;
struct bed6FloatScore *hit = NULL, *maxHit = NULL;
char **row;
char query[256];

if (motifTable != NULL && sqlTableExists(conn, motifTable))
    {
    struct sqlResult *sr;
    int rowOffset;
    char where[256];

    if (motifMapTable == NULL || !sqlTableExists(conn, motifMapTable))
        {
        // Assume cluster name is motif name if there is no map table
        motifNames = slNameNew(cluster->name);
        }
    else
        {
        sqlSafef(query, sizeof(query),
                "select motif from %s where target = '%s'", motifMapTable, cluster->name);
        char *ret = sqlQuickString(conn, query);
        if (ret == NULL)
            {
            // missing target from table -- no canonical motif
            webNewEmptySection();
            return;
            }
        motifNames = slNameListFromString(ret, ',');
        }
    for (mn = motifNames; mn != NULL; mn = mn->next)
        {
        sqlSafef(where, sizeof(where), "name='%s' order by score desc limit 1", mn->name);
        sr = hRangeQuery(conn, motifTable, cluster->chrom, cluster->chromStart,
                     cluster->chromEnd, where, &rowOffset);
        if ((row = sqlNextRow(sr)) != NULL)
            {
            hit = bed6FloatScoreLoad(row + rowOffset);
            if (maxHit == NULL || maxHit->score < hit->score)
                maxHit = hit;
            }
        sqlFreeResult(&sr);
        }
    }
if (maxHit == NULL)
    {
    // Maintain table layout
    webNewEmptySection();
    return;
    }
hit = maxHit;

webNewSection("Canonical Motif in Cluster");
char posLink[1024];
safef(posLink, sizeof(posLink),"<a href=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a>",
        hgTracksPathAndSettings(), database, 
            cluster->chrom, hit->chromStart+1, hit->chromEnd,
            cluster->chrom, hit->chromStart+1, hit->chromEnd);
printf("<b>Motif Name:</b>  %s<br>\n", hit->name);
printf("<b>Motif Score");
printf(":</b>  %.2f<br>\n", hit->score);
printf("<b>Motif Position:</b> %s<br>\n", posLink);
printf("<b>Motif Strand:</b> %c<br>\n", (int)hit->strand[0]);

struct dnaSeq *seq = hDnaFromSeq(database, seqName, hit->chromStart, hit->chromEnd, dnaLower);
if (seq == NULL)
    return;
if (hit->strand[0] == '-')
    reverseComplement(seq->dna, seq->size);
if (motifPwmTable != NULL && sqlTableExists(conn, motifPwmTable))
    {
    motif = loadDnaMotif(hit->name, motifPwmTable);
    if (motif == NULL)
        return;
    motifLogoAndMatrix(&seq, 1, motif);
    }
}

void doFactorSource(struct sqlConnection *conn, struct trackDb *tdb, char *item, int start, int end)
/* Display detailed info about a cluster of TFBS peaks from other tracks. */
{
char extraWhere[256];
sqlSafef(extraWhere, sizeof extraWhere, "name='%s'", item);
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, tdb->table, seqName, start, end, extraWhere, &rowOffset);
char **row = sqlNextRow(sr);
struct factorSource *cluster = NULL;
if (row != NULL)
    cluster = factorSourceLoad(row + rowOffset);
sqlFreeResult(&sr);

if (cluster == NULL)
    errAbort("Error loading cluster from track %s", tdb->track);

char *sourceTable = trackDbRequiredSetting(tdb, "sourceTable");

char *factorLink = cluster->name;
char *vocab = trackDbSetting(tdb, "controlledVocabulary");
if (vocab != NULL)
    {
    factorLink = wgEncodeVocabLink("term", factorLink, factorLink, factorLink, "");
    }

printf("<B>Factor:</B> %s<BR>\n", factorLink);
printf("<B>Cluster Score (out of 1000):</B> %d<BR>\n", cluster->score);
printPos(cluster->chrom, cluster->chromStart, cluster->chromEnd, NULL, TRUE, item);

/* Get list of tracks we'll look through for input. */
char *inputTrackTable = trackDbRequiredSetting(tdb, "inputTrackTable");
char query[256];
sqlSafef(query, sizeof(query), "select tableName from %s where factor='%s' order by source", 
                inputTrackTable, 
    cluster->name);

/* Next do the lists of hits and misses.  We have the hits from the non-zero signals in
 * cluster->expScores.  We need to figure out the sources actually assayed though
 * some other way.  We'll do this by one of two techniques. */
char *inputTableFieldDisplay = trackDbSetting(tdb, "inputTableFieldDisplay");
if (inputTableFieldDisplay != NULL)
    {
    char *fieldUrls = trackDbSetting(tdb, "inputTableFieldUrls");
    struct hash *fieldToUrl = hashFromString(fieldUrls);
    struct slName *fieldList = stringToSlNames(inputTableFieldDisplay);
    char *vocab = trackDbSetting(tdb, "controlledVocabulary");

    /* In a new section put up list of hits. */
    webNewSection("Assays for %s in Cluster", cluster->name);
    webPrintLinkTableStart();
    printClusterTableHeader(fieldList, TRUE, FALSE, TRUE);
    printFactorSourceTableHits(cluster, conn, sourceTable, 
            inputTrackTable, fieldList, FALSE, vocab, fieldToUrl);
    webPrintLinkTableEnd();

    webNewSectionHeaderStart();
    char sectionTitle[128];
    safef(sectionTitle, 
            sizeof(sectionTitle),"Assays for %s Without Hits in Cluster", cluster->name);
    jsBeginCollapsibleSectionOldStyle(cart, tdb->track, "cellNoHits", sectionTitle, FALSE);
    webNewSectionHeaderEnd();
    webPrintLinkTableStart();
    printClusterTableHeader(fieldList, TRUE, FALSE, FALSE);
    printFactorSourceTableHits(cluster, conn, sourceTable, 
            inputTrackTable, fieldList, TRUE, vocab, fieldToUrl);
    webPrintLinkTableEnd();
    jsEndCollapsibleSection();
    }
else
    {
    errAbort("Missing required trackDb setting %s for track %s",
        "inputTableFieldDisplay", tdb->track);
    }
webNewSectionHeaderStart();
jsBeginCollapsibleSectionOldStyle(cart, tdb->track, "cellSources", "Cell Abbreviations", FALSE);
webNewSectionHeaderEnd();
hPrintFactorSourceAbbrevTable(conn, tdb);
jsEndCollapsibleSection();

doClusterMotifDetails(conn, tdb, cluster);
}

