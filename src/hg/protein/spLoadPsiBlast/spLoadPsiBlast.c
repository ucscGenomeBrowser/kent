/* spLoadPsiBlast - load table of swissprot all-against-all PSI-BLAST data */
#include "common.h"
#include "options.h"
#include "spKgMap.h"
#include "jksql.h"
#include "hash.h"
#include "linefile.h"
#include "hgRelate.h"
#include "verbose.h"

/*
 * Note: need to keep in-sync with spPsiBlast.as, however the generated struct
 * is not used due to a different in-memory representation.
 */

static char const rcsid[] = "$Id: spLoadPsiBlast.c,v 1.2 2005/01/08 03:25:29 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"append", OPTION_BOOLEAN},
    {"keep", OPTION_BOOLEAN},
    {"noLoad", OPTION_BOOLEAN},
    {"noKgIdFile", OPTION_STRING},
    {NULL, 0}
};

boolean gAppend = FALSE;  /* append to table? */
boolean gKeep = FALSE;    /* keep tab file */
boolean gLoad = TRUE;     /* load databases */
boolean gNoKdMap = FALSE; /* don't map to kg ids */
float gMaxEval = 10.0;    /* max e-value to save */
char *gNoKgIdFile = NULL; /* output dropped sp ids to this file */

struct spKgMap *gSpKgMap = NULL;  /* has of swissprot id to kg id. */

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
  "  -noKgIdFile=file - write list of swissprots dropped because a known gene\n"
  "   id was not found.\n"
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



/* hash table of best evalues for query and target ids.  A naive
 * implementation would have the pair as a single string, but this greatly
 * increase the memory usage.  Instead, both are entered separately, point to
 * the same struct. */
struct hash *gPairEvalHash = NULL;
struct pairEval *gPairEvalList = NULL;

struct kgEntry
/* struct used to store a known gene id with a link to the next entry
 * and the associated pairEval struct */
{
    struct kgEntry *next;
    char *id;                    /* kd id, memory not owned by this struct */
    struct pairEval *pairEval;  /* containing object shared e-value */
};

struct pairEval
/* structure used to keep the best e-value of a pair of kg ids. */
{
    struct pairEval *next;
    struct kgEntry *kg1Entry;   /* pointers back to each entry */
    struct kgEntry *kg2Entry;
    float eValue;   /* best e-value for the pair */
};

struct kgEntry *kgEntryAdd(struct hashEl* kgHel, struct pairEval *pairEval)
/* Add a new kgEntry to a hash chain */
{
struct kgEntry *kgEntry;
lmAllocVar(gPairEvalHash->lm, kgEntry);
kgEntry->id = kgHel->name;
kgEntry->pairEval = pairEval;
slAddHead((struct kgEntry**)&kgHel->val, kgEntry);
return kgEntry;
}

void pairEvalAdd(struct hashEl* kg1Hel, struct hashEl* kg2Hel, 
                 float eValue)
/* add a new pairEval object to the hash */
{
struct pairEval *pairEval;

lmAllocVar(gPairEvalHash->lm, pairEval);
pairEval->eValue = eValue;
slAddHead(&gPairEvalList, pairEval);

/* link both kgId objects in struct */
pairEval->kg1Entry = kgEntryAdd(kg1Hel, pairEval);
pairEval->kg2Entry = kgEntryAdd(kg2Hel, pairEval);
}

struct pairEval *pairEvalFind(struct hashEl* queryHel, struct hashEl* targetHel)
/* find a pairEval object for a pair of kg ids, or NULL if not in table */
{
struct kgEntry *entry;

/* search for an entry for the pair.  Ok to compare string ptrs here, since
 * they are in same local memory.  Don't know order in structure, so must
 * check both ways.
 */
for (entry = queryHel->val; entry != NULL; entry = entry->next)
    {
    if (((entry->pairEval->kg1Entry->id == queryHel->name)
         && (entry->pairEval->kg2Entry->id == targetHel->name))
        || ((entry->pairEval->kg1Entry->id == targetHel->name)
            && (entry->pairEval->kg2Entry->id == queryHel->name)))
        return entry->pairEval;
    }
return NULL;
}

