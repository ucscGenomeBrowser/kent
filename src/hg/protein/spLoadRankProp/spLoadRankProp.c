/* spLoadRankProp - rankProp table */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "rankProp.h"
#include "rankPropProt.h"
#include "kgXref.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "hgRelate.h"
#include "verbose.h"

static char const rcsid[] = "$Id: spLoadRankProp.c,v 1.3 2004/10/05 08:04:27 markd Exp $";

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
char *gNoKgIdFile = NULL; /* output dropped sp ids to this file */

struct hash *gProtToKgMap = NULL;  /* has of protein id to kg id. Also
                                    * contains entries with a NULL value for
                                    * proteins not found in kgProtMap so these
                                    * can be counted. There maybe multiple
                                    * entries for a given swissprot id. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spLoadRankProp - load swissprot rankProp table.\n"
  "usage:\n"
  "   spLoadRankProp database table rankPropFile\n"
  "Options:\n"
  "  -append - append to the table instead of recreating it\n"
  "  -keep - keep tab file used to load database\n"
  "  -noLoad - don't load database, implies -keep\n"
  "  -noKgIdFile=file - write list of swissprots dropped because a known gene\n"
  "   id was not found.\n"
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
    "    qKgId varchar(255) not null,	# known genes id of query protein\n"
    "    tKgId varchar(255) not null,	# known genes id of target protein\n"
    "    score float not null,	# rankp score\n"
    "    qtEVal double not null,	# query to target psi-blast E-value\n"
    "    tqEVal double not null,	# target to query psi-blast E-value\n"
    "              #Indices\n"
    "    INDEX(qKgId(12))\n"
    ");\n";

void buildProtToKgMap(struct sqlConnection *conn)
/* build gProtToKgMap from kgXRef table */
{
char query[1024];
struct kgXref kgXref;
struct sqlResult *sr;
char *kgId;
char **row;

gProtToKgMap = hashNew(21);
safef(query, sizeof(query), "SELECT * from kgXref");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    kgXrefStaticLoad(row, &kgXref);
    kgId = lmCloneString(gProtToKgMap->lm, kgXref.kgID);
    hashAdd(gProtToKgMap, kgXref.spID, kgId);
    }
sqlFreeResult(&sr);
}

char *getKgId(char *spId)
/* get the kgId for a swissprot.  If not found, enter in the
 * table for later counting */
{
struct hashEl *hel = hashStore(gProtToKgMap, spId);
if (hel->val == NULL)
    return NULL;
else
    return hel->val;
}

int countKgMissing()
/* count the number kgIds that were not found */
{
int missing = 0;
struct hashCookie cookie = hashFirst(gProtToKgMap);
struct hashEl *hel;

while ((hel = hashNext(&cookie)) != NULL)
    {
    if (hel->val == NULL)
        missing++;
    }
return missing;
}

void listKgMissing(char *file)
/* output the spIds for the kgIds that were not found */
{
FILE *fh = mustOpen(file, "w");
struct hashCookie cookie = hashFirst(gProtToKgMap);
struct hashEl *hel;

while ((hel = hashNext(&cookie)) != NULL)
    {
    if (hel->val == NULL)
        fprintf(fh, "%s\n", hel->name);
    }
carefulClose(&fh);
}

enum rowStat
/* reason a row was skipped */
{
    rowKeep,             /* row was loaded */
    rowNoKgXref,         /* target or query not in kgXref */
    rowSingleton         /* no score or e-value */
};

void outputEntry(struct rankPropProt *inRec, FILE *tabFh,
                 struct hashEl *qKgHel, struct hashEl *tKgHel)
