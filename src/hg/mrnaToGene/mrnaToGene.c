/* mrnaToGene - convert PSL alignments of mRNAs to gene annotation.
 */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "genbank.h"
#include "genePred.h"
#include "psl.h"
#include "hash.h"
#include "linefile.h"

static char const rcsid[] = "$Id: mrnaToGene.c,v 1.19 2008/05/11 20:31:14 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"cdsDb", OPTION_STRING},
    {"cdsFile", OPTION_STRING},
    {"requireUtr", OPTION_BOOLEAN},
    {"smallInsertSize", OPTION_INT},
    {"insertMergeSize", OPTION_INT},
    {"smallInsertSize", OPTION_INT},
    {"cdsMergeSize", OPTION_INT},
    {"cdsMergeMod3", OPTION_BOOLEAN},
    {"utrMergeSize", OPTION_INT},
    {"genePredExt", OPTION_BOOLEAN},
    {"allCds", OPTION_BOOLEAN},
    {"noCds", OPTION_BOOLEAN},
    {"keepInvalid", OPTION_BOOLEAN},
    {"ignoreUniqSuffix", OPTION_BOOLEAN},
    {"quiet", OPTION_BOOLEAN},
    {NULL, 0}
};

/* command line options */
static int gCdsMergeSize = -1;
static int gUtrMergeSize = -1;
static unsigned gPslOptions = genePredPslDefaults;
static boolean gRequireUtr = FALSE;
static boolean gKeepInvalid = FALSE;
static boolean gAllCds = FALSE;
static boolean gNoCds = FALSE;
static boolean gGenePredExt = FALSE;
static boolean gQuiet = FALSE;
static boolean gIgnoreUniqSuffix = FALSE;

/* hash table of accession to CDS */
static struct hash *gCdsTable = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mrnaToGene - convert PSL alignments of mRNAs to gene annotations\n"
  "usage:\n"
  "   mrnaToGene [options] psl genePredFile\n"
  "\n"
  "Convert PSL alignments with CDS annotation from genbank to  gene\n"
  "annotations in genePred format.  Accessions without valids CDS are\n"
  "optionally dropped. A best attempt is made to convert incomplete CDS\n"
  "annotations.\n"
  "\n"
  "The psl argument may either be a PSL file or a table in a databases,\n"
  "depending on options.  CDS maybe obtained from the database or file.\n"
  "Accession in PSL files are tried with and with out genbank versions.\n"
  "\n"
  "Options:\n"
  "  -db=db - get PSLs and CDS from this database, psl specifies the table.\n"
  "  -cdsDb=db - get CDS from this database, psl is a file.\n"
  "  -cdsFile=file - get CDS from this database, psl is a file.\n"
  "   File is table seperate with accession as the first column and\n"
  "   CDS the second\n"
  "  -insertMergeSize=%d - Merge inserts (gaps) no larger than this many bases.\n"
  "   A negative size disables merging of blocks.  This differs from specifying zero\n"
  "   in that adjacent blocks will not be merged, allowing tracking of frame for\n"
  "   each block. Defaults to %d unless -cdsMergeSize or -utrMergeSize are specified,\n"
  "   if either of these are specified, this option is ignored.\n"
  "  -smallInsertSize=n - alias for -insertMergetSize\n"
  "  -cdsMergeSize=-1 - merge gaps in CDS no larger than this size.\n"
  "   A negative values disables.\n"
  "  -cdsMergeMod3 - only merge CDS gaps if they mod 3\n"
  "  -utrMergeSize=-1  - merge gaps in UTR no larger than this size.\n"
  "   A negative values disables.\n"
  "  -requireUtr - Drop sequences that don't have both 5' and 3' UTR annotated.\n"
  "  -genePredExt - create a extended genePred, including frame information.\n"
  "  -allCds - consider PSL to be all CDS.\n"
  "  -noCds - consider PSL to not contain any CDS.\n"
  "  -keepInvalid - Keep sequences with invalid CDS.\n"
  "  -quiet - Don't print print info about dropped sequences.\n"
  "  -ignoreUniqSuffix - ignore all characters after last `-' in qName\n"
  "   when looking up CDS. Used when a suffix has been added to make qName\n"
  "   unique.  It is not removed from the name in the genePred.\n"
  "\n", genePredStdInsertMergeSize, genePredStdInsertMergeSize);
}

void loadCdsFile(char *cdsFile)
/* read a CDS file into the global hash */
{
struct lineFile *lf = lineFileOpen(cdsFile, TRUE);
char *row[2];

gCdsTable = hashNew(20);
while (lineFileNextRowTab(lf, row, 2))
    hashAdd(gCdsTable, row[0], 
            lmCloneString(gCdsTable->lm, row[1]));

lineFileClose(&lf);
}

int getGbVersionIdx(char *acc)
/* determine if a accession appears to have a genbank version extension.  If
 * so, return index of the dot, otherwise -1 */
{
char *verPtr = strrchr(acc, '.');
int dotIdx;
if (verPtr == NULL)
    return -1;
dotIdx = verPtr - acc;
verPtr++;
while (*verPtr != '\0')
    {
    if (!isdigit(*verPtr))
        return -1;
    verPtr++;
    }
return dotIdx;
}

