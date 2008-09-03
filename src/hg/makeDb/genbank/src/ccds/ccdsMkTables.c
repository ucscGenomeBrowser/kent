/* ccdsMkTables - create tables for hg db from imported CCDS database */

#include "common.h"
#include "options.h"
#include "genePred.h"
#include "jksql.h"
#include "hdb.h"
#include "dystring.h"
#include "hash.h"
#include "binRange.h"
#include "ccdsInfo.h"
#include "verbose.h"
#include "ccdsLocationsJoin.h"
#include "ccdsCommon.h"
#include <sys/types.h>
#include <regex.h>

static char const rcsid[] = "$Id: ccdsMkTables.c,v 1.21 2008/09/03 19:19:33 markd Exp $";

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
  "   ccdsMkTables [options] ccdsDb hgDb ncbiBuild ccdsInfoOut ccdsGeneOut\n"
  "\n"
  "ccdsDb is the database created by ccdsImport. If the name is in the form\n"
  "'profile:ccdsDb', then hg.conf variables starting with 'profile.' are\n"
  "used to open the database. hgDb is the targeted genome database, required\n"
  "even if tables are not being loaded.\n"
  "ncbiBuild is the NCBI build number, such as 32.2.\n"
  "If -loadDb is specified, ccdsInfoOut and ccdsGeneOut are tables to load\n"
  "with the cddsInfo and genePred data.  If -loadDb is not specified, they\n"
  "are files for the data.\n"
  "\n"
  "Options:\n"
  "  -stat=statVal - only include CCDS with the specified status values,\n"
  "   which are case-insensitive.  This option maybe repeated.  Defaults to:\n"
  "      \"Public\",\n"
  "      \"Under review, update\",\n"
  "      \"Reviewed, update pending\",\n"
  "      \"Under review, withdrawal\",\n"
  "      \"Reviewed, withdrawal pending\"\n"
  "   if \"any\" is specified, then status is not used in selection.\n"
  "  -loadDb - load tables into hgdb, bin column is added to genePred\n"
  "  -keep - keep tab file used to load database\n"
  "  -verbose=n\n"
  "     2 - show selects against CCDS database\n"
  "     3 - include ignored entries\n"
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

static char *ccdsMkId(int ccdsId, int ccdsVersion)
/* construct a CCDS id.  WARNING: static return */
{
static char id[32];
safef(id, sizeof(id), "CCDS%d.%d", ccdsId, ccdsVersion);
return id;
}

static int getTaxon(char *hgDb)
/* get the taxon id for the organism associated with hgDb */
{
if (startsWith("hg", hgDb))
    return 9606;
else if (startsWith("mm", hgDb))
    return 10090;
else
    errAbort("don't know taxon id for %s", hgDb);
return 0;
};

static int parseNcbiBuild(char *ncbiBuild, int *verRet)
/* parse an NCBI build number */
{
char *dotPtr = skipNumeric(ncbiBuild);
char *endPtr = skipNumeric(dotPtr+1);
if (!(isdigit(ncbiBuild[0]) && (*dotPtr == '.') && (*endPtr == '\0')))
    errAbort("invalid NCBI build number: %s", ncbiBuild);
*dotPtr = '\0';
int bld = sqlUnsigned(ncbiBuild);
*dotPtr = '.';
*verRet = sqlUnsigned(dotPtr+1);
return bld;
}

static struct genomeInfo *getGenomeInfo(char *hgDb, char *ncbiBuild)
/* translated the target databases to ncbi build and taxon */
{
struct genomeInfo *gi;
AllocVar(gi);
gi->db = cloneString(hgDb);
gi->taxonId = getTaxon(hgDb);
gi->ncbiBuild = parseNcbiBuild(ncbiBuild, &gi->ncbiBuildVersion);
return gi;
}

