/* gbSanity - validate genbank data on disk and in a database. Checks the
 * following:
 *   - consistency of the gbIndex files
 *   - gbStatus table rows against gbIndex:
 *     - acc exists in release
 *     - aligned version matches latest version in release
 *     - moddate matches latest moddate for version in release
 *     - number of alignments for acc match
 *   - all gbCdnaInfo table entries are in gbSeq table
 *   - gbSeq table sizes seem sane for mRNAs/ESTs
 *   - mRNAs/ESTs have gbExtFiile entries an seq offset and size makes sense
 *     with gbExtFile size.
 *   - File exists and is readable for gbExtFile and the size matches.
 *   - check that number of PSL entries for each acc in all and per-chrom
 *     tables matches number in gbStatus table.
 *   - check PSL target fields against chrom sizes
 *   - check PSL query fields against gbSeq table.
 */

#include "common.h"
#include "options.h"
#include "hdb.h"
#include "hash.h"
#include "chkCommon.h"
#include "portable.h"
#include "hgRelate.h"
#include "gbIndex.h"
#include "gbRelease.h"
#include "gbAligned.h"
#include "gbUpdate.h"
#include "gbGenome.h"
#include "gbFileOps.h"
#include "gbProcessed.h"
#include "gbVerb.h"
#include "metaData.h"
#include "chkMetaDataTbls.h"
#include "chkGbIndex.h"
#include "chkAlignTbls.h"
#include "../dbload/dbLoadOptions.h"
#include <stdarg.h>

static char const rcsid[] = "$Id: gbSanity.c,v 1.16.46.1 2008/07/31 02:24:34 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"gbdbCurrent", OPTION_STRING},
    {"srcDb", OPTION_STRING},
    {"type", OPTION_STRING},
    {"accPrefix", OPTION_STRING},
    {"test", OPTION_BOOLEAN},
    {"checkExtSeqRecs", OPTION_BOOLEAN},
    {"conf", OPTION_STRING},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

// FIXME: use of gbLoadRna options parsing is a bit of a hack.

/* global parameters from command line */
static char* gGbdbMapToCurrent = NULL;  /* map this gbdb root to current */
static boolean gCheckExtSeqRecs = FALSE;
static struct dbLoadOptions gOptions; /* options from cmdline and conf */

static void checkGbCdnaInfoStrKeys(struct sqlConnection* conn)
/* Verify that the ids appear valid for all of the unique string tables
 * referenced by the mrna table.  This does a join of the mrna with
 * all of the other tables.  If the number of results don't match
 * the number of rows in the table, some of the ids are wrong.
 */
{
static char *joinSql =
    "SELECT count(*) FROM "
    "gbCdnaInfo,author,cds,cell,description,development,geneName,"
    "keyword,library,mrnaClone,organism,productName,sex,"
    "source,tissue "
    "WHERE gbCdnaInfo.author=author.id AND gbCdnaInfo.cds=cds.id "
    "AND gbCdnaInfo.cell=cell.id AND gbCdnaInfo.description=description.id "
    "AND gbCdnaInfo.development=development.id AND gbCdnaInfo.geneName=geneName.id "
    "AND gbCdnaInfo.keyword=keyword.id AND gbCdnaInfo.library=library.id "
    "AND gbCdnaInfo.mrnaClone=mrnaClone.id AND gbCdnaInfo.organism=organism.id "
    "AND gbCdnaInfo.productName=productName.id AND gbCdnaInfo.sex=sex.id "
    "AND gbCdnaInfo.source=source.id AND gbCdnaInfo.tissue=tissue.id";
unsigned numJoinRows = sqlQuickNum(conn, joinSql);
unsigned numTotalRows = sqlQuickNum(conn, "SELECT count(*) FROM gbCdnaInfo");

if (numJoinRows != numTotalRows)
    gbError("number of rows in gbCdnaInfo join with string tables does (%u) "
            "does not match total in table (%u), something is wrong",
            numJoinRows, numTotalRows);
}

static void chkGbRelease(struct gbSelect* select,
                         struct metaDataTbls* metaDataTbls)
/* Check a partation of gbRelease */
{
gbVerbEnter(1, "check: %s", gbSelectDesc(select));
/* load required entry date */
gbReleaseLoadProcessed(select);
gbReleaseLoadAligned(select);

chkGbIndex(select, metaDataTbls);

/* unload entries to free memory */
gbReleaseUnload(select->release);
gbVerbLeave(1, "check: %s", gbSelectDesc(select));
}