/* output entry for all combinations of kgIDs corrisponding to the swissprot
 * pair */
{
struct rankProp outRec;
struct hashEl *qKgScan,  *tKgScan;

outRec.score = inRec->score;
outRec.qtEVal = inRec->qtEVal;
outRec.tqEVal = inRec->tqEVal;

for (qKgScan = qKgHel; qKgScan != NULL; qKgScan = hashLookupNext(qKgScan))
    {
    outRec.qKgId = qKgScan->val;
    for (tKgScan = tKgHel; tKgScan != NULL; tKgScan = hashLookupNext(tKgScan))
        {
        outRec.tKgId = tKgScan->val;
        rankPropTabOut(&outRec, tabFh);
        }
    }
}

enum rowStat processRow(char **row, FILE *tabFh)
/* Parse and process on row of a rankProp file, converting ids to known gene
 * ids. Only output if score is greater than zero or there is an e-value */
{
struct rankPropProt inRec;
struct rankProp outRec;
struct hashEl *qKgHel, *tKgHel;
rankPropProtStaticLoad(row, &inRec);

if ((inRec.score == 0.0) && (inRec.qtEVal < 0) && (inRec.tqEVal < 0))
    {
    verbose(2, "no score or edges for %s <=> %s\n",
            inRec.qSpId, inRec.tSpId);
    return rowSingleton;;
    }

/* since a swissprot can be associated with multiple kgIds, need to
 * loop over all combinations.  Use hashStore so NULL entries are added
 * for ids that are not found. */

qKgHel = hashStore(gProtToKgMap, inRec.qSpId);
tKgHel = hashStore(gProtToKgMap, inRec.tSpId);
if ((qKgHel->val == NULL) || (tKgHel == NULL))
    {
    verbose(2, "can't find kgXref id for %s, skipping %s <=> %s\n",
            ((qKgHel->val == NULL) ? inRec.qSpId : inRec.tSpId),
            inRec.qSpId, inRec.tSpId);
    return rowNoKgXref;
    }
else
    {
    outputEntry(&inRec, tabFh, qKgHel, tKgHel);
    return rowKeep;
    }
}

void loadRankProp(struct sqlConnection *conn, char *table, char *rankPropFile)
/* load rankProp file into database */
{
struct lineFile *inLf = lineFileOpen(rankPropFile, TRUE);
FILE *tabFh = hgCreateTabFile(".", table);
char *row[RANKPROPPROT_NUM_COLS];
int statCnts[3]; /* indexed by status */

statCnts[rowKeep] = 0;
statCnts[rowNoKgXref] = 0;
statCnts[rowSingleton] = 0;

/* copy file, converting ids */
while (lineFileNextRowTab(inLf, row, RANKPROPPROT_NUM_COLS))
    {
    enum rowStat stat = processRow(row, tabFh);
    statCnts[stat]++;
    }
lineFileClose(&inLf);
if (statCnts[rowNoKgXref] > 0)
    {
    verbose(1, "skipped %d pairs due to not being in kgXref \n", statCnts[rowNoKgXref]);
    verbose(1, "%d proteins could not be mapped\n", countKgMissing());
    }
if (statCnts[rowSingleton] > 0)
    verbose(1, "skipped %d entries due to no score and no edges\n", statCnts[rowSingleton]);
verbose(1, "kept %d pairs\n", statCnts[rowKeep]);
if (gNoKgIdFile != NULL)
    listKgMissing(gNoKgIdFile);

if (gLoad)
    hgLoadTabFileOpts(conn, ".", table, SQL_TAB_FILE_ON_SERVER, &tabFh);
if (!gKeep)
    hgRemoveTabFile(".", table);
}

void spLoadRankProp(char *database, char *table, char *rankPropFile)
/* spLoadRankProp - load a rankProp table */
{
char query[1024];
struct sqlConnection *conn = sqlConnect(database);

buildProtToKgMap(conn);

if (gLoad)
    {
    safef(query, sizeof(query), createString, table);
    if (gAppend)
        sqlMaybeMakeTable(conn, table, query);
    else
        sqlRemakeTable(conn, table, query);
    }
loadRankProp(conn, table, rankPropFile);
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
if (!gLoad)
    gKeep = TRUE;
spLoadRankProp(argv[1], argv[2], argv[3]);
return 0;
}