char *cdsQuery(struct sqlConnection *conn, char *acc, char *cdsBuf, int cdsBufSize)
/* query for a CDS, either in the hash table or database */
{
if (gCdsTable != NULL)
    return hashFindVal(gCdsTable, acc);
else
    {
    char query[512];
    safef(query, sizeof(query),
          "SELECT cds.name FROM cds,gbCdnaInfo WHERE (gbCdnaInfo.acc = '%s') AND (gbCdnaInfo.cds !=0) AND (gbCdnaInfo.cds = cds.id)",
          acc);
    return sqlQuickQuery(conn, query, cdsBuf, cdsBufSize);
    }
}

char *getCdsForAcc(struct sqlConnection *conn, char *acc, char *cdsBuf, int cdsBufSize)
/* look up a cds, trying with and without version, and optionally dropping unique suffix */
{
char *dash = NULL;
if (gIgnoreUniqSuffix)
    {
    dash = strrchr(acc, '-');
    if (dash != NULL)
        *dash = '\0';
    }

char *cdsStr = cdsQuery(conn, acc, cdsBuf, cdsBufSize);
if (cdsStr == NULL)
    {
    int dotIdx = getGbVersionIdx(acc);
    if (dotIdx >= 0)
        {
        acc[dotIdx] = '\0';
        cdsStr = cdsQuery(conn, acc, cdsBuf, cdsBufSize);
        acc[dotIdx] = '.';
        }
    }
if (dash != NULL)
    *dash = '-';
return cdsStr;
}

struct genbankCds getCds(struct sqlConnection *conn, struct psl *psl)
/* Lookup the CDS, either in the database or hash, or generate for query.  If
 * not found and looks like a it has a genbank version, try without the
 * version.  If allCds is true, generate a cds that covers the query.  Conn
 * maybe null if gCdsTable exists or gAllCds or gNoCds are true.  If CDS can't be
 * obtained, start and end are both set to -1.  If there is an error parsing
 * it, start and end are both set to 0. */
{
struct genbankCds cds;
ZeroVar(&cds);
if (gNoCds)
    {
    cds.start = -1;
    cds.end = -1;
    cds.startComplete = FALSE;
    cds.endComplete = FALSE;
    }
else if (gAllCds)
    {
    cds.start = psl->qStart;
    cds.end = psl->qEnd;
    if (psl->strand[0] == '-')
        reverseIntRange(&cds.start, &cds.end, psl->qSize);
    cds.startComplete = TRUE;
    cds.endComplete = TRUE;
    }
else
    {
    char cdsBuf[4096];
    char *cdsStr = getCdsForAcc(conn, psl->qName, cdsBuf, sizeof(cdsBuf));
    if (cdsStr == NULL)
        {
        if (!gQuiet)
            fprintf(stderr, "Warning: no CDS for %s\n", psl->qName);
        cds.start = cds.end = -1;
        }
    else
        {
        if (!genbankCdsParse(cdsStr, &cds))
            {
            if (!gQuiet)
                fprintf(stderr, "Warning: invalid CDS for %s: %s\n",
                        psl->qName, cdsStr);
            }
        else if ((cds.end-cds.start) > psl->qSize)
            {
            if (!gQuiet)
                fprintf(stderr, "Warning: CDS for %s (%u..%u) longer than qSize (%u)\n",
                        psl->qName, cds.start, cds.end, psl->qSize);
            cds.start = cds.end = -1;
            }
        }
    }
return cds;
}

struct genePred* pslToGenePred(struct psl *psl, struct genbankCds *cds)
/* Convert a psl to genePred with specified CDS string; return NULL
 * if should be skipped.  cdsStr maybe NULL if not available. */
{
unsigned optFields = gGenePredExt ? (genePredAllFlds) : 0;

if ((cds->start == cds->end) && !(gKeepInvalid || gNoCds))
    return NULL;
if (gRequireUtr && ((cds->start == 0) || (cds->end == psl->qSize)))
    {
    if (!gQuiet)
        fprintf(stderr, "Warning: no 5' or 3' UTR for %s\n", psl->qName);
    return NULL;
    }
return genePredFromPsl3(psl, cds, optFields, gPslOptions,
                        gCdsMergeSize, gUtrMergeSize);
}

void convertPsl(struct psl *psl, struct genbankCds *cds, FILE *genePredFh)
/* convert a cds and psl and output */
{
struct genePred *genePred = pslToGenePred(psl, cds);
if (genePred != NULL)
    {
    genePredTabOut(genePred, genePredFh);
    genePredFree(&genePred);
    }
}

void convertPslTableRow(char **row, FILE *genePredFh)
/* A row from the PSL query that includes CDS */
{
struct psl *psl = pslLoad(row+1);
struct  genbankCds cds;
genbankCdsParse(row[0], &cds);
convertPsl(psl, &cds, genePredFh);
pslFree(&psl);
}

