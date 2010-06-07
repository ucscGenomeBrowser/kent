/* orfeomeDbLoad - create and load ORFeome tracks into the database. */

#include "common.h"
#include "options.h"
#include "portable.h"
#include "linefile.h"
#include "genbank.h"
#include "psl.h"
#include "hgRelate.h"
#include "hdb.h"
#include "jksql.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include "gbSql.h"
#include "gbDefs.h"
#include "gbGenome.h"
#include "dystring.h"
#include "hdb.h"
#include "orfeomeImageIds.h"

static char const rcsid[] = "$Id: orfeomeDbLoad.c,v 1.4 2008/09/03 19:19:36 markd Exp $";

/* Notes:
 *  - Identifies ORFeome clones by both image id and genbank keywords, as it
 *    was taking a long time to get all of the data in place.
 *
 *  - Several attempts at generating the orfeomeMRna alignment table from
 *    the all_mrna table using INSERT ... SELECT with a join.  However this
 *    ways really, really slow (one version didn't complete after 9 hours).
 *    Various attempts to optimize it failed.  It was demonstrated that the
 *    select was the problem, not the insert.  So this was changed to build a tmp
 *    table of accessions and do a insert via join from just this table..
 */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"drop", OPTION_BOOLEAN},
    {"workdir", OPTION_STRING},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

/* file with ORFeome IMAGE ids */
static char *orfeomeImageIdsFile = "etc/orfeome.imageIds";

/* table names */
static char *ORFEOME_MRNA_TBL = "orfeomeMrna";
static char *ORFEOME_GENES_TBL = "orfeomeGenes";

/* list of all tables */
static char *orfeomeTables[] =
{
    "orfeomeMrna",
    "orfeomeGenes",
    NULL
};

/* temporary table to use in select, automatically deleted on conntect close*/
static char *orfeomeAccTmpTbl = "orfeomeAcc_tmp";

/* command line globals */
static char *workDir;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orfeomeDbLoad - load ORFeome tracks into the databases.   This creates\n"
  "tables by selecting from the standard Genbank alignment tracks\n"
  "\n"
  "Usage:\n"
  "   orfeomeDbLoad [options] database\n"
  "\n"
  "   o database - database to load\n"
  "\n"
  "Options:\n"
  "    -drop - drop existing tables \n"
  "    -workdir=work/load/orfeome - temporary directory for load files.\n"
  "    -verbose=n - enable verbose output\n"
  "              n >= 1 - basic information abort each step\n"
  "              n >= 2 - more details\n"
  "              n >= 5 - SQL queries\n"
  "\n"
  );
}

static struct orfeomeImageIds* loadOrfeomeImageIds(char *db)
/* load ORFeome image id ranges for this organism */
{
struct gbGenome* genome = gbGenomeNew(db);
struct orfeomeImageIds *allIds = orfeomeImageIdsLoadAllByTab(orfeomeImageIdsFile);
struct orfeomeImageIds *dbIds = NULL, *ids;
while ((ids = slPopHead(&allIds)) != NULL)
    {
    if (gbGenomeOrgCat(genome, ids->organism) == GB_NATIVE)
        slAddHead(&dbIds, ids);
    else
        orfeomeImageIdsFree(&ids);
    }
slReverse(&dbIds);
gbGenomeFree(&genome);
return dbIds;
}

