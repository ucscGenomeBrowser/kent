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

static char const rcsid[] = "$Id: mrnaToGene.c,v 1.7 2003/06/15 06:48:43 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"cdsDb", OPTION_STRING},
    {"cdsFile", OPTION_STRING},
    {"requireUtr", OPTION_BOOLEAN},
    {"smallInsertSize", OPTION_INT},
    {"keepInvalid", OPTION_BOOLEAN},
    {"quiet", OPTION_BOOLEAN},
    {NULL, 0}
};

/* command line options */
static int gSmallInsertSize = 0;
static boolean gRequireUtr = FALSE;
static boolean gKeepInvalid = FALSE;
static boolean gQuiet = FALSE;

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
  "  -smallInsertSize=5 - Merge inserts smaller than this many bases (default 5)\n"
  "  -requireUtr - Drop sequences that don't have both 5' and 3' UTR annotated.\n"
  "  -keepInvalid - Keep sequences with invalid CDS.\n"
  "  -quiet - Don't print print info about dropped sequences.\n"
  "\n");
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
          "SELECT cds.name FROM cds,mrna WHERE (mrna.acc = '%s') AND (mrna.cds !=0) AND (mrna.cds = cds.id)",
          acc);
    return sqlQuickQuery(conn, query, cdsBuf, cdsBufSize);
    }
}

char *getCds(struct sqlConnection *conn, char *acc, char *cdsBuf, int cdsBufSize)
/* lookup the CDS, either in the database or hash.  If not found and looks
 * like a it has a genbank version, try without the version.  conn maybe null
 * if gCdsTable exists. */
{
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
return cdsStr;
}

struct genePred* pslToGenePred(struct psl *psl, char *cdsStr)
/* Convert a psl to genePred with specified CDS string; return NULL
 * if should be skipped.  cdsStr maybe NULL if not available. */
{
unsigned cdsStart = -1, cdsEnd = -1;

if (cdsStr == NULL)
    {
    if (!gQuiet)
        fprintf(stderr, "Warning: no CDS for %s\n", psl->qName);
    if (!gKeepInvalid)
        return NULL;
    }
else
    {
    if (!genbankParseCds(cdsStr, &cdsStart, &cdsEnd))
        {
        if (!gQuiet)
            fprintf(stderr, "Warning: invalid CDS for %s: %s\n",
                    psl->qName, cdsStr);
        if (!gKeepInvalid)
            return NULL;
        }
    else
        {
        if ((cdsEnd-cdsStart) > psl->qSize)
            {
            if (!gQuiet)
                fprintf(stderr, "Warning: CDS for %s (%u..%u) longer than qSize (%u)\n",
                        psl->qName, cdsStart, cdsEnd, psl->qSize);
            if (!gKeepInvalid)
                return NULL;
            cdsStart = -1;
            cdsEnd = -1;
            }
        if (gRequireUtr && ((cdsStart == 0) || (cdsEnd == psl->qSize)))
            {
            fprintf(stderr, "Warning: no 5' or 3' UTR for %s\n", psl->qName);
            return NULL;
            }
        }
    }
return genePredFromPsl(psl, cdsStart, cdsEnd, gSmallInsertSize);
}

void convertPslRow(char* cdsStr, char **row, FILE *genePredFh)
/* convert a row from the query of cds and psl */
{
struct psl *psl = pslLoad(row);
struct genePred *genePred = pslToGenePred(psl, cdsStr);
if (genePred != NULL)
    {
    genePredTabOut(genePred, genePredFh);
    genePredFree(&genePred);
    }
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
      "FROM cds,%s,mrna WHERE (%s.qName = mrna.acc) AND (mrna.cds !=0) AND (mrna.cds = cds.id)",
      pslTable, pslTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    convertPslRow(row[0], row+1, genePredFh);
sqlFreeResult(&sr);
}

void convertPslFileRow(struct sqlConnection *conn, char **row, FILE *genePredFh)
/* convert a psl record from a file to a genePred object */
{
char *qName = row[9];
char cdsBuf[4096];
char *cdsStr = getCds(conn, qName, cdsBuf, sizeof(cdsBuf));

convertPslRow(cdsStr, row, genePredFh);
}

void convertPslFile(struct sqlConnection *conn, char *pslFile, FILE *genePredFh)
/* convert mrnas in a psl file to genePred objects */
{
struct lineFile *lf = lineFileOpen(pslFile, TRUE);
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
int i;

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
gSmallInsertSize = optionInt("smallInsertSize", 5);
gKeepInvalid = optionExists("keepInvalid");
gQuiet = optionExists("quiet");

optCnt = 0;
if (db != NULL)
    optCnt++;
if (cdsDb == NULL)
    optCnt++;
if (cdsFile != NULL)
    optCnt++;

if (optCnt == 1)
    errAbort("must specify one and only one of -db, -cdsDb, or -cdsFile");

mrnaToGene(db, cdsDb, cdsFile, pslSpec, genePredFile);
return 0;
}

