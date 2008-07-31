/* mgcDbLoad - create and load MGC tracks into the database. */

#include "common.h"
#include "options.h"
#include "portable.h"
#include "mgcStatusTbl.h"
#include "linefile.h"
#include "psl.h"
#include "genePred.h"
#include "hgRelate.h"
#include "hdb.h"
#include "jksql.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include "gbSql.h"
#include "hdb.h"

static char const rcsid[] = "$Id: mgcDbLoad.c,v 1.18.76.1 2008/07/31 02:24:35 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"drop", OPTION_BOOLEAN},
    {"allMgcTables", OPTION_BOOLEAN},
    {"workdir", OPTION_STRING},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

/* table names */
static char *MGC_STATUS_TBL = "mgcStatus";
static char *MGC_FULL_STATUS_TBL = "mgcFullStatus";
static char *MGC_FULL_MRNA_TBL = "mgcFullMrna";
static char *MGC_GENES_TBL = "mgcGenes";
static char *MGC_INCOMPLETE_MRNA_TBL = "mgcIncompleteMrna";
static char *MGC_FAILED_EST_TBL = "mgcFailedEst";
static char *MGC_PICKED_EST_TBL = "mgcPickedEst";
static char *MGC_UNPICKED_EST_TBL = "mgcUnpickedEst";

/* table on databases with just full-length MGCs */
static char *mgcFullTables[] =
{
    "mgcFullStatus",
    "mgcFullMrna",
    "mgcGenes",
    NULL
};

/* tables only on the full browser */
static char *mgcFullOnlyTables[] =
{
    "mgcFullStatus",
    NULL
};

/* table on databases with all MGC tracks */
static char *mgcAllTables[] =
{
    "mgcStatus",
    "mgcFullMrna",
    "mgcGenes",
    "mgcIncompleteMrna",
    "mgcFailedEst",
    "mgcPickedEst",
    "mgcUnpickedEst",
    NULL
};

/* table only on browser with all MGC tracks */
static char *mgcAllOnlyTables[] =
{
    "mgcStatus",
    "mgcIncompleteMrna",
    "mgcFailedEst",
    "mgcPickedEst",
    "mgcUnpickedEst",
    NULL
};

/* command line globals */
char *workDir;
boolean allMgcTables;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mgcDbLoad - load MGC tracks into the databases.  This loads the tab file\n"
  "created by mgcImport into the mgcStatus table. It creates mgcFullMrna\n"
  "table with a join and optionally creates the other mgc alignment tables in\n"
  "a similar manner.  The tables are created with at _tmp extension and renamed\n"
  "into place at the end.\n"
  "\n"
  "Usage:\n"
  "   mgcDbLoad [options] database mgcStatusTab\n"
  "   mgcDbLoad -drop database\n"
  "\n"
  "   o database - database to load\n"
  "   o mgcStatusTab - status table to use, if all tables are being loaded\n"
  "     this must be the status for all clones; if only the full-length\n"
  "     table is being load, this can have only the full-length clones.\n"
  "     File maybe compressed.\n"
  "\n"
  "Options:\n"
  "    -allMgcTables - create all mgc, not just full length. \n"
  "    -workdir=work/load/mgc - Temporary directory for load files.\n"
  "    -verbose=n - enable verbose output\n"
  "              n >= 1 - basic information abort each step\n"
  "              n >= 2 - more details\n"
  "              n >= 5 - SQL queries\n"
  "\n"
  "This program may also be called to drop all of the MGC tables:\n"
  "   mgcDbLoad -drop database\n"
  "\n"
  );
}


void loadMgcStatus(struct sqlConnection *conn, char *mgcStatusTab, char *statusTblName)
/* load the mgcStatus or mgcFullStatus tables, return name loaded */
{
struct lineFile* inLf;
FILE *outFh;
char tmpFile[PATH_LEN];
gbVerbEnter(2, "loading %s", statusTblName);

/* uncompress to tmp file */
safef(tmpFile, sizeof(tmpFile), "%s/mgcStatus.%s.%d.tmp", workDir,
      getHost(), getpid());
inLf = gzLineFileOpen(mgcStatusTab);
outFh = gzMustOpen(tmpFile, "w");

while (mgcStatusTblCopyRow(inLf, outFh))
    continue;

gzClose(&outFh);
gzLineFileClose(&inLf);

mgcStatusTblCreate(conn, statusTblName);

sqlLoadTabFile(conn, tmpFile, statusTblName, SQL_TAB_FILE_ON_SERVER);
unlink(tmpFile);

gbVerbLeave(2, "loading %s", statusTblName);
}

