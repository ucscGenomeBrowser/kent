/* mgcDbLoad - create and load MGC tracks into the databases. */

#include "common.h"
#include "options.h"
#include "portable.h"
#include "mgcStatusTbl.h"
#include "linefile.h"
#include "genbank.h"
#include "psl.h"
#include "genePred.h"
#include "hgRelate.h"
#include "hdb.h"
#include "jksql.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include "hdb.h"

static char const rcsid[] = "$Id: mgcDbLoad.c,v 1.14 2006/02/23 05:22:05 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"drop", OPTION_BOOLEAN},
    {"allMgcTables", OPTION_BOOLEAN},
    {"workdir", OPTION_STRING},
    {"verbose", OPTION_INT},
    {NULL, 0}
};


/* Flags indicate what tables depending on browser (all or just full)  */
#define MGC_BOTH_TABLES 0x10  /* tables in both all and full-length */
#define MGC_FULL_TABLES 0x20  /* tables just in full-length browser */
#define MGC_MGC_TABLES  0x40  /* tables just in all MGC browder */

/* Flags indicate sets of tables: */
#define MGC_REAL_TABLES 0x1  /* base tables */
#define MGC_TMP_TABLES  0x2  /* _tmp tables */
#define MGC_OLD_TABLES  0x4  /* _old tables */

/* table names */
static char *MGC_STATUS_TBL = "mgcStatus";
static char *MGC_FULL_STATUS_TBL = "mgcFullStatus";
static char *MGC_FULL_MRNA_TBL = "mgcFullMrna";
static char *MGC_GENES_TBL = "mgcGenes";
static char *MGC_INCOMPLETE_MRNA_TBL = "mgcIncompleteMrna";
static char *MGC_FAILED_EST_TBL = "mgcFailedEst";
static char *MGC_PICKED_EST_TBL = "mgcPickedEst";
static char *MGC_UNPICKED_EST_TBL = "mgcUnpickedEst";

/* tmp versions of tables */
static char *MGC_STATUS_TMP = "mgcStatus_tmp";
static char *MGC_FULL_STATUS_TMP = "mgcFullStatus_tmp";
static char *MGC_FULL_MRNA_TMP = "mgcFullMrna_tmp";
static char *MGC_GENES_TMP = "mgcGenes_tmp";
static char *MGC_INCOMPLETE_MRNA_TMP = "mgcIncompleteMrna_tmp";
static char *MGC_FAILED_EST_TMP = "mgcFailedEst_tmp";
static char *MGC_PICKED_EST_TMP = "mgcPickedEst_tmp";
static char *MGC_UNPICKED_EST_TMP = "mgcUnpickedEst_tmp";

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


time_t getTablModTime(struct sqlConnection *conn, char *table)
/* get the modification date and time of a table, or 0 if doesn't exist */
{
time_t modTime = 0;
struct sqlResult *sr;
char **row;
char query[512];

safef(query, sizeof(query), "show table status like '%s'", table);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    /* column 11 == update_time */
    char *updateTime = row[11];
    char *src = updateTime, *dest = updateTime;

    /* format is 2003-03-18 18:14:43, convert to YYYYMMDDHHMMSS */
    while (*src != '\0')
        {
        if (isdigit(*src))
            *dest++ = *src;
        src++;
        }
    *dest = '\0';
    modTime = gbParseTimeStamp(updateTime);
    }

sqlFreeResult(&sr);
return modTime;
}

void dropTable(struct sqlConnection *conn, char *table, unsigned which)
/* Drop a table, and _tmp, _old, select by flags  */
{
char table2[64];
if (which & MGC_REAL_TABLES)
    sqlDropTable(conn, table);
if (which & MGC_TMP_TABLES)
    {
    safef(table2, sizeof(table2), "%s_tmp", table);
    sqlDropTable(conn, table2);
    }
if (which & MGC_OLD_TABLES)
    {
    safef(table2, sizeof(table2), "%s_old", table);
    sqlDropTable(conn, table2);
    }
}