static boolean loadEnabled(unsigned srcDb, unsigned type,
                           unsigned orgCat)
/* is load enabled for (srcDb, type, orgCat) */
{
struct dbLoadAttr* attr
    = dbLoadOptionsGetAttr(&gOptions, srcDb, type, orgCat);
verbose(1, "%s %s %s: %s\n", gbSrcDbName(srcDb), gbTypeName(type),
        gbOrgCatName(orgCat), (attr->load ? "enabled" : "disabled"));
return attr->load;
}

static unsigned getLoadOrgCats(char* database, unsigned srcDb, unsigned type)
/* determine orgCats that should be loaded, or zero if none */
{
unsigned orgCats = 0;
if (loadEnabled(srcDb, type, GB_NATIVE))
    orgCats |= GB_NATIVE;
if (loadEnabled(srcDb, type, GB_XENO))
    orgCats |= GB_XENO;
return orgCats;
}

unsigned shouldHaveDesc(struct gbSelect* select)
/* determine orgcats that should be descriptions */
{
struct dbLoadAttr* attr;
unsigned descOrgCats = 0;

attr = dbLoadOptionsGetAttr(&gOptions, select->release->srcDb,
                            select->type, GB_NATIVE);
if (attr->load && attr->loadDesc)
    descOrgCats |= GB_NATIVE;
attr = dbLoadOptionsGetAttr(&gOptions, select->release->srcDb,
                            select->type, GB_XENO);
if (attr->load && attr->loadDesc)
    descOrgCats |= GB_XENO;
return descOrgCats;
}

static boolean checkSanity(char *db, struct gbSelect* select,
                           struct sqlConnection* conn)
/* check sanity on a select partation */
{
/* load and validate all metadata */
struct metaDataTbls* metaDataTbls;
unsigned descOrgCats = shouldHaveDesc(select);

metaDataTbls= chkMetaDataTbls(select, conn, gCheckExtSeqRecs, descOrgCats,
                              gGbdbMapToCurrent);
chkGbRelease(select, metaDataTbls);
chkMetaDataXRef(metaDataTbls);

/* check the alignment tables */
int cnt = chkAlignTables(db, select, conn, metaDataTbls, &gOptions);
metaDataTblsFree(&metaDataTbls);
return (cnt > 0) ? TRUE : FALSE;
}

static boolean checkRelease(struct gbRelease* release, char* database,
                            unsigned type, unsigned orgCats, char* accPrefix)
/* Check a release/type */
{
struct sqlConnection* conn = hAllocConn(database);
struct gbSelect select;

ZeroVar(&select);
select.release = release;
select.type = type;

select.orgCats = orgCats;
select.accPrefix = accPrefix;
boolean checked = checkSanity(database, &select, conn);
hFreeConn(&conn);
return checked;
}

static int releaseSanity(struct gbRelease* release, char *database)
/* Run sanity checks on a release */
{
unsigned orgCats;
int checkedSetCnt = 0;

/* check if native, and/or xeno should be included */
orgCats = getLoadOrgCats(database, release->srcDb, GB_MRNA);
if (orgCats != 0)
    {
    if (checkRelease(release, database, GB_MRNA, orgCats, NULL))
        checkedSetCnt++;
    }

orgCats = getLoadOrgCats(database, release->srcDb, GB_EST);
if (orgCats != 0)
    {
    struct slName* prefixes, *prefix;
    boolean checkedSome = FALSE;
    if (gOptions.accPrefixRestrict != NULL)
        prefixes = newSlName(gOptions.accPrefixRestrict);
    else
        prefixes = gbReleaseGetAccPrefixes(release, GB_PROCESSED, GB_EST);
    for (prefix = prefixes; prefix != NULL; prefix = prefix->next)
        {
        if (checkRelease(release, database, GB_EST, orgCats, prefix->name))
            checkedSome = TRUE;
        }
    slFreeList(&prefixes);
    if (checkedSome)
        checkedSetCnt++;
    }
return checkedSetCnt;
}

static struct gbRelease* newestReleaseWithAligns(struct gbIndex* index,
                                                 char* database, unsigned srcDb)
