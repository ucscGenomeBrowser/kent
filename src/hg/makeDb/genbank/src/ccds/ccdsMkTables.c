/* ccdsMkTables - create tables for hg db from imported CCDS database */

#include "common.h"
#include "options.h"
#include "genePred.h"
#include "jksql.h"
#include "hdb.h"
#include "hash.h"
#include "binRange.h"
#include "ccdsInfo.h"
#include "verbose.h"
#include "ccdsLocationsJoin.h"
#include "ccdsCommon.h"

static char const rcsid[] = "$Id: ccdsMkTables.c,v 1.8 2006/08/29 00:05:30 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"stat", OPTION_STRING|OPTION_MULTI},
    {"loadDb", OPTION_BOOLEAN},
    {"keep", OPTION_BOOLEAN},
    {NULL, 0}
};
static boolean keep = FALSE;    /* keep tab files after load */
static boolean loadDb = FALSE;  /* load database */
static struct slName *statVals = NULL;  /* ccds status values to load */

static char *statValDefaults[] = {
    "Public",
    "Under review, update",
    "Reviewed, update pending",
    "Under review, withdrawal",
    "Reviewed, withdrawal pending",
    NULL
};

static void usage()
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
  "  -stat=statVal - only include CCDS with the specified status values,\n"
  "   which are cases-insensitive.  This option maybe repeated.  Defaults to:\n"
  "      \"Public\",\n"
  "      \"Under review, update\",\n"
  "      \"Reviewed, update pending\",\n"
  "      \"Under review, withdrawal\",\n"
  "      \"Reviewed, withdrawal pending\"\n"  
  "  -loadDb - load tables into hgdb\n"
  "  -keep - keep tab file used to load database\n"
  "  -verbose=n\n"
  "     2 - show selects against CCDS database\n"
  );
}

struct genomeInfo
/* infomation about the specific genome needed for selects */
{
    char *db;
    int taxonId;
    int ncbiBuild;
    int ncbiBuildVersion;
};

/* table of mapping UCSC db to NCBI build information */
static struct genomeInfo genomeInfoTbl[] = 
{
    {"hg17",  9606, 35, 1},
    {"hg18",  9606, 36, 1},
    {"mm8",  10090, 36, 1},
    {NULL,       0,  0, 0}
};


static struct genomeInfo *getBuildTaxon(char *hgDb)
/* translated the target databases to ncbi build and taxon */
{
int i;
for (i = 0;  genomeInfoTbl[i].db != NULL; i++)
    {
    if (sameString(genomeInfoTbl[i].db, hgDb))
        return &(genomeInfoTbl[i]);
    }
errAbort("don't know how to load ccds into %s, supported databases: hg17",
         hgDb);
return NULL;
}

static struct hashEl *gotCcdsSave(struct hash *gotCcds, int ccdsId, int ccdsVersion)
/* save a ccds id in a hash table for sanity check purposes */
{
char id[32];
safef(id, sizeof(id), "ccds%d.%d", ccdsId, ccdsVersion);
return hashStore(gotCcds, id);
}

static void gotCcdsSaveSrcDb(struct hash *gotCcds, int ccdsId, int ccdsVersion,
                             char *srcDb)
/* save a ccds id and source database in a hash table created while build the
 * ccdsInfo table. srcDb is `N' or `H' */
{
struct hashEl *hel = gotCcdsSave(gotCcds, ccdsId, ccdsVersion);
if (hel->val == NULL)
    hel->val = needMem(3);  /* max 2 database characters in string */
if (strchr((char*)hel->val, srcDb[0]) == NULL)
    {
    strcat((char*)hel->val, srcDb);
    assert(strlen((char*)hel->val) <= 2);
    }
}

static int gotCcdsCheck(char *name1, struct hash *got1,
                        char *name2, struct hash *got2)
/* check that CCDSs in the first set were found in the second set */
{
int errCnt = 0;
struct hashCookie cookie = hashFirst(got1);
struct hashEl* hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (hashLookup(got2, hel->name) == NULL)
        {
        fprintf(stderr, "CCDS %s in %s not added to %s\n", hel->name,
                name1, name2);
        errCnt++;
        }
    }
return errCnt;
}

static int gotCcdsCheckSrcDb(struct hashEl *infoEl)
/* check that both hinxton and ncbi are in the source database, return 1
 * if they are not matched. */
{
if ((infoEl->val == NULL) || (strchr((char*)infoEl->val, 'N') == NULL)
    || (strchr((char*)infoEl->val, 'H') == NULL))
    {
    fprintf(stderr, "CCDS %s does not have entries for both NCBI and Hinxton in the cdsInfo table, got: %s\n", infoEl->name,
            (char*)infoEl->val);
    return 1;
    }
else
    return 0;
}

