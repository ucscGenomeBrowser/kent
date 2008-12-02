/* hgLsSnpPdbLoad - fetch data from LS-SNP PDB mysql server and load table for browser. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "lsSnpPdb.h"

static char const rcsid[] = "$Id: hgLsSnpPdbLoad.c,v 1.1 2008/12/02 01:37:02 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLsSnpPdbLoad - fetch data from LS-SNP PDB mysql server and\n"
  "                 load table for browser\n"
  "usage:\n"
  "   hgLsSnpPdbLoad lsSnpDb db table\n"
  "options:\n"
  "   -noLoad - don't load the table, create a file table.tab.\n"
  "             The db argument is ignored when specified.\n"
  "\n"
  "lsSnpDb can be either a local database name or profile:database,\n"
  "to use a profile in ~/.hg.conf to access a remote database.\n"
  );
}

static struct optionSpec options[] = {
    {"noLoad", OPTION_BOOLEAN},
    {NULL, 0},
};
static boolean noLoad = FALSE;

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
/* build lsSnpPdb tab file  */
{
struct sqlConnection *conn = sqlConnectProfile(lsSnpProf, lsSnpDb);
FILE *fh = mustOpen(tabFile, "w");
char *query =
    "SELECT pr.accession,ps.pdb_id,pstr.struct_type,ps.chain,SNP.name,ps.snp_position "
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

static void loadLsSnpPdb(char *tabFile, char *db, char *table)
/* build lsSnpPdb tab file */
{
struct sqlConnection * conn = sqlConnect(db);
sqlDropTable(conn, table);
char query[512];
safef(query, sizeof(query), createSql, table);
sqlUpdate(conn, query);
sqlLoadTabFile(conn, tabFile, table, 0);
sqlDisconnect(&conn);
unlink(tabFile);
}

static void hgLsSnpPdbLoad(char *lsSnpDb, char *db, char *table)
/* hgLsSnpPdbLoad - fetch data from LS-SNP PDB mysql server and load table for browser. */
{
/* check lsSnpDb for profile */
char *lsSnpProf = NULL;
char *sep = strchr(lsSnpDb, ':');
if (sep != NULL)
    {
    lsSnpProf = lsSnpDb;
    *sep = '\0';
    lsSnpDb = sep+1;
    }

char tabFile[PATH_LEN];
safef(tabFile, sizeof(tabFile), "%s.tab", table);
buildLsSnpPdb(lsSnpProf,lsSnpDb, tabFile);
if (!noLoad)
    loadLsSnpPdb(tabFile, db, table);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
noLoad = optionExists("noLoad");
hgLsSnpPdbLoad(argv[1], argv[2], argv[3]);
return 0;
}
