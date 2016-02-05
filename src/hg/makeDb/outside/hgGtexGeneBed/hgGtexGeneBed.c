/* hgGtexGeneBed - Load BED6+ table of per-gene data from NIH Common Fund Gene Tissue Expression (GTEX)
        Format:  chrom, chromStart, chromEnd, name, score, strand,
                        geneId, transcriptId, transcriptClass, expCount, expScores
                                (gtexGeneBed.as)
    Uses hgFixed data tables loaded via hgGtex, and various gene tables.
*/
/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "verbose.h"
#include "options.h"
#include "hash.h"
#include "jksql.h"
#include "hgRelate.h"
#include "basicBed.h"
#include "genePred.h"
#include "linefile.h"
// TODO: Consider using Appris to pick 'functional' transcript
#include "encode/wgEncodeGencodeAttrs.h"
#include "gtexTissueMedian.h"
#include "gtexGeneBed.h"
#include "gtexTranscript.h"

#define GTEX_TISSUE_MEDIAN_TABLE  "gtexTissueMedian"
#define GTEX_TRANSCRIPT_TABLE  "gtexTranscript"
#define GTEX_GENE_MODEL_TABLE  "gtexGeneModel"

// Versions are used to suffix tablenames
char *gtexVersion = "";
char *gencodeVersion = "V19";

boolean doLoad = FALSE;
char *database, *table;
char *tabDir = ".";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGtexGeneBed - Load BED file of per gene data from GTEX data and sample tables\n"
  "usage:\n"
  "   hgGtexGeneBed database table\n"
  "options:\n"
  "    -gtexVersion=VN (default \'%s\')\n"
  "    -gencodeVersion=VNN (default \'%s\')\n"
  "    -noLoad  - If true don't load database and don't clean up tab files\n"
  , gtexVersion, gencodeVersion);
}

static struct optionSpec options[] = {
    {"gtexVersion", OPTION_STRING},
    {"gencodeVersion", OPTION_STRING},
    {"noLoad", OPTION_BOOLEAN},
    {NULL, 0},
};

struct geneInfo 
    {
    struct geneInfo *next;
    char *chrom;
    uint chromStart;
    uint chromEnd;
    char strand[2];
    char *transcriptId;
    char *geneId;
    };

struct geneInfo *geneInfoLoad(char **row)
{
struct geneInfo *geneInfo;
AllocVar(geneInfo);
geneInfo->chrom = cloneString(row[0]);
geneInfo->chromStart = sqlUnsigned(row[1]);
geneInfo->chromEnd = sqlUnsigned(row[2]);
strcpy(geneInfo->strand, row[3]);
geneInfo->transcriptId = cloneString(row[4]);
chopSuffix(geneInfo->transcriptId);
geneInfo->geneId = cloneString(row[5]);
chopSuffix(geneInfo->geneId);
return geneInfo;
}

void hgGtexGeneBed(char *database, char *table)
/* Main function to load tables*/
{
char **row;
struct sqlResult *sr;
char query[128];

struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *connFixed = sqlConnect("hgFixed");

// Get transcript for each gene
verbose(2, "Reading gtexTranscript table\n");
struct hash *transcriptHash = newHash(0);
sqlSafef(query, sizeof(query),"SELECT * from %s%s", GTEX_TRANSCRIPT_TABLE, gtexVersion);
struct gtexTranscript *tx = NULL, *transcripts = gtexTranscriptLoadByQuery(connFixed, query);
for (tx = transcripts; tx != NULL; tx = tx->next)
    {
    hashAdd(transcriptHash, tx->geneId, tx->transcriptName);
    verbose(3, "...Adding geneId %s transcript %s to transcriptHash\n", 
                tx->geneId, tx->transcriptName);
    }

// Get gene name, transcript ID and transcript status from GENCODE attributes table
struct hash *gaHash = newHash(0);
struct wgEncodeGencodeAttrs *ga;
verbose(2, "Reading wgEncodeGencodeAttrs table\n");
sqlSafef(query, sizeof(query), "SELECT * from wgEncodeGencodeAttrs%s", gencodeVersion);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ga = wgEncodeGencodeAttrsLoad(row);
#ifdef HAS_TRANSCRIPT_ID
    // need to notify GTEx DCC that model file lacks transcriptID's
    chopSuffix(ga->transcriptId);
    verbose(3, "...Adding transcriptId %s to gaHash\n", ga->transcriptId);
    hashAdd(gaHash, ga->transcriptId, ga);
#else
    verbose(3, "...Adding transcriptName %s for gene %s to gaHash\n", 
                ga->transcriptName, ga->geneName);
    hashAdd(gaHash, ga->transcriptName, ga);
#endif
    }