static void gotCcdsCheckInfo(struct hash *infoCcds)
/* check source databases added to ccdsInof table */
{
int errCnt = 0;
struct hashCookie cookie = hashFirst(infoCcds);
struct hashEl* hel;
while ((hel = hashNext(&cookie)) != NULL)
    errCnt += gotCcdsCheckSrcDb(hel);
if (errCnt > 0)
    errAbort("Error: not all CCDSs have both NCBI and Hinxton genes in ccdsInfo table");
}
static void gotCcdsValidate(struct hash *infoCcds, struct hash *geneCcds)
/* check that the same set of CCDS ids were found for the info and gene selects */
{
int errCnt = 0;

errCnt += gotCcdsCheck("ccdsInfo", infoCcds, "ccdsGene", geneCcds);
errCnt += gotCcdsCheck("ccdsGene", geneCcds, "ccdsInfo", infoCcds);
if (errCnt > 0)
    errAbort("Error: mismatch between CCDS ids added to ccdsInfo and ccdsGene tables");
gotCcdsCheckInfo(infoCcds);
}

static char *mkStatusValSet(struct sqlConnection *conn)
/* Generate set of CCDS status values to use, based on cmd options or
 * defaults.  WARNING: static return. */
{
static char *statValSet = NULL;
if (statValSet != NULL)
    return statValSet;
struct dyString *buf = dyStringNew(0);
struct slName *val;
struct hash *validStats = ccdsStatusValLoad(conn);

for (val = statVals; val != NULL; val = val->next)
    {
    ccdsStatusValCheck(validStats, val->name);
    if (buf->stringSize > 0)
        dyStringAppendC(buf, ',');
    dyStringPrintf(buf, "\"%s\"", val->name);
    }
hashFree(&validStats);
statValSet = dyStringCannibalize(&buf);
return statValSet;
}

static char *mkGroupVersionClause(struct genomeInfo *genome, struct sqlConnection *conn)
/* get part of where clause that selects GroupVersions and CcdsUids that are currently
 * public.  WARNING: static return. */
{
static char clause[1025];
safef(clause, sizeof(clause),
      "((Groups.tax_id = %d) "
      "AND (GroupVersions.ncbi_build_number = %d) "
      "AND (GroupVersions.first_ncbi_build_version <= %d) "
      "AND (%d <= GroupVersions.last_ncbi_build_version) "
      "AND (GroupVersions.group_uid = Groups.group_uid) "
      "AND (CcdsStatusVals.ccds_status_val_uid = GroupVersions.ccds_status_val_uid) "
      "AND (CcdsStatusVals.ccds_status in (%s)) "
      "AND (CcdsUids.group_uid = Groups.group_uid))",
      genome->taxonId, genome->ncbiBuild,
      genome->ncbiBuildVersion, genome->ncbiBuildVersion,
      mkStatusValSet(conn));
return clause;
}

static char *mkCcdsInfoSelect(struct genomeInfo *genome, struct sqlConnection *conn)
/* Construct select to get data for ccdsInfo table.  WARNING: static
 * return. */
{
static char select[4096];
safef(select, sizeof(select),
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
      "WHERE %s "
      "AND (Accessions.accession_uid = Accessions_GroupVersions.accession_uid) "
      "AND (Accessions_GroupVersions.group_version_uid = GroupVersions.group_version_uid) "
      "AND (Organizations.organization_uid = Accessions.organization_uid)",
      mkGroupVersionClause(genome, conn));
return select;
}

static void processCcdsInfoRow(char **row, FILE *outFh, struct hash *gotCcds)
/* process a row from ccdsInfoSelect */
{
int ccdsId = sqlSigned(row[0]);
int ccdsVersion = sqlSigned(row[1]);
struct ccdsInfo ci;
char *srcDb;
safef(ci.ccds, sizeof(ci.ccds), "CCDS%d.%d", ccdsId, ccdsVersion);
if (sameString(row[2], "NCBI"))
    {
    /* NCBI has separate version numbers */
    ci.srcDb = ccdsInfoNcbi;
    safef(ci.mrnaAcc, sizeof(ci.mrnaAcc), "%s.%s", row[3], row[4]);
    safef(ci.protAcc, sizeof(ci.protAcc), "%s.%s", row[5], row[6]);
    srcDb = "N";
    }
else
    {
    ci.srcDb = (startsWith("OTT", ci.mrnaAcc) ? ccdsInfoVega : ccdsInfoEnsembl);
    safef(ci.mrnaAcc, sizeof(ci.mrnaAcc), "%s", row[3]);
    safef(ci.protAcc, sizeof(ci.protAcc), "%s", row[5]);
    srcDb = "H";
    }
ccdsInfoTabOut(&ci, outFh);
gotCcdsSaveSrcDb(gotCcds, ccdsId, ccdsVersion, srcDb);
}

static void createCcdsInfo(struct sqlConnection *conn, char *ccdsInfoFile,
                           struct genomeInfo *genome, struct hash *gotCcds)
/* create ccdsInfo table file */
{
char *query = mkCcdsInfoSelect(genome, conn);
struct sqlResult *sr = sqlGetResult(conn, query);
FILE *outFh = mustOpen(ccdsInfoFile, "w");
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    processCcdsInfoRow(row, outFh, gotCcds);
carefulClose(&outFh);
sqlFreeResult(&sr);
}

