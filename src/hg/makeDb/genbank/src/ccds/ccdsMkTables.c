/* ccdsMkTables - create tables for hg db from imported CCDS database */

#include "common.h"
#include "options.h"
#include "genePred.h"
#include "jksql.h"
#include "hdb.h"
#include "ccdsInfo.h"
#include "linefile.h"
#include "verbose.h"
#include "ccdsLocationsJoin.h"
#include "ccdsCommon.h"

static char const rcsid[] = "$Id: ccdsMkTables.c,v 1.2.4.2 2005/07/04 04:38:49 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"loadDb", OPTION_BOOLEAN},
    {"keep", OPTION_BOOLEAN},
    {NULL, 0}
};
boolean keep = FALSE;    /* keep tab files after load */
boolean loadDb = FALSE;  /* load database */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ccdsMkTables - create tables for hg db from imported CCDS database\n"
  "\n"
  "Usage:\n"
  "   ccdsMkTables [options] ccdsDb hgDb ccdsInfoOut ccdsGeneOut\n"
  "\n"
  "ccdsDb is the database created by ccdsImport, hgDb is the targeted\n"
  "genome database, required even if tables are not being loaded.\n"
  "If -loadDb is specified, ccdsInfoOut and ccdsGeneOut are tables to load\n"
  "with the cddsInfo and genePred data.  If -loadDb is not specified, they\n"
  "are files for the data.\n"
  "\n"
  "Options:\n"
  "  -loadDb - load tables into hgdb\n"
  "  -keep - keep tab file used to load database\n"
  "  -verbose=n\n"
  );
}

struct genomeInfo
/* infomation about the specific genome needed for selects */
{
    int ncbiBuild;
    int ncbiBuildVersion;
    int taxonId;
};

struct genomeInfo getBuildTaxon(char *hgDb)
/* translated the target databases to ncbi build and taxon */
{
struct genomeInfo genomeInfo;
ZeroVar(&genomeInfo);
if (sameString(hgDb, "hg17"))
    {
    genomeInfo.ncbiBuild = 35;
    genomeInfo.ncbiBuildVersion = 1;
    genomeInfo.taxonId = 9606;
    }
else
    errAbort("don't know how to load ccds into %s, supported databases: hg17",
             hgDb);
return genomeInfo;
}

/* Select to get data for ccdsInfo table.. Format build, buildVersion twice,
 * and taxon id. */
char *ccdsInfoSelect = 
    "SELECT "
    "CcdsUids.ccds_uid, GroupVersions.ccds_version, "
    "Organizations.name, "
    "Accessions.nuc_acc, Accessions.nuc_version, "
    "Accessions.prot_acc, Accessions.prot_version "
    "FROM "
    "Accessions, "
    "Accessions_GroupVersions, "
    "Groups, "
    "GroupVersions, "
    "CcdsUids, "
    "Organizations, "
    "CcdsStatusVals "
    "WHERE "
    "(Accessions.accession_uid = Accessions_GroupVersions.accession_uid) "
    "AND (Accessions_GroupVersions.group_version_uid = GroupVersions.group_version_uid) "
    "AND (GroupVersions.ncbi_build_number = %d)"
    "AND (GroupVersions.first_ncbi_build_version <= %d)"
    "AND (%d <= GroupVersions.last_ncbi_build_version)"
    "AND (Groups.group_uid = GroupVersions.group_uid) "
    "AND (Groups.tax_id = %d) "
    "AND (CcdsUids.group_uid = GroupVersions.group_uid) "
    "AND (Organizations.organization_uid = Accessions.organization_uid) "
    "AND (Accessions.alive = 1) "
    "AND (GroupVersions.was_public = 1) "
    "AND (Accessions_GroupVersions.ccds_status_val_uid = CcdsStatusVals.ccds_status_val_uid) "
    "AND ((CcdsStatusVals.ccds_status = \"accepted\") "
    "      OR (CcdsStatusVals.ccds_status = \"public\")) ";

/* FIXME: GroupVersions.was_public test was added bcause of getting a few hinxton
 * accessions in CCDSs with no refseqs.  However, I don't know why this is needed */