/* find the newest release for srcDb that has alignements */
{
/* can't look at update objects, since they haven't been loaded */
struct gbRelease* release;
char alIdxPat[PATH_LEN];
safef(alIdxPat, sizeof(alIdxPat), "*.%s", ALIDX_EXT);

for (release = index->rels[gbSrcDbIdx(srcDb)]; release != NULL;
     release = release->next)
    {
    char relDir[PATH_LEN];
    struct slName* idxFiles;
    safef(relDir, sizeof(relDir), "%s/%s/%s/%s", GB_ALIGNED_DIR, release->name,
          database, GB_FULL_UPDATE);
    idxFiles = listDir(relDir, alIdxPat);
    if (idxFiles != NULL)
        {
        slFreeList(&idxFiles);
        verbose(1, "checking %s for %s\n", release->name, database);
        return release;
        }
    }
return NULL;
}

static void gbSanity(char* database)
/* Run sanity checks */
{
struct gbIndex* index = gbIndexNew(database, NULL);
struct sqlConnection *conn;
struct gbRelease* release;
gbErrorSetDb(database);
int checkedSetCnt = 0;
if (gOptions.relRestrict == NULL)
    {
    int releaseCnt = 0;
    /* Check each partition of the genbank/refseq using the newest aligned
     * release */
    release = newestReleaseWithAligns(index, database, GB_GENBANK);
    if (release != NULL)
        {
        releaseCnt++;
        checkedSetCnt += releaseSanity(release, database);
        }

    release = newestReleaseWithAligns(index, database, GB_REFSEQ);
    if (release != NULL)
        {
        releaseCnt++;
        checkedSetCnt += releaseSanity(release, database);
        }
    if (releaseCnt == 0)
        errAbort("Error: No RefSeq or Genbank alignments for %s\n"
                 "have updates been enabled?", database);
    }
else
    {
    release = gbIndexMustFindRelease(index, gOptions.relRestrict);
    checkedSetCnt += releaseSanity(release, database);
    }
if (checkedSetCnt == 0)
    errAbort("Error: no alignment data was checked");
verbose(1, "%d alignment sets checked\n", checkedSetCnt);
    
gbIndexFree(&index);

/* check of uniqueStr ids */
conn = hAllocConn(database);
checkGbCdnaInfoStrKeys(conn);
hFreeConn(&conn);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gbSanity - validate genbank data on disk and in a database.\n"
  "\n"
  "Currently validates the latest genbank and refseq releases that are\n"
  "found.  Will create tmp files using TMPDIR env.\n"
  "\n"
  "usage:\n"
  "   gbSanity [options] db\n"
  "     db - database for the alignment\n"
  "   Options:\n"
  "     -gbdbCurrent=dir - map extFile paths from specified root to\n"
  "      specified dir.  \n"
  "     -verbose=n - enable verbose output, values greater than 1 increase\n"
  "      verbosity (max is 3)\n"
  "     -test - enable test mode, certain errors, such as empty tables,\n"
  "      become warnings\n"
  "     -srcDb=src - refseq or genebank)\n"
  "     -type=type - only validate this type.\n"
  "     -accPrefix=aa - only validate this acc prefix.\n"
  "     -checkExtSeqRecs - Check the seq and extTables with the contents\n"
  "      of the fasta files. (This is slow!)\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *database;

optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage();
if (optionExists("gbdbCurrent"))
    {
    char* p;
    gGbdbMapToCurrent = optionVal("gbdbCurrent", NULL);
    /* trim trailing slashes */
    for (p = gGbdbMapToCurrent + (strlen(gGbdbMapToCurrent)-1);
         ((p > gGbdbMapToCurrent) && (*p == '/')); p--)
        *p = '\0';
    }
gbVerbInit(optionInt("verbose", 0));
if (gbVerbose >= 5)
    sqlMonitorEnable(JKSQL_TRACE);
database = argv[1];
gOptions = dbLoadOptionsParse(database);
testMode = optionExists("test");
gCheckExtSeqRecs = optionExists("checkExtSeqRecs");

gbVerbEnter(0, "gbSanity: begin: %s", database);
gbSanity(database);
gbVerbLeave(0, "gbSanity: completed: %d errors", errorCnt);
return ((errorCnt == 0) ? 0 : 1);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
