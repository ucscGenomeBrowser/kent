/* hgGtexGeneBed - Load BED6+ table of per-gene data from NIH Common Fund Gene Tissue Expression (GTEX)
        Format:  chrom, chromStart, chromEnd, name, score, strand,
                        geneId, geneType, expCount, expScores
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
#include "hdb.h"
#include "basicBed.h"
#include "genePred.h"
#include "linefile.h"
#include "encode/wgEncodeGencodeAttrs.h"
#include "gtexTissueMedian.h"
#include "gtexGeneBed.h"

#define GTEX_TISSUE_MEDIAN_TABLE  "gtexTissueMedian"
#define GTEX_GENE_MODEL_TABLE  "gtexGeneModel"

// Versions are used to suffix tablenames
static char *version = "V6";
static char *gencodeVersion = "V19";

boolean doLoad = FALSE;
char *database, *table;
char *tabDir = ".";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGtexGeneBed - Load BED file of per gene data from GTEX gene model, data and sample tables\n"
  "usage:\n"
  "   hgGtexGeneBed database table\n"
  "options:\n"
  "    -version=VN (default \'%s\')\n"
  "    -gencodeVersion=VNN (default \'%s\')\n"
  "    -noLoad  - If true don't load database and don't clean up tab files\n"
  , version, gencodeVersion);
}

static struct optionSpec options[] = {
    {"version", OPTION_STRING},
    {"gencodeVersion", OPTION_STRING},
    {"noLoad", OPTION_BOOLEAN},
    {NULL, 0},
};


void hgGtexGeneBed(char *database, char *table)
/* Main function to load tables*/
{
char **row;
struct sqlResult *sr;
char query[128];
char buf[256];

struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *connFixed = sqlConnect("hgFixed");

// Get gene id, name and type from GENCODE attributes table
struct hash *gaHash = newHash(0);
struct wgEncodeGencodeAttrs *ga;
verbose(2, "Reading wgEncodeGencodeAttrs table\n");
sqlSafef(query, sizeof(query), "SELECT * from wgEncodeGencodeAttrs%s", gencodeVersion);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ga = wgEncodeGencodeAttrsLoad(row, sqlCountColumns(sr));
    char *baseGeneId = cloneString(ga->geneId);
    chopSuffix(baseGeneId);
    verbose(3, "...Adding geneType %s for gene %s %s to gaHash\n", 
                        ga->geneType, ga->geneName, baseGeneId);
    hashAdd(gaHash, baseGeneId, ga);
    }
sqlFreeResult(&sr);

// Get GTEx gene models
// and GENCODE gene tables
safef(buf, sizeof buf, "%s%s", GTEX_GENE_MODEL_TABLE, version);
if (!sqlTableExists(conn, buf))
    errAbort("Can't find gene model table %s", buf);
struct hash *modelHash = newHash(0);
struct genePred *gp;
sqlSafef(query, sizeof(query), "SELECT * from %s", buf);
verbose(2, "Reading %s table\n", buf);
sr = sqlGetResult(conn, query);
boolean hasBin = hIsBinned(database, buf);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row + (hasBin ? 1 : 0));
    verbose(3, "...Adding gene model %s to modelHash\n", gp->name);
    hashAdd(modelHash, gp->name, gp);
    }
sqlFreeResult(&sr);

// Get data (experiment count and scores), and create BEDs
verbose(2, "Reading gtexTissueMedian table\n");
struct gtexGeneBed *geneBed, *geneBeds = NULL;
struct gtexTissueMedian *gtexData;
sqlSafef(query, sizeof(query),"SELECT * from %s%s", GTEX_TISSUE_MEDIAN_TABLE, version);
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

    // Get gene model
    char *baseGeneId = cloneString(geneId);
    chopSuffix(baseGeneId);
    struct wgEncodeGencodeAttrs *ga = hashFindVal(gaHash, baseGeneId);
    if (ga == NULL)
        {
        warn("Can't find geneId %s in wgEncodeGencodeAttrs%s", baseGeneId, gencodeVersion);
        continue;
        }
     int i;
    if (geneBed->expScores[0] > 0)
        {
        for (i=0; i<geneBed->expCount; i++)
            maxVal = (geneBed->expScores[i] > maxVal ? geneBed->expScores[i] : maxVal);
        }
    geneBed->name = ga->geneName;
    geneBed->geneType = ga->geneType;
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
version = optionVal("version", version);
gencodeVersion = optionVal("gencodeVersion", gencodeVersion);
database = argv[1];
table = argv[2];
hgGtexGeneBed(database, table);
return 0;
}




