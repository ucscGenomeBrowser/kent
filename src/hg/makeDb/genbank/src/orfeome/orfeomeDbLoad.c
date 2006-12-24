/* orfeomeDbLoad - create and load ORFeome tracks into the database. */

#include "common.h"
#include "options.h"
#include "portable.h"
#include "linefile.h"
#include "genbank.h"
#include "psl.h"
#include "genePred.h"
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

static char const rcsid[] = "$Id: orfeomeDbLoad.c,v 1.1 2006/12/24 20:48:14 markd Exp $";

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
static char *ORFEOME_MRNA_TBL = "orfeomeMRna";

/* list of all tables */
static char *orfeomeTables[] =
{
    "orfeomeMRna",
    NULL
};

/* command line globals */
char *workDir;

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
struct orfeomeImageIds *allIds = orfeomeImageIdsLoadAll(orfeomeImageIdsFile);
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

static void addImageIdWhere(struct dyString *sql, struct orfeomeImageIds* imageIds)
/* add a where clause on imageIds being in the imageClone table */
{
struct orfeomeImageIds *ids;
dyStringAppend(sql, "(");
for (ids = imageIds; ids != NULL; ids = ids->next)
    {
    if (ids != imageIds)
        dyStringAppend(sql, " OR ");
    dyStringPrintf(sql, "(%d <= imageId and imageId <= %d)", ids->imageFirst, ids->imageLast);
    }
dyStringAppend(sql, ")");
}

static void createOrfeomeMrna(struct sqlConnection *conn, struct orfeomeImageIds* imageIds)
/* create the orfeomeMrna table */
{
struct dyString *sql = dyStringNew(1024);
char tmpTbl[32];
tblBldGetTmpName(tmpTbl, sizeof(tmpTbl), ORFEOME_MRNA_TBL);
gbVerbEnter(2, "loading %s", tmpTbl);

tblBldRemakePslTable(conn, tmpTbl, "all_mrna");

/* Build insert of a join from he all_mrna table having image ids
 * in the specified ranges. */
dyStringPrintf(sql,
      "INSERT INTO %s"
      "  SELECT all_mrna.* FROM all_mrna,imageClone"
      "    WHERE (all_mrna.qName = imageClone.acc)"
      "      AND ",
      tmpTbl);
addImageIdWhere(sql, imageIds);

sqlUpdate(conn, sql->string);
dyStringFree(&sql);
gbVerbLeave(2, "loading %s", tmpTbl);
}

static void buildOrfeomeTables(struct sqlConnection *conn,
                               struct orfeomeImageIds* imageIds)
/* build ORFeome tables with _tmp names */
{
createOrfeomeMrna(conn, imageIds);
}

static void orfeomeDbLoad(char *db)
/* Load the database with the ORFeome tables. */
{
gbVerbEnter(1, "Loading ORFeome tables");
hSetDb(db);
struct sqlConnection *conn = hAllocConn();
struct orfeomeImageIds* imageIds = loadOrfeomeImageIds(db);

tblBldDropTables(conn, orfeomeTables, TBLBLD_TMP_TABLE);
buildOrfeomeTables(conn, imageIds);
tblBldAtomicInstall(conn, orfeomeTables);

orfeomeImageIdsFreeList(&imageIds);
hFreeConn(&conn);
gbVerbLeave(1, "Loading ORFeome tables");
}

static void orfeomeDropTables(char *db)
/* drop all ORFeome-related tables. */
{
hSetDb(db);
struct sqlConnection *conn = hAllocConn();
gbVerbEnter(1, "droping ORFeome tables");

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
gbVerbInit(optionInt("verbose", 0));
if (gbVerbose >= 5)
    sqlMonitorEnable(JKSQL_TRACE);
if (drop)
    {
    orfeomeDropTables(argv[1]);
    }
else
    {
    if (argc != 3)
        usage();
    workDir = optionVal("workdir", "work/load/orfeome");
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
