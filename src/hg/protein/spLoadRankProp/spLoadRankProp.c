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

static char const rcsid[] = "$Id: spLoadRankProp.c,v 1.6 2005/07/09 05:18:56 markd Exp $";

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
  "  -verbose=n - n >=2 lists dropped entries\n"
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


void outputHit(FILE *tabFh, struct rankPropProt *rpRec, struct spMapperPairs *kgPairs)
/* output a rankProp hit for the kg query/target pairs */
{
struct rankProp outRec;
struct spMapperPairs *pairs;
struct spMapperId *tId;
outRec.score = rpRec->score;

for (pairs = kgPairs; pairs != NULL; pairs = pairs->next)
    {
    outRec.query = pairs->qId;
    for (tId = pairs->tIds; tId != NULL; tId = tId->next)
        {
        outRec.target = tId->id;
        rankPropTabOut(&outRec, tabFh);
        }
    }
}

void processRow(char **row, struct spMapper *mapper, FILE *tabFh, int *zeroScoreCntPtr)
/* Parse and process on row of a rankProp file, converting ids to known gene
 * ids. Only output if score is greater than zero */
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
    struct spMapperPairs *kgPairs = spMapperMapPair(mapper, rpRec.qSpId, rpRec.tSpId);
    outputHit(tabFh, &rpRec, kgPairs);
    spMapperPairsFree(&kgPairs);
    }
}

void processRankProp(struct sqlConnection *conn, char *rankPropFile, char *unirefFile, FILE *tabFh)
/* process rankProp file, creating load file */
{
char *organism = hScientificName(sqlGetDatabase(conn));
struct lineFile *inLf = lineFileOpen(rankPropFile, TRUE);
struct spMapper *mapper = spMapperNew(conn, unirefFile, organism);
char *row[RANKPROPPROT_NUM_COLS];
int zeroScoreCnt = 0;

/* copy file, converting ids */
while (lineFileNextRowTab(inLf, row, RANKPROPPROT_NUM_COLS))
    processRow(row, mapper, tabFh, &zeroScoreCnt);
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
verbose(1, "%d pairs loaded\n", mapper->qtMapCnt);
if (gNoMapFile != NULL)
    spMapperPrintNoMapInfo(mapper, gNoMapFile);
spMapperFree(&mapper);
freeMem(organism);
}

void spLoadRankProp(char *database, char *table, char *rankPropFile,
                    char *unirefFile)
/* spLoadRankProp - load a rankProp table */
{
char query[1024];
struct sqlConnection *conn = sqlConnect(database);
FILE *tabFh = hgCreateTabFile(".", table);

if (gLoad)
    {
    safef(query, sizeof(query), createString, table);
    if (gAppend)
        sqlMaybeMakeTable(conn, table, query);
    else
        sqlRemakeTable(conn, table, query);
    }
processRankProp(conn, rankPropFile, unirefFile, tabFh);

if (gLoad)
    hgLoadTabFileOpts(conn, ".", table, SQL_TAB_FILE_ON_SERVER, &tabFh);
else
    carefulClose(&tabFh);
if (!gKeep)
    hgRemoveTabFile(".", table);

sqlDisconnect(&conn);
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
