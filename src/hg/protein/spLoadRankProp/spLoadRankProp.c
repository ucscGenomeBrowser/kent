/* spLoadRankProp - rankProp table */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "rankProp.h"
#include "kgXref.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "hgRelate.h"
#include "verbose.h"

static char const rcsid[] = "$Id: spLoadRankProp.c,v 1.2 2004/08/31 00:47:28 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"append", OPTION_BOOLEAN},
    {"keep", OPTION_BOOLEAN},
    {"noLoad", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean gAppend = FALSE;  /* append to table? */
boolean gKeep = FALSE;    /* keep tab file */
boolean gLoad = TRUE;     /* load databases */

struct hash *gProtToKgMap = NULL;  /* has of protein id to kg id */

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

boolean processRow(char **row, FILE *tabFh)
/* parse and process on row of a rankProp file, converting ids to known gene
 * ids */
{
struct rankProp rec;
char *qKgId, *tKgId;
rankPropStaticLoad(row, &rec);
qKgId = hashFindVal(gProtToKgMap, rec.qKgId);
tKgId = hashFindVal(gProtToKgMap, rec.tKgId);
if ((qKgId == NULL) || (tKgId == NULL))
    {
    verbose(2, "can't find kgXref id for %s, skipping %s <=> %s\n",
            ((qKgId == NULL) ? rec.qKgId : rec.tKgId),
            rec.qKgId, rec.tKgId);
    return FALSE;
    }
else
    {
    rec.qKgId = qKgId;
    rec.tKgId = tKgId;
    rankPropTabOut(&rec, tabFh);
    return TRUE;
    }
}

void loadRankProp(struct sqlConnection *conn, char *table, char *rankPropFile)
/* load rankProp file into database */
{
struct lineFile *inLf = lineFileOpen(rankPropFile, TRUE);
FILE *tabFh = hgCreateTabFile(".", table);
char *row[RANKPROP_NUM_COLS];
int total = 0;
int numSkipped = 0;

/* copy file, converting ids */
while (lineFileNextRowTab(inLf, row, RANKPROP_NUM_COLS))
    {
    if (!processRow(row, tabFh))
        numSkipped++;
    total++;
    }
lineFileClose(&inLf);
verbose(1, "skipped %d of %d due to not being in kgXref\n", numSkipped,
        total);
if (gLoad)
    hgLoadTabFile(conn, ".", table, &tabFh);
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
if (!gLoad)
    gKeep = TRUE;
spLoadRankProp(argv[1], argv[2], argv[3]);
return 0;
}