static struct hashEl *gotCcdsSave(struct hash *gotCcds, int ccdsId, int ccdsVersion)
/* save a ccds id in a hash table for sanity check purposes */
{
return hashStore(gotCcds, ccdsMkId(ccdsId, ccdsVersion));
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

static boolean ccdsIsIgnored(struct hash* ignoreTbl, int ccdsId, int ccdsVersion)
/* detemine if a CCDS should be ignored */
{
return (hashLookup(ignoreTbl, ccdsMkId(ccdsId, ccdsVersion)) != NULL);
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

static bool selectByStatus()
/* check if specific status values should be used in select */
{
struct slName *val;
for (val = statVals; val != NULL; val = val->next)
    {
    if (sameString(val->name, "any"))
        return FALSE;
    }
return TRUE;
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

static char *mkCommonFrom()
/* Get common FROM clause. WARNING: static return. */
{
static char clause[257];
safecpy(clause, sizeof(clause),
        "Groups, "
        "GroupVersions, "
        "CcdsUids");
if (selectByStatus())
    safecat(clause, sizeof(clause), ", CcdsStatusVals");
return clause;
}

static char *mkCommonWhere(struct genomeInfo *genome, struct sqlConnection *conn)
/* Get the common clause.  This is the where clause that selects GroupVersions
 * and CcdsUids that are currently selected by organism, build, and optionally status
 * values. WARNING: static return. */
{
static char clause[1025];
safef(clause, sizeof(clause),
      "((Groups.tax_id = %d) "
      "AND (GroupVersions.ncbi_build_number = %d) "
      "AND (GroupVersions.first_ncbi_build_version <= %d) "
      "AND (%d <= GroupVersions.last_ncbi_build_version) "
      "AND (GroupVersions.group_uid = Groups.group_uid) "
      "AND (CcdsUids.group_uid = Groups.group_uid) ",
      genome->taxonId, genome->ncbiBuild,
      genome->ncbiBuildVersion, genome->ncbiBuildVersion);

if (selectByStatus())
    {
    char clause2[1025];
    safef(clause2, sizeof(clause2),
          "AND (CcdsStatusVals.ccds_status_val_uid = GroupVersions.ccds_status_val_uid) "
          "AND (CcdsStatusVals.ccds_status in (%s)) ",
          mkStatusValSet(conn));
    safecat(clause, sizeof(clause), clause2);
    }
safecat(clause, sizeof(clause), ")");
return clause;
}

static void dumpIgnoreTbl(struct hash* ignoreTbl)
/* print ignoreTbl for debugging purposes */
{
struct hashCookie cookie = hashFirst(ignoreTbl);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    verbose(3, "ignore: %s: %s\n", hel->name, (char*)hel->val);
}

static void findPartialMatches(struct sqlConnection *conn, struct genomeInfo *genome,
                               struct hash* ignoreTbl)
/* find CCDS interpretation_subtype of "Partial match".  Does in a separate
 * pass due to the lack of sub-selects in mysql 4. */
{
verbose(2, "begin findPartialMatches\n");
static char select[4096];
safef(select, sizeof(select),
      "SELECT "
      "CcdsUids.ccds_uid, GroupVersions.ccds_version "
      "FROM %s, Interpretations, InterpretationSubtypes "
      "WHERE %s "
      "AND (Interpretations.ccds_uid = CcdsUids.ccds_uid) "
#if 0 // FIXME: not sure why this breaks query
      "AND (Interpretations.group_uid = GroupVersions.group_uid) "
#endif
      "AND (Interpretations.interpretation_subtype_uid = InterpretationSubtypes.interpretation_subtype_uid) "
      "AND (InterpretationSubtypes.interpretation_subtype = \"Partial match\")",
      mkCommonFrom(), mkCommonWhere(genome, conn));

struct sqlResult *sr = sqlGetResult(conn, select);
char **row;
int cnt = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hashEl *hel = hashStore(ignoreTbl, ccdsMkId(sqlUnsigned(row[0]), sqlUnsigned(row[1])));
    hel->val = "partial_match";
    cnt++;
    }
sqlFreeResult(&sr);
verbose(2, "end findPartialMatches: %d partial matches found\n", cnt);
}

static struct hash* buildIgnoreTbl(struct sqlConnection *conn, struct genomeInfo *genome)
/* Build table of CCDS ids to ignore.  This currently contains:
 *   - ones that have the interpretation_subtype of "Partial match".
 * This should be doable as part of the query, but MySQL 4.0 was very, very slow at it. */
{
struct hash* ignoreTbl = hashNew(20);
findPartialMatches(conn, genome, ignoreTbl);
if (verboseLevel() >= 3)
    dumpIgnoreTbl(ignoreTbl);
return ignoreTbl;
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
      "FROM %s, Accessions_GroupVersions, Accessions, Organizations "
      "WHERE %s "
      "AND (Accessions_GroupVersions.group_version_uid = GroupVersions.group_version_uid) "
      "AND (Accessions.accession_uid = Accessions_GroupVersions.accession_uid) "
      "AND (Organizations.organization_uid = Accessions.organization_uid)",
      mkCommonFrom(), mkCommonWhere(genome, conn));
return select;
}

static boolean processCcdsInfoRow(char **row, struct ccdsInfo **ccdsInfoList,
                                  struct hash* ignoreTbl, struct hash *gotCcds)
/* Process a row from ccdsInfoSelect and add to the list of ccdsInfo rows.
 * Buffering the rows is necessary since there maybe multiple versions of an
 * accession returned.  While only one accession will be alive, we can't
 * filter on Accessions.alive due to the update time lag between RefSeq and
 * CCDS.  Returns True if processed, False if ignored.
 */
{
int ccdsId = sqlSigned(row[0]);
int ccdsVersion = sqlSigned(row[1]);
if (!ccdsIsIgnored(ignoreTbl, ccdsId, ccdsVersion))
    {
    struct ccdsInfo *ci;
    AllocVar(ci);
    char *srcDb;
    safecpy(ci->ccds, sizeof(ci->ccds), ccdsMkId(ccdsId, ccdsVersion));
    if (sameString(row[2], "NCBI"))
        {
        /* NCBI has separate version numbers */
        ci->srcDb = ccdsInfoNcbi;
        safef(ci->mrnaAcc, sizeof(ci->mrnaAcc), "%s.%s", row[3], row[4]);
        safef(ci->protAcc, sizeof(ci->protAcc), "%s.%s", row[5], row[6]);
        srcDb = "N";
        }
    else
        {
        ci->srcDb = (startsWith("OTT", ci->mrnaAcc) ? ccdsInfoVega : ccdsInfoEnsembl);
        safef(ci->mrnaAcc, sizeof(ci->mrnaAcc), "%s", row[3]);
        safef(ci->protAcc, sizeof(ci->protAcc), "%s", row[5]);
        srcDb = "H";
        }
    slSafeAddHead(ccdsInfoList, ci);
    gotCcdsSaveSrcDb(gotCcds, ccdsId, ccdsVersion, srcDb);
    return TRUE;
    }
else
    return FALSE;
}

static int lenLessVer(char *acc)
/* get the length of an accession, less a dot version extension, if any */
{
char *dot = strchr(acc, '.');
return (dot == NULL) ? strlen(acc) : (dot-acc);
}

static boolean sameAccession(char *acc1, char *acc2)
/* test if an accessions is the same, ignoring optional versions */
{
int len1 = lenLessVer(acc1);
int len2 = lenLessVer(acc2);
return (len1 != len2) ? FALSE : sameStringN(acc1, acc2, len1);
}

static boolean keepCcdsInfo(struct ccdsInfo *ci, struct ccdsInfo *nextCi)
/* determine if an ccds info entry should be kept, or if the next entry in
 * a sorted list has a new version */
{
return (nextCi == NULL) || (ci->srcDb != ccdsInfoNcbi) || !sameAccession(ci->mrnaAcc, nextCi->mrnaAcc);
}

static void ccdsFilterDupAccessions(struct ccdsInfo **ccdsInfoList)
/* remove duplicate accessions (see processCcdsInfoRow for why we have them),
 * keeping only the latest version.  Also sort the list in CCDS order. */
{
struct ccdsInfo *newList = NULL, *ci;
ccdsInfoCcdsMRnaSort(ccdsInfoList);
while ((ci = slPopHead(ccdsInfoList)) != NULL)
    {
    if (keepCcdsInfo(ci, *ccdsInfoList))
        slAddHead(&newList, ci);
    else
        ccdsInfoFree(&ci);
    }
slReverse(&newList);
*ccdsInfoList = newList;
}

static void createCcdsInfo(struct sqlConnection *conn, char *ccdsInfoFile,
                           struct genomeInfo *genome, struct hash* ignoreTbl,
                           struct hash *gotCcds)
/* create ccdsInfo table file */
{
verbose(2, "begin createCcdsInfo\n");
char *query = mkCcdsInfoSelect(genome, conn);
struct sqlResult *sr = sqlGetResult(conn, query);
struct ccdsInfo *ccdsInfoList = NULL, *ci;
char **row;
int cnt = 0, ignoreCnt = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!processCcdsInfoRow(row, &ccdsInfoList, ignoreTbl, gotCcds))
        ignoreCnt++;
    cnt++;
    }