void processCcdsInfoRow(char **row, FILE *outFh)
/* process a row from ccdsInfoSelect */
{
struct ccdsInfo ci;
ZeroVar(&ci);
safef(ci.ccds, sizeof(ci.ccds), "CCDS%s.%s", row[0], row[1]);
if (sameString(row[2], "NCBI"))
    {
    /* NCBI has separate version numbers */
    ci.srcDb = ccdsInfoNcbi;
    safef(ci.mrnaAcc, sizeof(ci.mrnaAcc), "%s.%s", row[3], row[4]);
    safef(ci.protAcc, sizeof(ci.protAcc), "%s.%s", row[5], row[6]);
    }
else
    {
    ci.srcDb = (startsWith("OTT", ci.mrnaAcc) ? ccdsInfoVega : ccdsInfoEnsembl);
    safef(ci.mrnaAcc, sizeof(ci.mrnaAcc), "%s", row[3]);
    safef(ci.protAcc, sizeof(ci.protAcc), "%s", row[5]);
    }
ccdsInfoTabOut(&ci, outFh);
}

void createCcdsInfo(struct sqlConnection *conn, char *ccdsInfoFile,
                    struct genomeInfo *genomeInfo)
/* create ccdsInfo table file */
{
struct sqlResult *sr;
FILE *outFh;
char **row;
char query[4096];

safef(query, sizeof(query), ccdsInfoSelect,
      genomeInfo->ncbiBuild, genomeInfo->ncbiBuildVersion,
      genomeInfo->ncbiBuildVersion, genomeInfo->taxonId);
sr = sqlGetResult(conn, query);
outFh = mustOpen(ccdsInfoFile, "w");
while ((row = sqlNextRow(sr)) != NULL)
    processCcdsInfoRow(row, outFh);

carefulClose(&outFh);
sqlFreeResult(&sr);
}

/* Select to get data for ccdsGene table. Format build, buildVersion twice,
 * and taxon id. */
char *ccdsLocationsSelect = 
    "SELECT  "
    "CcdsUids.ccds_uid, GroupVersions.ccds_version, "
    "Locations_GroupVersions.chromosome, "
    "Groups.orientation, "
    "Locations.chr_start, "
    "Locations.chr_stop "

    "FROM "
    "Locations_GroupVersions, "
    "Locations, "
    "GroupVersions, "
    "Groups, "
    "CcdsUids, "
    "CcdsStatusVals "

    "WHERE "
    "(Locations.location_uid = Locations_GroupVersions.location_uid) "
    "AND (Locations_GroupVersions.group_version_uid = GroupVersions.group_version_uid) "
    "AND (GroupVersions.ncbi_build_number = %d)"
    "AND (GroupVersions.first_ncbi_build_version <= %d)"
    "AND (%d <= GroupVersions.last_ncbi_build_version)"
    "AND (Groups.group_uid = GroupVersions.group_uid) "
    "AND (Groups.tax_id = %d) "
    "AND (Groups.current_version = GroupVersions.version) "
    "AND (CcdsUids.group_uid = GroupVersions.group_uid) "
    "AND (GroupVersions.ccds_status_val_uid = CcdsStatusVals.ccds_status_val_uid) "
    "AND ((CcdsStatusVals.ccds_status = \"accepted\") "
    "     OR (CcdsStatusVals.ccds_status = \"public\")) ";