static void selectAccByImageId(struct sqlConnection *conn, struct orfeomeImageIds *imageIds, struct hash *accSet)
/* select ORFeome accession by image id */
{
struct dyString *sql = dyStringNew(1024);
struct orfeomeImageIds *ids;
dyStringPrintf(sql, "SELECT acc FROM imageClone WHERE ");
for (ids = imageIds; ids != NULL; ids = ids->next)
    {
    if (ids != imageIds)
        dyStringAppend(sql, " OR ");
    dyStringPrintf(sql, "(%d <= imageId AND imageId <= %d)", ids->imageFirst, ids->imageLast);
    }
struct sqlResult *sr = sqlGetResult(conn, sql->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    hashStore(accSet, row[0]);
sqlFreeResult(&sr);
dyStringFree(&sql);
}

static void getAccByImageId(struct sqlConnection *conn, struct hash *accSet)
/* add ORFeome accession by image id */
{
struct orfeomeImageIds *imageIds = loadOrfeomeImageIds(sqlGetDatabase(conn));
if (imageIds != NULL)
    selectAccByImageId(conn, imageIds, accSet);
orfeomeImageIdsFreeList(&imageIds);
}

static void getAccByKeyword(struct sqlConnection *conn, struct hash *accSet)
/* add ORFeome accession by keyword search */
{
struct sqlResult *sr = sqlGetResult(conn,
                                    "SELECT acc FROM gbCdnaInfo,keyword WHERE (keyword.name LIKE \"%orfeome%\") AND (gbCdnaInfo.keyword = keyword.id)");
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    hashStore(accSet, row[0]);
sqlFreeResult(&sr);
}

static struct hash* getOrfeomeAccs(struct sqlConnection *conn)
/* build set of ORFeome GenBank accessions */
{
struct hash *accSet = hashNew(20);
getAccByImageId(conn, accSet);
getAccByKeyword(conn, accSet);
return accSet;
}

static void buildOrfeomeAccTmpTbl(struct sqlConnection *conn, struct hash *accSet)
/* build tmp table of ORFeome accessions */
{
// build tmp file
char tmpFile[PATH_LEN];
safef(tmpFile, sizeof(tmpFile), "%s/%s.%s.tab", workDir, orfeomeAccTmpTbl, sqlGetDatabase(conn));
FILE *fh = mustOpen(tmpFile, "w");

struct hashCookie hc = hashFirst(accSet);
struct hashEl *hel;
while ((hel = hashNext(&hc)) != NULL)
    fprintf(fh, "%s\n", hel->name);

carefulClose(&fh);

// create and load table;
char sql[4096];
safef(sql, sizeof(sql), "CREATE TEMPORARY TABLE %s (acc char(16) primary key)",
      orfeomeAccTmpTbl);
sqlUpdate(conn, sql);
sqlLoadTabFile(conn, tmpFile, orfeomeAccTmpTbl, SQL_TAB_FILE_ON_SERVER);

if (remove(tmpFile) < 0)
    errnoAbort("Couldn't remove %s", tmpFile);
}

static void loadOrfeomeMrnaTbl(struct sqlConnection *conn, char *tmpTbl)
/* load the orfeomeMrna table from the mRNA table with join from tmp acc table */
{
char sql[1024];
safef(sql, sizeof(sql), "INSERT INTO %s SELECT all_mrna.* FROM all_mrna,%s WHERE all_mrna.qName = %s.acc",
      tmpTbl, orfeomeAccTmpTbl, orfeomeAccTmpTbl);
sqlUpdate(conn, sql);
}

static void createOrfeomeMrnaTbl(struct sqlConnection *conn)
/* create the orfeomeMrna table */
{
char tmpTbl[32];
tblBldGetTmpName(tmpTbl, sizeof(tmpTbl), ORFEOME_MRNA_TBL);
gbVerbEnter(2, "loading %s", tmpTbl);
tblBldRemakePslTable(conn, tmpTbl, "all_mrna");

struct hash* accSet = getOrfeomeAccs(conn);
buildOrfeomeAccTmpTbl(conn, accSet);
hashFree(&accSet);

loadOrfeomeMrnaTbl(conn, tmpTbl);
gbVerbLeave(2, "loading %s", tmpTbl);
}

static void createOrfeomeGenesTbl(struct sqlConnection *conn)
/* create the orfeomeGenes table */
{
char tmpGeneTbl[32], tmpMrnaTbl[32];
tblBldGetTmpName(tmpGeneTbl, sizeof(tmpGeneTbl), ORFEOME_GENES_TBL);
tblBldGetTmpName(tmpMrnaTbl, sizeof(tmpMrnaTbl), ORFEOME_MRNA_TBL);

gbVerbEnter(2, "loading %s", tmpGeneTbl);
tblBldGenePredFromPsl(conn, workDir, tmpMrnaTbl, tmpGeneTbl, stderr);
gbVerbLeave(2, "loading %s", tmpGeneTbl);
}

static void orfeomeDbLoad(char *db)
/* Load the database with the ORFeome tables. */
{
gbVerbEnter(1, "Loading ORFeome tables");
struct sqlConnection *conn = hAllocConn(db);

tblBldDropTables(conn, orfeomeTables, TBLBLD_TMP_TABLE);
createOrfeomeMrnaTbl(conn);
createOrfeomeGenesTbl(conn);
tblBldAtomicInstall(conn, orfeomeTables);
tblBldDropTables(conn, orfeomeTables, TBLBLD_OLD_TABLE);

hFreeConn(&conn);
gbVerbLeave(1, "Loading ORFeome tables");
}

static void orfeomeDropTables(char *db)
/* drop all ORFeome-related tables. */
{
struct sqlConnection *conn = hAllocConn(db);
gbVerbEnter(1, "droping ORFeome tables");
tblBldDropTables(conn, orfeomeTables, TBLBLD_REAL_TABLE|TBLBLD_TMP_TABLE|TBLBLD_OLD_TABLE);
hFreeConn(&conn);
gbVerbLeave(1, "droping ORFeome tables");
}

int main(int argc, char *argv[])
/* Process command line. */
{
setlinebuf(stdout);
setlinebuf(stderr);

optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage();
boolean drop = optionExists("drop");
workDir = optionVal("workdir", "work/load/orfeome");
gbVerbInit(optionInt("verbose", 0));
if (gbVerbose >= 5)
    sqlMonitorEnable(JKSQL_TRACE);
if (drop)
    {
    orfeomeDropTables(argv[1]);
    }
else
    {
    gbMakeDirs(workDir);
    orfeomeDbLoad(argv[1]);
    }
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