void dropTables(struct sqlConnection *conn, unsigned browser, unsigned which)
/* drop MGC-related tables based on flags */
{
if (browser & MGC_BOTH_TABLES)
    {
    dropTable(conn, MGC_FULL_MRNA_TBL, which);
    dropTable(conn, MGC_GENES_TBL, which);
    }
if (browser & MGC_FULL_TABLES)
    {
    dropTable(conn, MGC_FULL_STATUS_TBL, which);
    }
if (browser & MGC_MGC_TABLES)
    {
    dropTable(conn, MGC_STATUS_TBL, which);
    dropTable(conn, MGC_INCOMPLETE_MRNA_TBL, which);
    dropTable(conn, MGC_FAILED_EST_TBL, which);
    dropTable(conn, MGC_PICKED_EST_TBL, which);
    dropTable(conn, MGC_UNPICKED_EST_TBL, which);
    }
}

void remakePslTable(struct sqlConnection *conn, char *table, char *insertTable)
/* rename a PSL table */
{
/* create with tName index and bin and if the specified table has bin.  This
 * is done so insert from the other table works. */
boolean useBin = (sqlFieldIndex(conn, insertTable, "bin") >= 0);
unsigned options = PSL_TNAMEIX | ((useBin ? PSL_WITH_BIN : 0));
char *sqlCmd = pslGetCreateSql(table, options, 0);
sqlRemakeTable(conn, table, sqlCmd);
freez(&sqlCmd);
}

char *loadMgcStatus(struct sqlConnection *conn, char *mgcStatusTab)
/* load the mgcStatus or mgcFullStatus tables, return name loaded */
{
struct lineFile* inLf;
FILE *outFh;
char tmpFile[PATH_LEN];
char *statusTblName = allMgcTables ? MGC_STATUS_TMP : MGC_FULL_STATUS_TMP;
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
return statusTblName;
}

void createMgcFullMrna(struct sqlConnection *conn, char *statusTblName)
/* create the mgcFullMrna table */
{
char sql[1024];
gbVerbEnter(2, "loading %s", MGC_FULL_MRNA_TMP);

remakePslTable(conn, MGC_FULL_MRNA_TMP, "all_mrna");

/* insert a join by accession of the all_mrna table and mgcStatus rows having
 * full-length state */
safef(sql, sizeof(sql),
      "INSERT INTO %s"
      "  SELECT all_mrna.* FROM all_mrna, %s"
      "    WHERE (all_mrna.qName = %s.acc)"
      "      AND (%s.state = %d)",
      MGC_FULL_MRNA_TMP, statusTblName, statusTblName,
      statusTblName, MGC_STATE_FULL_LENGTH);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", MGC_FULL_MRNA_TMP);
}

struct genePred* pslRowToGene(char **row)
/* Read CDS and PSL and convert to genePred with specified CDS string */
{
struct genbankCds cds;
struct genePred *genePred;
struct psl *psl = pslLoad(row+1);

/* this sets cds start/end to -1 on error, which results in no CDS */
if (!genbankCdsParse(row[0], &cds))
    {
    if (sameString(row[0], "n/a"))
        fprintf(stderr, "Warning: no CDS annotation for MGC mRNA %s\n",
                psl->qName);
    else 
        fprintf(stderr, "Warning: invalid CDS annotation for MGC mRNA %s: %s\n",
                psl->qName, row[0]);

    }

genePred = genePredFromPsl2(psl, genePredAllFlds, &cds, genePredStdInsertMergeSize);
pslFree(&psl);
return genePred;
}