void pairEvalSave(char *queryId, char *targetId, float eValue)
/* save a bi-directional e-value for a query/target pair */
{
struct hashEl *queryHel = hashStore(gPairEvalHash, queryId);
struct hashEl *targetHel = hashStore(gPairEvalHash, targetId);
struct pairEval *pairEval = pairEvalFind(queryHel, targetHel);
if (pairEval != NULL)
    {
    if (pairEval->eValue < eValue)
        pairEval->eValue = eValue;
    }
else
    pairEvalAdd(queryHel, targetHel, eValue);
}

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

void processQueryHit(struct lineFile* inLf, struct slName *queryKgIds,
                     char *targetSpId, float eValue)
/* Process a query hit, save in table if it can be mapped to a known gene id */
{
struct slName *targetKgIds = spKgMapGet(gSpKgMap, targetSpId);
struct slName *tKgId, *qKgId;

for (qKgId = queryKgIds; qKgId != NULL; qKgId = qKgId->next)
    {
    for (tKgId = targetKgIds; tKgId != NULL; tKgId = tKgId->next)
        pairEvalSave(qKgId->name, tKgId->name, eValue);
    }
}

boolean readQuery(struct lineFile* inLf)
/* Read a query and target hits */
{
char querySpId[MAX_QUERY_ID], *targetSpId;
struct slName *queryKgIds;
float eValue;

if (!readQueryHeader(inLf, querySpId))
    return FALSE; /* eof */
queryKgIds = spKgMapGet(gSpKgMap, querySpId);

while ((targetSpId = readHit(inLf, &eValue)) != NULL)
    {
    if ((queryKgIds != NULL) && (eValue <= gMaxEval))
        processQueryHit(inLf, queryKgIds, targetSpId, eValue);
    }
return TRUE;
}

void readBlastFile(char *blastFile)
/* read the reformat psl-blast results */
{
struct lineFile* inLf = lineFileOpen(blastFile, TRUE);

while (readQuery(inLf))
    continue;
lineFileClose(&inLf);
}

void writePairs(FILE *tabFh)
/* write saves pairs abd best e-value to file */
{
struct pairEval *pairEval;
for (pairEval = gPairEvalList; pairEval != NULL; pairEval = pairEval->next)
    {
    /* write both directions */
    fprintf(tabFh, "%s\t%s\t%0.4g\n", pairEval->kg1Entry->id, pairEval->kg2Entry->id,
            pairEval->eValue);
    fprintf(tabFh, "%s\t%s\t%0.4g\n", pairEval->kg2Entry->id, pairEval->kg1Entry->id,
            pairEval->eValue);
    }
}

void spLoadPsiBlast(char *database, char *table, char *blastFile)
/* spLoadPsiBlast - load table of swissprot all-against-all PSI-BLAST data */
{
struct sqlConnection *conn = sqlConnect(database);
int numMissing;
FILE *tabFh;
gPairEvalHash = hashNew(21);

gSpKgMap = spKgMapNew(conn);
readBlastFile(blastFile);
numMissing = spKgMapCountMissing(gSpKgMap);
if (numMissing > 0)
    verbose(1, "%d proteins could not be mapped to known genes\n", numMissing);
if (gNoKgIdFile != NULL)
    spKgMapListMissing(gSpKgMap, gNoKgIdFile);


tabFh = hgCreateTabFile(".", table);
writePairs(tabFh);
if (gLoad)
    {
    char query[1024];
    safef(query, sizeof(query), createString, table);
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
gNoKgIdFile = optionVal("noKgIdFile", NULL);
gMaxEval = optionFloat("maxEval", gMaxEval);
if (!gLoad)
    gKeep = TRUE;
spLoadPsiBlast(argv[1], argv[2], argv[3]);
return 0;
}