static char *mkCcdsGeneSelect(struct genomeInfo *genome, struct sqlConnection *conn)
/* Construct select to get data for ccdsGene table.  WARNING: static
 * return. */
{
static char select[4096];
safef(select, sizeof(select),
      "SELECT "
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
      "WHERE %s "
      "AND (Locations.location_uid = Locations_GroupVersions.location_uid) "
      "AND (Locations_GroupVersions.group_version_uid = GroupVersions.group_version_uid)",
      mkGroupVersionClause(genome, conn));
return select;
}

static int ccdsLocationsJoinCmp(const void *va, const void *vb)
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

static void locationProcessRow(char **row, struct ccdsLocationsJoin **locsList, struct hash *gotCcds)
/* proces a row from the locations query and add to the list.  If the
 * chromsome is XY, split it into two entries */
{
struct ccdsLocationsJoin *loc = ccdsLocationsJoinLoad(row);
if (sameString(loc->chrom, "XY"))
    {
    /* no dynamic data, so can just copy */
    struct ccdsLocationsJoin *locY;
    AllocVar(locY);
    *locY = *loc;
    strcpy(loc->chrom, "X");
    strcpy(locY->chrom, "Y");
    slSafeAddHead(locsList, locY);
    }
slSafeAddHead(locsList, loc);
gotCcdsSave(gotCcds, loc->ccds_uid, loc->ccds_version);
}

static struct ccdsLocationsJoin *loadLocations(struct sqlConnection *conn,
                                               struct genomeInfo *genome,
                                               struct hash *gotCcds)
/* load all exon locations into a list, sort by ccds id, and then
 * chrom and start */
{
char *query = mkCcdsGeneSelect(genome, conn);
struct sqlResult *sr = sqlGetResult(conn, query);
struct ccdsLocationsJoin *locs = NULL;
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    locationProcessRow(row, &locs, gotCcds);
sqlFreeResult(&sr);
slSort(&locs, ccdsLocationsJoinCmp);
return locs;
}

static struct ccdsLocationsJoin *popNextCcdsLocs(struct ccdsLocationsJoin **locsList)
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

static struct genePred *nextCcdsGenePred(struct ccdsLocationsJoin **locsList)
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

safef(buf, sizeof(buf), "CCDS%d.%d", locs->ccds_uid, locs->ccds_version);
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

static void createCcdsGene(struct sqlConnection *conn, char *ccdsGeneFile,
                           struct genomeInfo *genome, struct hash *gotCcds)
/* create the ccdsGene tab file from the ccds database */
{
struct ccdsLocationsJoin *locs = loadLocations(conn, genome, gotCcds);
struct genePred *genes = NULL, *gp;
FILE *genesFh;

while ((gp = nextCcdsGenePred(&locs)) != NULL)
    slAddHead(&genes, gp);

slSort(&genes, genePredCmp);

genesFh = mustOpen(ccdsGeneFile, "w");
for (gp = genes; gp != NULL; gp = gp->next)
    {
    fprintf(genesFh, "%d\t", binFromRange(gp->txStart, gp->txEnd));
    genePredTabOut(gp, genesFh);
    }
carefulClose(&genesFh);
genePredFreeList(&genes);
}

static void loadTables(char *hgDb, char *ccdsInfoTbl, char *ccdsInfoFile,
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
                                   genePredWithBin, hGetMinIndexLength());
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

static void ccdsMkTables(char *ccdsDb, char *hgDb, char *ccdsInfoOut, char *ccdsGeneOut)
/* create tables for hg db from imported CCDS database */
{
if (verboseLevel() >= 2)
    sqlMonitorEnable(JKSQL_TRACE);
struct sqlConnection *conn = sqlConnect(ccdsDb);
char ccdsInfoFile[PATH_LEN], ccdsInfoTbl[PATH_LEN];
char ccdsGeneFile[PATH_LEN], ccdsGeneTbl[PATH_LEN];
struct genomeInfo *genome = getBuildTaxon(hgDb);
struct hash *infoCcds = hashNew(18);
struct hash *geneCcds = hashNew(18);
hSetDb(hgDb);

ccdsGetTblFileNames(ccdsInfoOut, ccdsInfoTbl, ccdsInfoFile);
ccdsGetTblFileNames(ccdsGeneOut, ccdsGeneTbl, ccdsGeneFile);

createCcdsInfo(conn, ccdsInfoFile, genome, infoCcds);
createCcdsGene(conn, ccdsGeneFile, genome, geneCcds);
sqlDisconnect(&conn);
sqlMonitorDisable();
gotCcdsValidate(infoCcds, geneCcds);

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
statVals = optionMultiVal("stat", NULL);
if (statVals == NULL)
    {
    int i;
    for (i = 0; statValDefaults[i] != NULL; i++)
        slSafeAddHead(&statVals, slNameNew(statValDefaults[i]));
    }
ccdsMkTables(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