void createMgcGenes(struct sqlConnection *conn)
/* create the mgcGenes table from the mgcFullMrna table */
{
char *sql;
char query[512], **row;
struct sqlResult *sr;
char tabFile[PATH_LEN];
FILE *tabFh;

gbVerbEnter(2, "loading %s", MGC_GENES_TMP);

/* create the tmp table */
sql = genePredGetCreateSql(MGC_GENES_TMP, genePredAllFlds, 0, hGetMinIndexLength());
sqlRemakeTable(conn, MGC_GENES_TMP, sql);
freez(&sql);

/* get CDS and PSL to generate genePred, put result in tab file for load */
safef(tabFile, sizeof(tabFile), "%s/%s", workDir, MGC_GENES_TMP);
tabFh = mustOpen(tabFile, "w");

/* go ahead and get gene even if no CDS annotation (query returns string
 * of "n/a" */
safef(query, sizeof(query),
      "SELECT cds.name,matches,misMatches,repMatches,nCount,qNumInsert,qBaseInsert,tNumInsert,tBaseInsert,strand,qName,qSize,qStart,qEnd,tName,tSize,tStart,tEnd,blockCount,blockSizes,qStarts,tStarts "
      "FROM cds,%s,gbCdnaInfo WHERE (%s.qName = gbCdnaInfo.acc) AND (gbCdnaInfo.cds = cds.id)",
      MGC_FULL_MRNA_TMP, MGC_FULL_MRNA_TMP);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *genePred = pslRowToGene(row);
    genePredTabOut(genePred, tabFh);
    genePredFree(&genePred);
    }
sqlFreeResult(&sr);

carefulClose(&tabFh);
sqlLoadTabFile(conn, tabFile, MGC_GENES_TMP, SQL_TAB_FILE_ON_SERVER);

gbVerbLeave(2, "loading %s", MGC_GENES_TMP);
}

void createMgcIncompleteMrna(struct sqlConnection *conn)
/* create the  table */
{
char sql[1024];
gbVerbEnter(2, "loading %s", MGC_INCOMPLETE_MRNA_TMP);

remakePslTable(conn, MGC_INCOMPLETE_MRNA_TMP, "all_mrna");

/* insert a join by accession of the all_mrna table and mgcStatus rows not
 * having full-length or in-progress status values.  Wanring: This relies on
 * order of enum */
safef(sql, sizeof(sql),
      "INSERT INTO %s"
      "  SELECT all_mrna.* FROM all_mrna, mgcStatus_tmp"
      "    WHERE (all_mrna.qName =  mgcStatus_tmp.acc)"
      "      AND (mgcStatus_tmp.state = %d)",
      MGC_INCOMPLETE_MRNA_TMP, MGC_STATE_PROBLEM);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", MGC_INCOMPLETE_MRNA_TMP);
}

void createMgcPickedEst(struct sqlConnection *conn)
/* create the mgcPickedEst table */
{
char sql[1024];
gbVerbEnter(2, "loading %s", MGC_PICKED_EST_TMP);

remakePslTable(conn, MGC_PICKED_EST_TMP, "all_est");

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
      MGC_PICKED_EST_TMP, MGC_STATE_PENDING);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", MGC_PICKED_EST_TMP);
}

void createMgcFailedEst(struct sqlConnection *conn)
/* create the mgcFailedEst table */
{
char sql[1024];

gbVerbEnter(2, "loading %s", MGC_FAILED_EST_TMP);

remakePslTable(conn, MGC_FAILED_EST_TMP, "all_est");

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
      MGC_FAILED_EST_TMP, MGC_STATE_PROBLEM);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", MGC_FAILED_EST_TMP);
}

void createMgcUnpickedEst(struct sqlConnection *conn)
/* create the mgcUnpickedEst table */
{
char sql[1024];

gbVerbEnter(2, "loading %s", MGC_UNPICKED_EST_TMP);

remakePslTable(conn, MGC_UNPICKED_EST_TMP, "all_est");

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
      MGC_UNPICKED_EST_TMP, MGC_STATE_UNPICKED);
sqlUpdate(conn, sql);
gbVerbLeave(2, "loading %s", MGC_UNPICKED_EST_TMP);
}

void addRename(struct sqlConnection *conn, struct dyString* sql, char *table)
/* Add a rename of one of the tables to the sql command */
{
if (sqlTableExists(conn, table))
    dyStringPrintf(sql, "%s TO %s_old, ", table, table);
dyStringPrintf(sql, "%s_tmp TO %s", table, table);
}