void createMgcFullMrna(struct sqlConnection *conn, char *statusTblName)
/* create the mgcFullMrna table */
{
char sql[1024];
char tmpTbl[32];
tblBldGetTmpName(tmpTbl, sizeof(tmpTbl), MGC_FULL_MRNA_TBL);
gbVerbEnter(2, "loading %s", tmpTbl);

tblBldRemakePslTable(conn, tmpTbl, "all_mrna");

/* insert a join by accession of the all_mrna table and mgcStatus rows having
 * full-length state */
safef(sql, sizeof(sql),
      "INSERT INTO %s"
      "  SELECT all_mrna.* FROM all_mrna, %s"
      "    WHERE (all_mrna.qName = %s.acc)"
      "      AND (%s.state = %d)",
      tmpTbl, statusTblName, statusTblName,
      statusTblName, MGC_STATE_FULL_LENGTH);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", tmpTbl);
}

void createMgcGenes(struct sqlConnection *conn)
/* create the mgcGenes table from the mgcFullMrna table */
{
char tmpGeneTbl[32], tmpMrnaTbl[32];
tblBldGetTmpName(tmpGeneTbl, sizeof(tmpGeneTbl), MGC_GENES_TBL);
tblBldGetTmpName(tmpMrnaTbl, sizeof(tmpMrnaTbl), MGC_FULL_MRNA_TBL);

gbVerbEnter(2, "loading %s", tmpGeneTbl);
tblBldGenePredFromPsl(conn, workDir, tmpMrnaTbl, tmpGeneTbl, stderr);
gbVerbLeave(2, "loading %s", tmpGeneTbl);
}

void createMgcIncompleteMrna(struct sqlConnection *conn)
/* create the  table */
{
char sql[1024];
char tmpTbl[32];
tblBldGetTmpName(tmpTbl, sizeof(tmpTbl), MGC_INCOMPLETE_MRNA_TBL);
gbVerbEnter(2, "loading %s", tmpTbl);

tblBldRemakePslTable(conn, tmpTbl, "all_mrna");

/* insert a join by accession of the all_mrna table and mgcStatus rows not
 * having full-length or in-progress status values.  Wanring: This relies on
 * order of enum */
safef(sql, sizeof(sql),
      "INSERT INTO %s"
      "  SELECT all_mrna.* FROM all_mrna, mgcStatus_tmp"
      "    WHERE (all_mrna.qName =  mgcStatus_tmp.acc)"
      "      AND (mgcStatus_tmp.state = %d)",
      tmpTbl, MGC_STATE_PROBLEM);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", tmpTbl);
}

void createMgcPickedEst(struct sqlConnection *conn)
/* create the mgcPickedEst table */
{
char sql[1024];
char tmpTbl[32];
tblBldGetTmpName(tmpTbl, sizeof(tmpTbl), MGC_PICKED_EST_TBL);
gbVerbEnter(2, "loading %s", tmpTbl);

tblBldRemakePslTable(conn, tmpTbl, "all_est");

/* insert a join by image id of the all_est table and mgcStatus rows having
 * inprogress status values, only getting 5' ESTs.  No acc in mgcStatus
 * indicates a imageClone id that has not be sequenced.
 */
safef(sql, sizeof(sql),
      "INSERT INTO %s"
      "  SELECT all_est.* FROM all_est, mgcStatus_tmp, imageClone"
      "    WHERE (all_est.qName = imageClone.acc)"
      "      AND (imageClone.direction = '5')"
      "      AND (mgcStatus_tmp.imageId = imageClone.imageId)"
      "      AND (mgcStatus_tmp.acc = '')"
      "      AND (mgcStatus_tmp.state = %d)",
      tmpTbl, MGC_STATE_PENDING);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", tmpTbl);
}

void createMgcFailedEst(struct sqlConnection *conn)
/* create the mgcFailedEst table */
{
char sql[1024];
char tmpTbl[32];
tblBldGetTmpName(tmpTbl, sizeof(tmpTbl), MGC_FAILED_EST_TBL);

gbVerbEnter(2, "loading %s", tmpTbl);

tblBldRemakePslTable(conn, tmpTbl, "all_est");

/* insert a join by image id of the all_est table and mgcStatus rows having
 * failed status values, only getting 5' ESTs.  No acc in mgcStatus indicates
 * a imageClone id that has not be sequenced. */
safef(sql, sizeof(sql),
      "INSERT INTO %s"
      "  SELECT all_est.* FROM all_est,  mgcStatus_tmp, imageClone"
      "    WHERE (all_est.qName = imageClone.acc)"
      "      AND (imageClone.direction = '5')"
      "      AND (mgcStatus_tmp.imageId = imageClone.imageId)"
      "      AND (mgcStatus_tmp.acc = '')"
      "      AND (mgcStatus_tmp.state = %d)",
      tmpTbl, MGC_STATE_PROBLEM);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", tmpTbl);
}

