/* spLoadPsiBlast - load table of swissprot all-against-all PSI-BLAST data */
#include "common.h"
#include "options.h"
#include "spMapper.h"
#include "localmem.h"
#include "jksql.h"
#include "hash.h"
#include "linefile.h"
#include "hgRelate.h"
#include "hdb.h"
#include "verbose.h"

/*
 * Note: need to keep in-sync with spPsiBlast.as, however the generated struct
 * is not used due to a different in-memory representation.
 */


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
float gMaxEval = 10.0;    /* max e-value to save */
char *gNoMapFile = NULL; /* output dropped sp ids to this file */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spLoadPsiBlast - load swissprot PSL-BLAST table.\n"
  "\n"
  "This loads the results of all-against-all PSI-BLAST on Swissprot, which\n"
  "is done as part of the creating the rankprot table.  This saves the best\n"
  "pair-wise target/query or query/target e-value.\n"
  "\n"
  "usage:\n"
  "   spLoadPsiBlast database table eValFile\n"
  "Options:\n"
  "  -append - append to the table instead of recreating it\n"
  "  -keep - keep tab file used to load database\n"
  "  -noLoad - don't load database, implies -keep\n"
  "  -noMapFile=file - write list of swissprots or uniref ids that couldn't\n"
  "   be mapped.  First column is id, second column is R if it wasn't found\n"
  "   in uniref, or K if it couldn't be associated with a known gene\n"
  "  -maxEval=10.0 - only keep pairs with e-values <= max \n"
  "  -verbose=n - n >=2 lists dropped entries\n"
  "\n"
  "Load a file in the format:\n"
  "   Query= spId comment\n"
  "      bitScore eValue hitSpId\n"
  "      ...\n"
  "   ...\n"
  "\n"
  "swissprot ids will be converted to known gene ids\n"
  "\n"
  );
}

char createString[] =
    "CREATE TABLE %s (\n"
    "    kgId1 varchar(255) not null,	# known genes ids proteins\n"
    "    kgId2 varchar(255) not null,\n"
    "    eValue float not null,	# best e-value\n"
    "              #Indices\n"
    "    INDEX(kgId1(12))\n"
    ");\n";


#define MAX_QUERY_ID  256 /* maximum size of an id + 1 */

boolean readQueryHeader(struct lineFile* inLf,
                        char *querySpId)
/* reader the psi-blast query header line, return FALSE on eof. */
{
char* line, *row[3];
int nCols;

if (!lineFileNext(inLf, &line, NULL))
    return FALSE; /* eof */

/* format: Query= 104K_THEPA 104 KD MICRONEME-RHOPTRY ANTIGEN */
if (!startsWith("Query=", line))
    errAbort("expected Query=, got: %s" , line);

nCols = chopByWhite(line, row, ArraySize(row));
if (nCols < 2)
    lineFileExpectAtLeast(inLf, 2, nCols);
safef(querySpId, MAX_QUERY_ID, "%s", row[1]);
return TRUE;
}

char *readHit(struct lineFile* inLf, float *eValuePtr)
/* read the next hit for the current query, return NULL if
 * end of hits for query. */
{
char *line, *row[3];
int nCols;

/* format:  27 0.30 18C_DROME */
if (!lineFileNext(inLf, &line, NULL))
    return NULL;  /* eof */
if (startsWith("Query=", line))
    {
    lineFileReuse(inLf);  /* save for next call */
    return NULL;  /* end of query */
    }

nCols = chopByWhite(line, row, ArraySize(row));
if (nCols < 3)
    lineFileExpectAtLeast(inLf, 3, nCols);
*eValuePtr = sqlFloat(row[1]);
return row[2];  /* memory is in lineFile buffer */
}

boolean processQuery(struct lineFile* inLf, struct spMapper *mapper, int *maxEvalCntPtr)
/* Read and process a query and target hits */
{
char querySpId[MAX_QUERY_ID], *targetSpId;
float eValue;

if (!readQueryHeader(inLf, querySpId))
    return FALSE; /* eof */
while ((targetSpId = readHit(inLf, &eValue)) != NULL)
    {
    if (eValue <= gMaxEval)
        spMapperMapPair(mapper, querySpId, targetSpId, eValue);
    else
        (*maxEvalCntPtr)++;
    }
return TRUE;
}

void processBlastFile(struct spMapper *mapper, char *blastFile, char *unirefFile)
/* read the reformat psl-blast results */
{
struct lineFile* inLf = lineFileOpen(blastFile, TRUE);
int maxEvalCnt = 0;

while (processQuery(inLf, mapper, &maxEvalCnt))
    continue;
lineFileClose(&inLf);

if (mapper->qtNoUnirefMapCnt > 0)
    verbose(1, "%d pairs dropped due to missing uniref entries\n", mapper->qtNoUnirefMapCnt);
if (mapper->noUnirefMapCnt)
    verbose(1, "%d missing uniref entries\n", mapper->noUnirefMapCnt);
if (mapper->qtMapCnt > 0)
    verbose(1, "%d pairs dropped due to no swissprot to known genes mapping\n", mapper->qtMapCnt);
if (mapper->noSpIdMapCnt > 0)
    verbose(1, "%d swissprot ids not mapped known genes\n", mapper->noSpIdMapCnt);
if (maxEvalCnt > 0)
    verbose(1, "%d entries dropped due to exceeding max E-value\n", maxEvalCnt);
verbose(1, "%d pairs loaded\n", mapper->qtMapCnt);
if (gNoMapFile != NULL)
    spMapperPrintNoMapInfo(mapper, gNoMapFile);
}

void writePairs(struct spMapper *mapper, FILE *tabFh)
/* write saves pairs abd best e-value to file */
{
struct kgPair *kgPair;
for (kgPair = mapper->kgPairList; kgPair != NULL; kgPair = kgPair->next)
    {
    /* write both directions */
    fprintf(tabFh, "%s\t%s\t%0.4g\n", kgPair->kg1Entry->id, kgPair->kg2Entry->id,
            kgPair->score);
    if (!sameString(kgPair->kg1Entry->id, kgPair->kg2Entry->id))
        fprintf(tabFh, "%s\t%s\t%0.4g\n", kgPair->kg2Entry->id, kgPair->kg1Entry->id,
                kgPair->score);
    }
}

void spLoadPsiBlast(char *database, char *table, char *blastFile, char *unirefFile)
/* spLoadPsiBlast - load table of swissprot all-against-all PSI-BLAST data */
{
struct sqlConnection *conn = sqlConnect(database);
char *organism = hScientificName(database);
struct spMapper *mapper = spMapperNew(conn, -1, unirefFile, organism);
FILE *tabFh;

processBlastFile(mapper, blastFile, unirefFile);
tabFh = hgCreateTabFile(".", table);
writePairs(mapper, tabFh);

if (gLoad)
    {
    char query[1024];
    sqlSafef(query, sizeof(query), createString, table);
    if (gAppend)
        sqlMaybeMakeTable(conn, table, query);
    else
        sqlRemakeTable(conn, table, query);
    }
if (gLoad)
    hgLoadTabFileOpts(conn, ".", table, SQL_TAB_FILE_ON_SERVER, &tabFh);
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
gMaxEval = optionFloat("maxEval", gMaxEval);
if (!gLoad)
    gKeep = TRUE;
spLoadPsiBlast(argv[1], argv[2], argv[3], optionVal("unirefFile", NULL));
return 0;
}