void convertPslTable(struct sqlConnection *conn, char *pslTable, FILE *genePredFh)
/* convert mrnas in a psl table to genePred objects */
{
char query[512], **row;
struct sqlResult *sr;

/* generate join of cds with psls */
safef(query, sizeof(query),
      "SELECT cds.name,matches,misMatches,repMatches,nCount,qNumInsert,qBaseInsert,tNumInsert,tBaseInsert,strand,qName,qSize,qStart,qEnd,tName,tSize,tStart,tEnd,blockCount,blockSizes,qStarts,tStarts "
      "FROM cds,%s,gbCdnaInfo WHERE (%s.qName = gbCdnaInfo.acc) AND (gbCdnaInfo.cds !=0) AND (gbCdnaInfo.cds = cds.id)",
      pslTable, pslTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    convertPslTableRow(row, genePredFh);
sqlFreeResult(&sr);
}

void convertPslFileRow(struct sqlConnection *conn, char **row, FILE *genePredFh)
/* A row from the PSL file, getting CDS */
{
struct psl *psl = pslLoad(row);
struct  genbankCds cds = getCds(conn, psl);
convertPsl(psl, &cds, genePredFh);
pslFree(&psl);
}

void convertPslFile(struct sqlConnection *conn, char *pslFile, FILE *genePredFh)
/* convert mrnas in a psl file to genePred objects */
{
struct lineFile *lf = pslFileOpen(pslFile);
char *row[PSL_NUM_COLS];

while (lineFileNextRowTab(lf, row, PSL_NUM_COLS))
    convertPslFileRow(conn, row, genePredFh);
lineFileClose(&lf);
}

void mrnaToGene(char *db, char *cdsDb, char *cdsFile, char *pslSpec,
                char *genePredFile)
/* convert an PSL mRNA table to a genePred file */
{
struct sqlConnection *conn = NULL;
FILE* genePredFh;

if (db != NULL)
    conn = sqlConnect(db);
else if (cdsDb != NULL)
    conn = sqlConnect(cdsDb);
genePredFh = mustOpen(genePredFile, "w");

if (cdsFile != NULL)
    loadCdsFile(cdsFile);

if (db == NULL)
    convertPslFile(conn, pslSpec, genePredFh);
else
    convertPslTable(conn, pslSpec, genePredFh);

if (ferror(genePredFh))
    errAbort("error writing %s", genePredFile);
carefulClose(&genePredFh);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *db, *cdsDb, *cdsFile, *pslSpec, *genePredFile;
int optCnt;

optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
pslSpec = argv[1];
genePredFile = argv[2];
db = optionVal("db", NULL);
cdsDb = optionVal("cdsDb", NULL);
cdsFile = optionVal("cdsFile", NULL);
gRequireUtr = optionExists("requireUtr");
if (optionExists("cdsMergeMod3") && !optionExists("cdsMergeSize"))
    errAbort("must specify -cdsMergeSize with -cdsMergeMod3");
if (optionExists("cdsMergeSize") || optionExists("utrMergeSize"))
    {
    gCdsMergeSize = optionInt("cdsMergeSize", -1);
    gUtrMergeSize = optionInt("utrMergeSize", -1);
    if (optionExists("cdsMergeMod3"))
        gPslOptions |= genePredPslCdsMod3;
    if (optionExists("smallInsertSize") || optionExists("insertMergeSize"))
        errAbort("can't specify -smallInsertSize or -insertMergeSize with -cdsMergeSize or -utrMergeSize");
    }
else
    {
    int insertMergeSize = genePredStdInsertMergeSize;
    if (optionExists("smallInsertSize"))
        insertMergeSize = optionInt("smallInsertSize", genePredStdInsertMergeSize);
    insertMergeSize = optionInt("insertMergeSize", genePredStdInsertMergeSize);
    gCdsMergeSize = gUtrMergeSize = insertMergeSize;
    }
gGenePredExt = optionExists("genePredExt");
gKeepInvalid = optionExists("keepInvalid");
gAllCds = optionExists("allCds");
gNoCds = optionExists("noCds");
gQuiet = optionExists("quiet");
gIgnoreUniqSuffix = optionExists("ignoreUniqSuffix");

if ((gAllCds || gNoCds) && ((cdsDb != NULL) || (cdsFile != NULL)))
    errAbort("can't specify -allCds or -noCds with -cdsDb or -cdsFile");
if (gAllCds && gRequireUtr)
    errAbort("can't specify -allCds with -requireUtr");
/* this is a bit of work to implement */
if ((gAllCds || gNoCds) && (db != NULL))
    errAbort("can't specify -allCds or -noCds with -db");

optCnt = 0;
if (db != NULL)
    optCnt++;
if (cdsDb == NULL)
    optCnt++;
if (cdsFile != NULL)
    optCnt++;
if (gAllCds)
    optCnt++;
if (gNoCds)
    optCnt++;

if (optCnt == 1)
    errAbort("must specify one and only one of -db, -cdsDb, -cdsFile, -allCds, or -noCds");

mrnaToGene(db, cdsDb, cdsFile, pslSpec, genePredFile);
return 0;
}