void createMgcUnpickedEst(struct sqlConnection *conn)
/* create the mgcUnpickedEst table */
{
char sql[1024];
char tmpTbl[32];
tblBldGetTmpName(tmpTbl, sizeof(tmpTbl), MGC_UNPICKED_EST_TBL);

gbVerbEnter(2, "loading %s", tmpTbl);

tblBldRemakePslTable(conn, tmpTbl, "all_est");

/* insert a join by accession of the all_mrna table and mgcStatus rows not
 * full-length or inprogress status values.  No acc in mgcStatus indicates a
 * imageClone id that has not be sequenced. */
safef(sql, sizeof(sql),
      "INSERT INTO %s"
      "  SELECT all_est.* FROM all_est,  mgcStatus_tmp, imageClone"
      "    WHERE (all_est.qName = imageClone.acc)"
      "      AND (imageClone.direction = '5')"
      "      AND (mgcStatus_tmp.imageId = imageClone.imageId)"
      "      AND (mgcStatus_tmp.acc = '')"
      "      AND (mgcStatus_tmp.state = %d)",
      tmpTbl, MGC_STATE_UNPICKED);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", tmpTbl);
}

void buildMgcTbls(struct sqlConnection *conn, char *mgcStatusTabFile)
/* build the tables and load with _tmp name */
{
char statusTblName[32];
tblBldGetTmpName(statusTblName, sizeof(statusTblName),
                 (allMgcTables ? MGC_STATUS_TBL : MGC_FULL_STATUS_TBL));
tblBldDropTables(conn, ((allMgcTables) ? mgcAllOnlyTables : mgcFullOnlyTables), TBLBLD_TMP_TABLE);
loadMgcStatus(conn, mgcStatusTabFile, statusTblName);
createMgcFullMrna(conn, statusTblName);
createMgcGenes(conn);
if (allMgcTables) 
    {
    createMgcIncompleteMrna(conn);
    createMgcPickedEst(conn);
    createMgcFailedEst(conn);
    createMgcUnpickedEst(conn);
    }
}

void installMgcTbls(struct sqlConnection *conn)
/* install the tables in an atomic manner */
{
if (allMgcTables)
    tblBldAtomicInstall(conn, mgcAllTables);
else
    tblBldAtomicInstall(conn, mgcFullTables);
}

void mgcDbLoad(char *database, char *mgcStatusTabFile)
/* Load the database with the MGC tables. */
{
gbVerbEnter(1, "Loading MGC tables");
struct sqlConnection *conn = hAllocConn(database);
buildMgcTbls(conn, mgcStatusTabFile);
installMgcTbls(conn);

/*  Drop tables only on *OTHER* type of browser, in case switching */
tblBldDropTables(conn, ((allMgcTables) ? mgcFullOnlyTables : mgcAllOnlyTables), TBLBLD_REAL_TABLE);

/* Now get ride of old and do tmp as well, in case of switching browser
 * type */
tblBldDropTables(conn, mgcFullTables, TBLBLD_TMP_TABLE|TBLBLD_OLD_TABLE);
tblBldDropTables(conn, mgcAllTables, TBLBLD_TMP_TABLE|TBLBLD_OLD_TABLE);

hFreeConn(&conn);
gbVerbLeave(1, "Loading MGC tables");
}

void mgcDropTables(char *database)
/* drop all MGC-related tables. */
{
struct sqlConnection *conn = hAllocConn(database);
gbVerbEnter(1, "droping MGC tables");

tblBldDropTables(conn, mgcFullTables, TBLBLD_REAL_TABLE|TBLBLD_TMP_TABLE|TBLBLD_OLD_TABLE);
tblBldDropTables(conn, mgcAllTables, TBLBLD_REAL_TABLE|TBLBLD_TMP_TABLE|TBLBLD_OLD_TABLE);

hFreeConn(&conn);
gbVerbLeave(1, "droping MGC tables");
}

int main(int argc, char *argv[])
/* Process command line. */
{
setlinebuf(stdout);
setlinebuf(stderr);

optionInit(&argc, argv, optionSpecs);
boolean drop = optionExists("drop");
gbVerbInit(optionInt("verbose", 0));
if (gbVerbose >= 5)
    sqlMonitorEnable(JKSQL_TRACE);
if (drop)
    {
    if (argc != 2)
        usage();
    mgcDropTables(argv[1]);
    }
else
    {
    if (argc != 3)
        usage();
    workDir = optionVal("workdir", "work/load/mgc");
    gbMakeDirs(workDir);
    allMgcTables = optionExists("allMgcTables");
    mgcDbLoad(argv[1], argv[2]);
    }
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