void mgcDbLoad(char *database, char *mgcStatusTabFile)
/* Load the database with the MGC tables. */
{
struct sqlConnection *conn;
struct dyString* sql = dyStringNew(1024);
char *statusTblName;

gbVerbEnter(1, "Loading MGC tables");
hSetDb(database);
conn = hAllocConn();

/* laod status table into database, and full entries into memory */
statusTblName = loadMgcStatus(conn, mgcStatusTabFile);

createMgcFullMrna(conn, statusTblName);
createMgcGenes(conn);
if (allMgcTables) 
    {
    createMgcIncompleteMrna(conn);
    createMgcPickedEst(conn);
    createMgcFailedEst(conn);
    createMgcUnpickedEst(conn);
    }

/* Get old tables out of the way (in case of failure) */
dropTables(conn, (MGC_BOTH_TABLES|MGC_FULL_TABLES|MGC_MGC_TABLES),
           MGC_OLD_TABLES);

/* Generate rename of tables, move real to old and tmp to real. */
dyStringAppend(sql, "RENAME TABLE ");
addRename(conn, sql, MGC_FULL_MRNA_TBL);
dyStringAppend(sql, ", ");
addRename(conn, sql, MGC_GENES_TBL);
if (allMgcTables)
    {
    dyStringAppend(sql, ", ");
    addRename(conn, sql, MGC_STATUS_TBL);
    dyStringAppend(sql, ", ");
    addRename(conn, sql, MGC_INCOMPLETE_MRNA_TBL);
    dyStringAppend(sql, ", ");
    addRename(conn, sql, MGC_FAILED_EST_TBL);
    dyStringAppend(sql, ", ");
    addRename(conn, sql, MGC_PICKED_EST_TBL);
    dyStringAppend(sql, ", ");
    addRename(conn, sql, MGC_UNPICKED_EST_TBL);
    }
else
    {
    dyStringAppend(sql, ", ");
    addRename(conn, sql, MGC_FULL_STATUS_TBL);
    }
sqlUpdate(conn, sql->string);
dyStringFree(&sql);

/*  Drop tables for other type of browser, in case switching */
if (allMgcTables)
    dropTables(conn, MGC_FULL_TABLES, MGC_REAL_TABLES);
else
    dropTables(conn, MGC_MGC_TABLES, MGC_REAL_TABLES);

/* Now get ride of old and do tmp as well, in case of switching browser
 * type */
dropTables(conn, (MGC_BOTH_TABLES|MGC_FULL_TABLES|MGC_MGC_TABLES),
           (MGC_TMP_TABLES|MGC_OLD_TABLES));

hFreeConn(&conn);
gbVerbLeave(1, "Loading MGC tables");
}

void mgcDropTables(char *database)
/* drop all MGC-related tables. */
{
struct sqlConnection *conn;
hSetDb(database);
conn = hAllocConn();
gbVerbEnter(1, "droping MGC tables");

dropTables(conn, (MGC_BOTH_TABLES|MGC_FULL_TABLES|MGC_MGC_TABLES),
           (MGC_REAL_TABLES|MGC_TMP_TABLES|MGC_OLD_TABLES));

hFreeConn(&conn);
gbVerbLeave(1, "droping MGC tables");
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *database;
char* mgcStatusTabFile;
boolean drop;

setlinebuf(stdout);
setlinebuf(stderr);

optionInit(&argc, argv, optionSpecs);
drop = optionExists("drop");
gbVerbInit(optionInt("verbose", 0));
if (gbVerbose >= 5)
    sqlMonitorEnable(JKSQL_TRACE);
if (drop)
    {
    if (argc != 2)
        usage();
    database = argv[1];
    mgcDropTables(database);
    }
else
    {
    if (argc != 3)
        usage();
    workDir = optionVal("workdir", "work/load/mgc");
    gbMakeDirs(workDir);
    allMgcTables = optionExists("allMgcTables");
    
    database = argv[1];
    mgcStatusTabFile = argv[2];
    hSetDb(database);

    mgcDbLoad(database, mgcStatusTabFile);
    }
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

