/* hgLsSnpPdbLoad - fetch data from LS-SNP PDB mysql server and load table for browser. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "lsSnpPdb.h"
#include "linefile.h"
#include "portable.h"


void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("Error: %s:\n"
  "hgLsSnpPdbLoad - fetch data from LS-SNP/PDB mysql server or\n"
  "                 load an lsSnpPdb format table or file\n"
  "usage:\n"
  "   hgLsSnpPdbLoad fetch lsSnpDb tabOut\n"
  "   hgLsSnpPdbLoad load db table tabIn\n"
  "\n"
  "lsSnpDb can be either a local database name or profile:database,\n"
  "to use a profile in ~/.hg.conf to access a remote database.\n",
  msg);
}

static struct optionSpec options[] = {
    {NULL, 0},
};

static char *createSql =
    "CREATE TABLE %s ("
    "    protId varchar(255) not null,"
    "    pdbId varchar(255) not null,"
    "    structType enum(\"XRay\", \"NMR\") not null,"
    "    chain char(1) not null,"
    "    snpId varchar(255) not null,"
    "    snpPdbLoc int not null,"
    "    index(protId),"
    "    index(pdbId),"
    "    index(snpId));";


static void buildLsSnpPdb(char *lsSnpProf, char *lsSnpDb, char *tabFile)
/* fetch LSSNP data and create an lsSnpPdb format tab file  */
{
struct sqlConnection *conn = sqlConnectProfile(lsSnpProf, lsSnpDb);
FILE *fh = mustOpen(tabFile, "w");
char *query =
    NOSQLINJ "SELECT distinct pr.accession,ps.pdb_id,pstr.struct_type,ps.chain,SNP.name,ps.snp_position "
    "FROM Protein pr,PDB_SNP ps, SNP, PDB_Structure pstr "
    "WHERE (ps.snp_id = SNP.snp_id) AND (pr.prot_id = ps.prot_id) "
    "AND (pstr.pdb_id = ps.pdb_id)";
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
struct lsSnpPdb rec;
while ((row = sqlNextRow(sr)) != NULL)
    {
    // translate struct_type
    char *hold = row[2];
    if (sameString(row[2], "X-Ray"))
        row[2] = "XRay";
    else if (sameString(row[2], "NMR"))
        row[2] = "NMR";
    else
        errAbort("unknown struct_type field value: \"%s\"", row[2]);
    lsSnpPdbStaticLoad(row, &rec);
    lsSnpPdbTabOut(&rec, fh);
    row[2] = hold;
    }
sqlFreeResult(&sr);
carefulClose(&fh);
sqlDisconnect(&conn);
}

static char *parseLsSnpDb(char **lsSnpDbPtr)
/* parse lsSnpDb spec, updating ptr and returning profile or null */
{
char *lsSnpProf = NULL;
char *sep = strchr(*lsSnpDbPtr, ':');
if (sep != NULL)
    {
    lsSnpProf = *lsSnpDbPtr;
    *sep = '\0';
    *lsSnpDbPtr = sep+1;
    }
return lsSnpProf;
}

static void fetchLsSnpPdb(char *lsSnpDb, char *tabOut)
/* fetch data from LS-SNP/PDB mysql server */
{
char *lsSnpProf = parseLsSnpDb(&lsSnpDb);
buildLsSnpPdb(lsSnpProf, lsSnpDb, tabOut);
}

/* get a temporary copy of the tab input file, possibly decompressing it */
static void getTmpTabFile(char *inFile, char *tmpFileName, int tmpFileNameSize)
{
char *tmpDir = getenv("TMPDIR");
if (tmpDir == NULL)
    tmpDir = "/var/tmp";
safef(tmpFileName, tmpFileNameSize, "%s/lsSnpPdb.%s.%d.tab", tmpDir, getHost(), getpid());
struct lineFile *inLf = lineFileOpen(inFile, TRUE);
FILE *outFh = mustOpen(tmpFileName, "w");
char *line;
while (lineFileNext(inLf, &line, NULL))
    {
    fputs(line, outFh);
    fputc('\n', outFh);
    }
carefulClose(&outFh);
lineFileClose(&inLf);
}

static void loadLsSnpPdb(char *db, char *table, char *inFile)
/* load lsSnpPdb table.  A temporary table is created and then
 * an atomic rename is done once it is loaded. */
{
char tmpTabFile[PATH_LEN];
getTmpTabFile(inFile, tmpTabFile, sizeof(tmpTabFile));

char newTbl[256], oldTbl[256], query[1024];
safef(newTbl, sizeof(newTbl), "%s_new", table);
safef(oldTbl, sizeof(oldTbl), "%s_old", table);

struct sqlConnection * conn = sqlConnect(db);

// clean up droppings from previous runs
sqlDropTable(conn, newTbl);
sqlDropTable(conn, oldTbl);

// load into tmp table
sqlSafef(query, sizeof(query), createSql, newTbl);
sqlUpdate(conn, query);
sqlLoadTabFile(conn, tmpTabFile, newTbl, 0);

// rename into place
if (sqlTableExists(conn, table))
    sqlSafef(query, sizeof(query), "rename table %s to %s, %s to %s",
          table, oldTbl, newTbl, table);
else
    sqlSafef(query, sizeof(query), "rename table %s to %s",
          newTbl, table);
sqlUpdate(conn, query);

sqlDropTable(conn, oldTbl);
sqlDisconnect(&conn);
unlink(tmpTabFile);  // ignore failures
}

int main(int argc, char *argv[])
/* Process command line. */
{
static char *wrongArgsMsg = "wrong number of arguments";
optionInit(&argc, argv, options);
if (argc < 2)
    usage(wrongArgsMsg);
if (sameString(argv[1], "fetch"))
    {
    if (argc != 4)
        usage(wrongArgsMsg);
    fetchLsSnpPdb(argv[2], argv[3]);
    }
else if (sameString(argv[1], "load"))
    {
    if (argc != 5)
        usage(wrongArgsMsg);
    loadLsSnpPdb(argv[2], argv[3], argv[4]);
    }
else
    usage("invalid action");

return 0;
}
