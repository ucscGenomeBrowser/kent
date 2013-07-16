/* spLoadRankProp - load rankProp table */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "spMapper.h"
#include "rankProp.h"
#include "rankPropProt.h"
#include "linefile.h"
#include "hgRelate.h"
#include "verbose.h"
#include "obscure.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"append", OPTION_BOOLEAN},
    {"keep", OPTION_BOOLEAN},
    {"noLoad", OPTION_BOOLEAN},
    {"noMapFile", OPTION_STRING},
    {"unirefFile", OPTION_STRING},
    {NULL, 0}
};

boolean gAppend = FALSE;  /* append to table? */
boolean gKeep = FALSE;    /* keep tab file */
boolean gLoad = TRUE;     /* load databases */
char *gNoMapFile = NULL; /* output dropped sp ids to this file */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spLoadRankProp - load swissprot rankProp table.\n"
  "usage:\n"
  "   spLoadRankProp database table rankPropFile\n"
  "Options:\n"
  "  -unirefFile=tab - tab-separated file containing uniref information.\n"
  "   If specified, used to map accessions to known genes associated with\n"
  "   all proteins in a uniref entry.\n"
  "  -append - append to the table instead of recreating it\n"
  "  -keep - keep tab file used to load database\n"
  "  -noLoad - don't load database, implies -keep\n"
  "  -noMapFile=file - write list of swissprots or uniref ids that couldn't\n"
  "   be mapped.  First column is id, second column is R if it wasn't found\n"
  "   in uniref, or K if it couldn't be associated with a known gene\n"
  "  -verbose=n - n >= 3 lists mappings\n"
  "\n"
  "Load a file in the format:\n"
  "   prot1Acc prot2Acc ranking eVal12 eVal21\n"
  "these will be converted to known gene ids\n"
  "\n"
  );
}

char createString[] =
    "CREATE TABLE %s (\n"
    "    query varchar(255) not null,	# known genes id of query protein\n"
    "    target varchar(255) not null,	# known genes id of target protein\n"
    "    score float not null,	# rankp score\n"
    "              #Indices\n"
    "    INDEX(query(12))\n"
    ");\n";

void processRow(char **row, struct spMapper *mapper, int *zeroScoreCntPtr)
/* Parse and process on row of a rankProp file, converting ids to known gene
 * ids. Only save if score is greater than zero */
{
struct rankPropProt rpRec;
rankPropProtStaticLoad(row, &rpRec);

if (rpRec.score == 0.0)
    {
    verbose(2, "zero score for %s <=> %s\n", rpRec.qSpId, rpRec.tSpId);
    (*zeroScoreCntPtr)++;
    }
else 
    {
    spMapperMapPair(mapper, rpRec.qSpId, rpRec.tSpId, rpRec.score);
    }
}

void outputHit(FILE *tabFh, struct kgPair *kgPair)
/* output rankProp record for a kg query/target pair */
{
/* output both directions */
struct rankProp outRec;
outRec.score = kgPair->score;

outRec.query = kgPair->kg1Entry->id;
outRec.target = kgPair->kg2Entry->id;
rankPropTabOut(&outRec, tabFh);

if (!sameString(kgPair->kg1Entry->id, kgPair->kg2Entry->id))
    {
    outRec.query = kgPair->kg2Entry->id;
    outRec.target = kgPair->kg1Entry->id;
    rankPropTabOut(&outRec, tabFh);
    }
}

void outputHits(FILE *tabFh, struct spMapper *mapper)
/* output save pairs and scores */
{
struct kgPair *kgPair;
for (kgPair = mapper->kgPairList; kgPair != NULL; kgPair = kgPair->next)
    outputHit(tabFh, kgPair);
}

void processRankProp(struct spMapper *mapper, char *rankPropFile, char *unirefFile)
/* process rankProp file, loading into mapper */
{
struct lineFile *inLf = lineFileOpen(rankPropFile, TRUE);
char *row[RANKPROPPROT_NUM_COLS];
int zeroScoreCnt = 0;

/* copy file, converting ids */
while (lineFileNextRowTab(inLf, row, RANKPROPPROT_NUM_COLS))
    processRow(row, mapper, &zeroScoreCnt);
lineFileClose(&inLf);

if (mapper->qtNoUnirefMapCnt > 0)
    verbose(1, "%d pairs dropped due to missing uniref entries\n", mapper->qtNoUnirefMapCnt);
if (mapper->noUnirefMapCnt)
    verbose(1, "%d missing uniref entries\n", mapper->noUnirefMapCnt);
if (mapper->qtMapCnt > 0)
    verbose(1, "%d pairs dropped due to no swissprot to known genes mapping\n", mapper->qtMapCnt);
if (mapper->noSpIdMapCnt > 0)
    verbose(1, "%d swissprot ids not mapped known genes\n", mapper->noSpIdMapCnt);
if (zeroScoreCnt > 0)
    verbose(1, "%d entries dropped due to zero score\n", zeroScoreCnt);
verbose(1, "%d pairs mapped\n", mapper->qtMapCnt);
if (gNoMapFile != NULL)
    spMapperPrintNoMapInfo(mapper, gNoMapFile);
}

/* output rankProp record for a kg query/target pair */
void spLoadRankProp(char *database, char *table, char *rankPropFile,
                    char *unirefFile)
/* load a rankProp table */
{
char query[1024];
struct sqlConnection *conn = sqlConnect(database);
char *organism = hScientificName(database);
struct spMapper *mapper = spMapperNew(conn, 1, unirefFile, organism);
FILE *tabFh;

processRankProp(mapper, rankPropFile, unirefFile);
tabFh = hgCreateTabFile(".", table);
outputHits(tabFh, mapper);

if (gLoad)
    {
    sqlSafef(query, sizeof(query), createString, table);
    if (gAppend)
        sqlMaybeMakeTable(conn, table, query);
    else
        sqlRemakeTable(conn, table, query);
    }

if (gLoad)
    hgLoadTabFileOpts(conn, ".", table, SQL_TAB_FILE_ON_SERVER, &tabFh);
else
    carefulClose(&tabFh);
if (!gKeep)
    hgRemoveTabFile(".", table);

sqlDisconnect(&conn);
spMapperFree(&mapper);
freeMem(organism);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong # args");
gAppend = optionExists("append");
gKeep = optionExists("keep");
gLoad = !optionExists("noLoad");
gNoMapFile = optionVal("noMapFile", NULL);
if (!gLoad)
    gKeep = TRUE;
spLoadRankProp(argv[1], argv[2], argv[3], optionVal("unirefFile", NULL));
return 0;
}
