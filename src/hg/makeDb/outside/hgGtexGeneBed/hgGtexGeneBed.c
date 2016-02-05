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

#define GENCODE_COMP_TABLE      "wgEncodeGencodeComp"
#define GENCODE_PSEUDO_TABLE    "wgEncodeGencodePseudoGene"

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
  " NOTE: if gtexGeneModel<version> table doesn't exist, this program will create a .tab file suitable\n"
  "       for loading, and then quit.  Inspect the model file, load it, then re-run."
  , gtexVersion, gencodeVersion);
}

static struct optionSpec options[] = {
    {"gtexVersion", OPTION_STRING},
    {"gencodeVersion", OPTION_STRING},
    {"noLoad", OPTION_BOOLEAN},
    {NULL, 0},
};

void loadGeneModelTable(struct sqlConnection *conn, struct hash *transcriptHash, struct hash *gaHash)
/* Load gtexGeneModel table */
{
// Load up gene preds from GENCODE
verbose(2, "Reading GENCODE tables\n");
struct hash *gencodeHash = newHash(0);
struct genePred *gp = NULL;
char query[256];

sqlSafef(query, sizeof(query),"SELECT * from %s%s", GENCODE_COMP_TABLE, gencodeVersion);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredExtLoad(row+1, 12);
    verbose(3, "...Adding genePred %s to gencodeHash\n", gp->name);
    hashAdd(gencodeHash, gp->name, gp);
    }

sqlSafef(query, sizeof(query),"SELECT * from %s%s", GENCODE_PSEUDO_TABLE, gencodeVersion);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredExtLoad(row+1, 10);
    verbose(3, "...Adding genePred %s to gencodeHash\n", gp->name);
    hashAdd(gencodeHash, gp->name, gp);
    }
// Get transcript ID's of all GTEx genes, and use to retrieve genePred
struct hashCookie cookie = hashFirst(transcriptHash);
struct hashEl *el;
struct genePred *gps = NULL;
while ((el = hashNext(&cookie)) != NULL)
    {
    AllocVar(gp);
    gp->name = el->name;
    char *transcriptName = (char *)el->val;
    verbose(3, "...Looking up transcriptId for name %s in gaHash\n", transcriptName);
    struct wgEncodeGencodeAttrs *ga = hashFindVal(gaHash, transcriptName);
    if (ga == NULL)
        {
        warn("Can't find transcript %s in wgEncodeGencodeAttrs%s", transcriptName, gtexVersion);
        continue;
        }
    verbose(3, "...Looking up transcriptId %s for name %s in gencodeHash\n", ga->transcriptId, transcriptName);
    struct genePred *gpGencode = hashFindVal(gencodeHash, ga->transcriptId);
    if (gpGencode == NULL)
        {
        warn("Can't find transcript %s in gencodeHash", ga->transcriptId);
        continue;
        }
    gp->chrom = cloneString(gpGencode->chrom);
    strcpy(gp->strand, gpGencode->strand);
    gp->txStart = gpGencode->txStart;
    gp->txEnd = gpGencode->txEnd;
    gp->cdsStart = gpGencode->cdsStart;
    gp->cdsEnd = gpGencode->cdsEnd;
    gp->exonCount = gpGencode->exonCount;
    AllocArray(gp->exonStarts, gp->exonCount);
    AllocArray(gp->exonEnds, gp->exonCount);
    int i;
    for (i=0; i<gp->exonCount; i++)
        {
        gp->exonStarts[i] = gpGencode->exonStarts[i];
        gp->exonEnds[i] = gpGencode->exonEnds[i];
        }
    slAddHead(&gps, gp);
    }
slSort(&gps, genePredCmp);
char buf[256];
safef(buf, sizeof buf, "%s%s.%s.genePred", GTEX_GENE_MODEL_TABLE, gtexVersion, database);
if (fopen(buf, "r") != NULL)
    errAbort("File %s exists\n", buf);
FILE *f = fopen(buf, "w");
verbose(2, "Writing model file\n");
if (f == NULL)
    errAbort("Can't create file %s\n", buf);
for (gp = gps; gp != NULL; gp = gp->next)
    genePredOutput(gp, f, '\t', '\n');
fclose(f);
}

void hgGtexGeneBed(char *database, char *table)
/* Main function to load tables*/
{
char **row;
struct sqlResult *sr;
char query[128];
char buf[256];

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
// If table doesn't exist, create it, using transcriptHash, gaHash
// and GENCODE gene tables
safef(buf, sizeof buf, "%s%s", GTEX_GENE_MODEL_TABLE, gtexVersion);
if (!sqlTableExists(conn, buf))
    {
    loadGeneModelTable(conn, transcriptHash, gaHash);
    exit(0);
    }
struct hash *modelHash = newHash(0);
struct genePred *gp;
sqlSafef(query, sizeof(query), "SELECT * from %s", buf);
verbose(2, "Reading %s table\n", buf);
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
    geneBed->chromStart = gp->txStart;
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




