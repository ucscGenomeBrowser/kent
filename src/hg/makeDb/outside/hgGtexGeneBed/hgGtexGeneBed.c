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
#include "linefile.h"
// TODO: Consider using Appris to pick 'functional' transcript
#include "encode/wgEncodeGencodeAttrs.h"
#include "gtexTissueMedian.h"
#include "gtexGeneBed.h"

#define GTEX_TISSUE_MEDIAN_TABLE  "gtexTissueMedian"

// Versions are used to suffix tablenames
char *gtexVersion = "";
char *gencodeVersion = "V20";

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

// Get canonical transcripts from known genes
verbose(2, "Reading knownCanonical table\n");
struct hash *kcHash = hashNew(0);
sqlSafef(query, sizeof(query),"SELECT transcript FROM knownCanonical");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *ucscId = cloneString(row[0]);
    chopSuffix(ucscId);
    verbose(3, "...Adding ucscId %s to kcHash\n", ucscId);
    hashAdd(kcHash, ucscId, NULL);
    }

// Map to Ensembl transcript IDs
verbose(2, "Reading knownToGencode table\n");
struct hash *tcHash = hashNew(0);
sqlSafef(query, sizeof(query),"SELECT name, value FROM knownToGencode%s", gencodeVersion);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *ucscId = cloneString(row[0]);
    chopSuffix(ucscId);
    verbose(3, "...Looking up ucscId %s in kcHash \n", ucscId);
    if (!hashLookup(kcHash, ucscId))
        // not canonical
        continue;
    char *ensTranscriptId = cloneString(row[1]);
    chopSuffix(ensTranscriptId);
    verbose(3, "...Adding transcriptId %s to tcHash\n", ensTranscriptId);
    hashAdd(tcHash, ensTranscriptId, NULL);
    }
sqlFreeResult(&sr);

// Get Ensembl gene IDs for canonical transcripts
verbose(2, "Reading ensGene table\n");
struct hash *geneHash = newHash(0);
struct geneInfo *geneInfo;
sqlSafef(query, sizeof(query),"SELECT chrom, txStart, txEnd, strand, name, name2 FROM ensGene");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    geneInfo = geneInfoLoad(row);
    verbose(3, "...Looking up transcriptId %s in tcHash\n", geneInfo->transcriptId);
    if (!hashLookup(tcHash, geneInfo->transcriptId))
        // not canonical
        continue;
    hashAdd(geneHash, geneInfo->geneId, geneInfo);
    verbose(3, "...Adding geneId %s to geneHash\n", geneInfo->geneId);
    }
sqlFreeResult(&sr);

// Get gene name and transcript status
struct hash *gaHash = newHash(0);
struct wgEncodeGencodeAttrs *ga;
verbose(2, "Reading wgEncodeGencodeAttrs table\n");
sqlSafef(query, sizeof(query), "SELECT * from wgEncodeGencodeAttrs%s", gencodeVersion);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ga = wgEncodeGencodeAttrsLoad(row);
    chopSuffix(ga->transcriptId);
    verbose(3, "...Adding transcriptId %s to gaHash\n", ga->transcriptId);
    hashAdd(gaHash, ga->transcriptId, ga);
    }
sqlFreeResult(&sr);

// Get data (experiment count and scores), and create BEDs
verbose(2, "Reading gtexTissueMedian table\n");
struct gtexGeneBed *geneBed, *geneBeds = NULL;
struct gtexTissueMedian *gtexData;
struct sqlConnection *connFixed = sqlConnect("hgFixed");
sqlSafef(query, sizeof(query),"SELECT * from %s%s", GTEX_TISSUE_MEDIAN_TABLE, gtexVersion);
sr = sqlGetResult(connFixed, query);
float maxVal = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    gtexData = gtexTissueMedianLoad(row);
    AllocVar(geneBed);
    char *geneId = gtexData->geneId;
    geneBed->geneId = cloneString(geneId);
    geneBed->expCount = gtexData->tissueCount;
    geneBed->expScores = CloneArray(gtexData->scores, gtexData->tissueCount);

    // Get canonical transcript
    verbose(3, "...Looking up geneId %s in geneHash \n", geneId);
    chopSuffix(geneId);
    struct hashEl *el = hashLookup(geneHash, geneId);
    // TODO:  Handle genes not in canonical table (there are 3 of them)
    if (el == NULL)
        {
        warn("Can't find gene %s in geneHash", geneId);
        continue;
        }
    geneInfo = el->val;
    char *transcriptId = geneInfo->transcriptId;
    geneBed->transcriptId = transcriptId;

    // Get other info
    geneBed->chrom = geneInfo->chrom;
    geneBed->chromStart = geneInfo->chromStart;
    geneBed->chromEnd = geneInfo->chromEnd;
    safecpy(geneBed->strand, sizeof(geneBed->strand), geneInfo->strand);

    struct wgEncodeGencodeAttrs *ga = hashFindVal(gaHash, transcriptId);
    if (ga == NULL)
        {
        warn("Can't find transcript %s in wgEncodeGencodeAttrs%s", transcriptId, gtexVersion);
        continue;
        }
     int i;
    if (geneBed->expScores[0] > 0)
        {
        for (i=0; i<geneBed->expCount; i++)
            maxVal = (geneBed->expScores[i] > maxVal ? geneBed->expScores[i] : maxVal);
        }
    geneBed->transcriptClass = ga->transcriptClass;
    geneBed->name = ga->geneName;
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