sqlFreeResult(&sr);

// Get GTEx gene models
struct hash *modelHash = newHash(0);
struct genePred *gp;
sqlSafef(query, sizeof(query), "SELECT * from %s%s", GTEX_GENE_MODEL_TABLE, gtexVersion);
verbose(2, "Reading %s%s table\n", GTEX_GENE_MODEL_TABLE, gtexVersion);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* skip bin */
    gp = genePredLoad(row+1);
    verbose(3, "...Adding gene model %s to modelHash\n", gp->name);
    hashAdd(modelHash, gp->name, gp);
    }
sqlFreeResult(&sr);

// Get data (experiment count and scores), and create BEDs
verbose(2, "Reading gtexTissueMedian table\n");
struct gtexGeneBed *geneBed, *geneBeds = NULL;
struct gtexTissueMedian *gtexData;
sqlSafef(query, sizeof(query),"SELECT * from %s%s", GTEX_TISSUE_MEDIAN_TABLE, gtexVersion);
sr = sqlGetResult(connFixed, query);
float maxVal = 0;
verbose(3, "transcriptHashSize = %d\n", hashNumEntries(transcriptHash));
while ((row = sqlNextRow(sr)) != NULL)
    {
    gtexData = gtexTissueMedianLoad(row);
    AllocVar(geneBed);
    char *geneId = gtexData->geneId;
    geneBed->geneId = cloneString(geneId);
    geneBed->expCount = gtexData->tissueCount;
    geneBed->expScores = CloneArray(gtexData->scores, gtexData->tissueCount);

    // Get position info from gene models
    struct hashEl *el = hashLookup(modelHash, geneId);
    if (el == NULL)
        {
        warn("Can't find gene %s in modelHash", geneId);
        continue;
        }
    gp = el->val;
    geneBed->chrom = cloneString(gp->chrom);
    geneBed->chromStart = gp->txEnd;
    geneBed->chromEnd = gp->txEnd;
    safecpy(geneBed->strand, sizeof(geneBed->strand), gp->strand);

    // Get transcript for gene
    verbose(3, "...Looking up gene %s in transcriptHash\n", geneId);
    el = hashLookup(transcriptHash, geneId);
    if (el == NULL)
        {
        warn("Can't find gene %s in transcriptHash", geneId);
        continue;
        }
    char *transcriptName = (char *)(el->val);
    struct wgEncodeGencodeAttrs *ga = hashFindVal(gaHash, transcriptName);
    if (ga == NULL)
        {
        warn("Can't find transcript %s in wgEncodeGencodeAttrs%s", transcriptName, gtexVersion);
        continue;
        }
     int i;
    if (geneBed->expScores[0] > 0)
        {
        for (i=0; i<geneBed->expCount; i++)
            maxVal = (geneBed->expScores[i] > maxVal ? geneBed->expScores[i] : maxVal);
        }
    geneBed->name = ga->geneName;
    geneBed->transcriptId = ga->transcriptId;
    geneBed->transcriptClass = ga->transcriptClass;
    slAddHead(&geneBeds, geneBed);
    }
sqlFreeResult(&sr);
sqlDisconnect(&connFixed);

// Create tab file
verbose(2, "Writing tab file %s\n", table);
FILE *bedFile = hgCreateTabFile(tabDir,table);
slSort(&geneBeds, bedCmp);
for (geneBed = geneBeds; geneBed != NULL; geneBed = geneBed->next)
    {
    verbose(3, "Writing gene %s\n", geneBed->name);
    gtexGeneBedOutput(geneBed, bedFile, '\t', '\n');
    }
carefulClose(&bedFile);

if (doLoad)
    {
    verbose(2, "Creating GTEX gene bed table %s\n", table);
    gtexGeneBedCreateTable(conn, table);
    hgLoadTabFile(conn, tabDir, table, &bedFile);
    hgRemoveTabFile(tabDir, table);
    }

sqlDisconnect(&conn);
printf("Max score: %f\n", maxVal);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
doLoad = !optionExists("noLoad");
gtexVersion = optionVal("gtexVersion", gtexVersion);
gencodeVersion = optionVal("gencodeVersion", gencodeVersion);
database = argv[1];
table = argv[2];
hgGtexGeneBed(database, table);
return 0;
}