int ccdsLocationsJoinCmp(const void *va, const void *vb)
/* compare two locations by ccds id, chrom, and start */
{
const struct ccdsLocationsJoin *a = *((struct ccdsLocationsJoin **)va);
const struct ccdsLocationsJoin *b = *((struct ccdsLocationsJoin **)vb);
int dif = a->ccds_uid - b->ccds_uid;
if (dif == 0)
    dif = a->chrom[0] - b->chrom[0];
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

void locationProcessRow(char **row, struct ccdsLocationsJoin **locsList)
/* proces a row from the locations query and add to the list.  If the
 * chromsome is XY, split it into two entries */
{
struct ccdsLocationsJoin *loc = ccdsLocationsJoinLoad(row);
if (sameString(loc->chrom, "XY"))
    {
    /* no dynamic data, so can just copy */
    struct ccdsLocationsJoin *locY;
    AllocVar(locY);
    memcpy(locY, loc, sizeof(struct ccdsLocationsJoin));
    strcpy(loc->chrom, "X");
    strcpy(locY->chrom, "Y");
    slSafeAddHead(locsList, locY);
    }

slSafeAddHead(locsList, loc);
}

struct ccdsLocationsJoin *loadLocations(struct sqlConnection *conn,
                                        struct genomeInfo *genomeInfo)
/* load all exon locations into a list, sort by ccds id, and then
 * chrom and start */
{
struct sqlResult *sr;
char **row;
struct ccdsLocationsJoin *locs = NULL;
char query[4096];

safef(query, sizeof(query), ccdsLocationsSelect,
      genomeInfo->ncbiBuild, genomeInfo->ncbiBuildVersion,
      genomeInfo->ncbiBuildVersion, genomeInfo->taxonId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    locationProcessRow(row, &locs);

sqlFreeResult(&sr);
slSort(&locs, ccdsLocationsJoinCmp);
return locs;
}

struct ccdsLocationsJoin *popNextCcdsLocs(struct ccdsLocationsJoin **locsList)
/* pop the locations for the next ccds and chrom in the list. Chrom is checked
 * to handle X/Y PAR */
{
struct ccdsLocationsJoin *ccdsLocs = slPopHead(locsList);

if (ccdsLocs == NULL)
    return NULL; /* no more */

while ((*locsList != NULL) && ((*locsList)->ccds_uid == ccdsLocs->ccds_uid)
       && ((*locsList)->chrom[0] == ccdsLocs->chrom[0]))
    slSafeAddHead(&ccdsLocs, slPopHead(locsList));
slReverse(&ccdsLocs);
return ccdsLocs;
}

struct genePred *nextCcdsGenePred(struct ccdsLocationsJoin **locsList)
/* convert the next ccds/chrom in a list of locations to a genePred */
{
struct ccdsLocationsJoin *locs = popNextCcdsLocs(locsList);
struct ccdsLocationsJoin *loc, *lastLoc;
struct genePred *gp;
char buf[32];
int numExons, iExon, iGene;
if (locs == NULL)
    return NULL;  /* no more */

AllocVar(gp);
gp->optFields = genePredAllFlds;
numExons = slCount(locs);
lastLoc = slElementFromIx(locs, numExons-1);

safef(buf, sizeof(buf), "CCDS%d.%d", locs->ccds_uid, locs->lastest_version);
gp->name = cloneString(buf);
safef(buf, sizeof(buf), "chr%s", locs->chrom);
gp->chrom = cloneString(buf);
gp->strand[0] = locs->strand[0];
gp->txStart = locs->start;
gp->txEnd = lastLoc->stop+1;
gp->cdsStart = locs->start;
gp->cdsEnd = lastLoc->stop+1;
gp->exonCount = numExons;

/* fill in exon blocks */
gp->exonStarts = needMem(numExons*sizeof(unsigned));
gp->exonEnds = needMem(numExons*sizeof(unsigned));
for (loc = locs, iExon = 0; loc != NULL; loc = loc->next, iExon++)
    {
    gp->exonStarts[iExon] = loc->start;
    gp->exonEnds[iExon] = loc->stop+1;
    }
gp->id = 0;
gp->name2 = cloneString("");
gp->cdsStartStat = cdsComplete;
gp->cdsEndStat = cdsComplete;

/* fill in frames */
gp->exonFrames = needMem(numExons*sizeof(int));
iGene = 0;
if (gp->strand[0] == '+')
    {
    for (iExon = 0; iExon < numExons; iExon++)
        {
        gp->exonFrames[iExon] = iGene % 3;
        iGene += gp->exonEnds[iExon] - gp->exonStarts[iExon];
        }
    }
else
    {
    for (iExon = numExons-1; iExon >= 0; iExon--)
        {
        gp->exonFrames[iExon] = iGene % 3;
        iGene += gp->exonEnds[iExon] - gp->exonStarts[iExon];
        }
    }
ccdsLocationsJoinFreeList(&locs);
return gp;
}

void createCcdsGene(struct sqlConnection *conn, char *ccdsGeneFile,
                    struct genomeInfo *genomeInfo)
/* create the ccdsGene tab file from the ccds database */
{
struct ccdsLocationsJoin *locs = loadLocations(conn, genomeInfo);
struct genePred *genes = NULL, *gp;
FILE *genesFh;

while ((gp = nextCcdsGenePred(&locs)) != NULL)
    slAddHead(&genes, gp);

slSort(&genes, genePredCmp);

genesFh = mustOpen(ccdsGeneFile, "w");
for (gp = genes; gp != NULL; gp = gp->next)
    genePredTabOut(gp, genesFh);
carefulClose(&genesFh);
genePredFreeList(&genes);
}

void loadTables(char *hgDb, char *ccdsInfoTbl, char *ccdsInfoFile,
                char *ccdsGeneTbl, char *ccdsGeneFile)
/* load tables into database */
{
struct sqlConnection *conn = sqlConnect(hgDb);
char ccdsInfoTmpTbl[512], ccdsGeneTmpTbl[512];
char *ccdsInfoSql, *ccdsGeneSql;

/* create tables with _tmp extension, then rename after both are loaded */
safef (ccdsInfoTmpTbl, sizeof(ccdsInfoTmpTbl), "%s_tmp", ccdsInfoTbl);
ccdsInfoSql = ccdsInfoGetCreateSql(ccdsInfoTmpTbl);
sqlRemakeTable(conn, ccdsInfoTmpTbl, ccdsInfoSql);
sqlLoadTabFile(conn, ccdsInfoFile, ccdsInfoTmpTbl, SQL_TAB_FILE_ON_SERVER);

safef(ccdsGeneTmpTbl, sizeof(ccdsGeneTmpTbl), "%s_tmp", ccdsGeneTbl);
ccdsGeneSql = genePredGetCreateSql(ccdsGeneTmpTbl, genePredAllFlds,
                                   genePredBasicSql, hGetMinIndexLength());
sqlRemakeTable(conn, ccdsGeneTmpTbl, ccdsGeneSql);
freeMem(ccdsInfoSql);
freeMem(ccdsGeneSql);
sqlLoadTabFile(conn, ccdsGeneFile, ccdsGeneTmpTbl, SQL_TAB_FILE_ON_SERVER);

ccdsRenameTable(conn, ccdsInfoTmpTbl, ccdsInfoTbl);
ccdsRenameTable(conn, ccdsGeneTmpTbl, ccdsGeneTbl);

if (!keep)
    {
    unlink(ccdsInfoFile);
    unlink(ccdsGeneFile);
    }
sqlDisconnect(&conn);
}

void ccdsMkTables(char *ccdsDb, char *hgDb, char *ccdsInfoOut, char *ccdsGeneOut)
/* create tables for hg db from imported CCDS database */
{
struct sqlConnection *conn = sqlConnect(ccdsDb);
char ccdsInfoFile[PATH_LEN], ccdsInfoTbl[PATH_LEN];
char ccdsGeneFile[PATH_LEN], ccdsGeneTbl[PATH_LEN];
struct genomeInfo genomeInfo = getBuildTaxon(hgDb);
hSetDb(hgDb);

ccdsGetTblFileNames(ccdsInfoOut, ccdsInfoTbl, ccdsInfoFile);
ccdsGetTblFileNames(ccdsGeneOut, ccdsGeneTbl, ccdsGeneFile);

createCcdsInfo(conn, ccdsInfoFile, &genomeInfo);
createCcdsGene(conn, ccdsGeneFile, &genomeInfo);
sqlDisconnect(&conn);

if (loadDb)
    loadTables(hgDb, ccdsInfoTbl, ccdsInfoFile, ccdsGeneTbl, ccdsGeneFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
keep = optionExists("keep");
loadDb = optionExists("loadDb");
ccdsMkTables(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