sqlFreeResult(&sr);

ccdsFilterDupAccessions(&ccdsInfoList);

FILE *fh = mustOpen(ccdsInfoFile, "w");
for (ci = ccdsInfoList; ci != NULL; ci = ci->next)
    ccdsInfoTabOut(ci, fh);
carefulClose(&fh);
ccdsInfoFreeList(&ccdsInfoList);
verbose(2, "end createCcdsInfo: %d processed, %d ignored, %d kept\n",
        cnt, ignoreCnt, cnt-ignoreCnt);
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
      "FROM %s, Locations, Locations_GroupVersions "
      "WHERE %s "
      "AND (Locations.location_uid = Locations_GroupVersions.location_uid) "
      "AND (Locations_GroupVersions.group_version_uid = GroupVersions.group_version_uid)",
      mkCommonFrom(), mkCommonWhere(genome, conn));
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

static void locationProcessRow(char **row, struct ccdsLocationsJoin **locsList,
                               struct hash* ignoreTbl, struct hash *gotCcds)
/* proces a row from the locations query and add to the list.  If the
 * chromsome is XY, split it into two entries */
{
struct ccdsLocationsJoin *loc = ccdsLocationsJoinLoad(row);
if (ccdsIsIgnored(ignoreTbl, loc->ccds_uid, loc->ccds_version))
    ccdsLocationsJoinFree(&loc);
else
    {
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
}

static struct ccdsLocationsJoin *loadLocations(struct sqlConnection *conn, struct genomeInfo *genome,
                                               struct hash* ignoreTbl, struct hash *gotCcds)
/* load all exon locations into a list, sort by ccds id, and then
 * chrom and start */
{
verbose(2, "begin loadLocations\n");
char *query = mkCcdsGeneSelect(genome, conn);
struct sqlResult *sr = sqlGetResult(conn, query);
struct ccdsLocationsJoin *locs = NULL;
char **row;
int cnt = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    locationProcessRow(row, &locs, ignoreTbl, gotCcds);
    cnt++;
    }
sqlFreeResult(&sr);
slSort(&locs, ccdsLocationsJoinCmp);
verbose(2, "end loadLocations: %d exons\n", cnt);
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

gp->name = cloneString(ccdsMkId(locs->ccds_uid, locs->ccds_version));
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

static struct genePred *buildCcdsGene(struct ccdsLocationsJoin **locsList)
/* build sorted list of CCDS genes from exon locations */
{
verbose(2, "begin buildCcdsGene\n");
struct genePred *genes = NULL, *gp;
int cnt = 0;
while ((gp = nextCcdsGenePred(locsList)) != NULL)
    {
    slAddHead(&genes, gp);
    cnt++;
    }
slSort(&genes, genePredCmp);

verbose(2, "end buildCcdsGene: %d CCDS genes\n", cnt);
return genes;
}

static void createCcdsGene(struct sqlConnection *conn, char *ccdsGeneFile,
                           struct genomeInfo *genome, struct hash* ignoreTbl,
                           struct hash *gotCcds)
/* create the ccdsGene tab file from the ccds database */
{
struct ccdsLocationsJoin *locs = loadLocations(conn, genome, ignoreTbl, gotCcds);
struct genePred *gp, *genes = buildCcdsGene(&locs);
FILE *genesFh;

genesFh = mustOpen(ccdsGeneFile, "w");
for (gp = genes; gp != NULL; gp = gp->next)
    {
    if (loadDb)
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
                                   genePredWithBin, hGetMinIndexLength(hgDb));
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

static struct sqlConnection *ccdsSqlConn(char *ccdsDb)
/* open ccds database, handle profile.db syntax */
{
char buf[256];
safecpy(buf, sizeof(buf), ccdsDb);
char *sep = strchr(buf, ':');
char *profile, *db;
if (sep == NULL)
    {
    profile = "db";
    db = ccdsDb;
    }
else
    {
    profile = buf;
    *sep = '\0';
    db = sep+1;
    }
return sqlConnectProfile(profile, db);
}

static void ccdsMkTables(char *ccdsDb, char *hgDb, char *ncbiBuild, char *ccdsInfoOut, char *ccdsGeneOut)
/* create tables for hg db from imported CCDS database */
{
if (verboseLevel() >= 2)
    sqlMonitorEnable(JKSQL_TRACE);
struct sqlConnection *ccdsConn = ccdsSqlConn(ccdsDb);
char ccdsInfoFile[PATH_LEN], ccdsInfoTbl[PATH_LEN];
char ccdsGeneFile[PATH_LEN], ccdsGeneTbl[PATH_LEN];
struct genomeInfo *genome = getGenomeInfo(hgDb, ncbiBuild);
struct hash *infoCcds = hashNew(20);
struct hash *geneCcds = hashNew(20);

ccdsGetTblFileNames(ccdsInfoOut, ccdsInfoTbl, ccdsInfoFile);
ccdsGetTblFileNames(ccdsGeneOut, ccdsGeneTbl, ccdsGeneFile);

struct hash* ignoreTbl = buildIgnoreTbl(ccdsConn, genome);

createCcdsInfo(ccdsConn, ccdsInfoFile, genome, ignoreTbl, infoCcds);
createCcdsGene(ccdsConn, ccdsGeneFile, genome, ignoreTbl, geneCcds);
sqlDisconnect(&ccdsConn);
sqlMonitorDisable();

gotCcdsValidate(infoCcds, geneCcds);

if (loadDb)
    loadTables(hgDb, ccdsInfoTbl, ccdsInfoFile, ccdsGeneTbl, ccdsGeneFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 6)
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
ccdsMkTables(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

