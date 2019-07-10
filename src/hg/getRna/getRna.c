/* getRna - get mrna sequences  */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "hdb.h"
#include "jksql.h"
#include "genbank.h"
#include "linefile.h"
#include "fa.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"cdsUpper", OPTION_BOOLEAN},
    {"cdsUpperAll", OPTION_BOOLEAN},
    {"inclVer", OPTION_BOOLEAN},
    {"peptides", OPTION_BOOLEAN},
    {"seqTbl", OPTION_STRING},
    {"extFileTbl", OPTION_STRING},
    {NULL, 0}
};

/* command line options */
static boolean cdsUpper = FALSE;
static boolean cdsUpperAll = FALSE;
static boolean inclVer = FALSE;
static boolean peptides = FALSE;
char *seqTbl = NULL;
char *extFileTbl = NULL;

/* derived from command line, it clearer as -cdsUpperAll and -peptides defines
 * multiple behaviors */
boolean warnOnNoCds = FALSE;
boolean skipNoCds = FALSE;

static int errCnt = 0;

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "getRna - Get mrna for GenBank or RefSeq sequences found in a database\n"
  "usage:\n"
  "   getRna [options] database accFile outfa\n"
  "\n"
  "Get mrna for all accessions in accFile, writing to a fasta file. If accession\n"
  " has a version, that version is returned or an error generated\n"
  "\n"
  "Options:\n"
  "  -cdsUpper - lookup CDS and output it as upper case. If CDS annotation\n"
  "    can't be obtained, the sequence is skipped with a warning.\n"
  "  -cdsUpperAll - like -cdsUpper, except keep sequeneces without CDS\n"
  "  -inclVer - include version with sequence id.\n"
  "  -peptides - translate mRNAs to peptides\n"
  "  -seqTbl=tbl - use this table instead of gbSeq and seq. Many other options don't work if this is used.\n"
  "  -extFileTbl=tbl - use this table instead of gbExtFile and extFile\n"
  "\n");
}

static void parseAccVersion(char* requestedAcc,
                            char acc[GENBANK_ACC_BUFSZ],
                            char ver[GENBANK_ACC_BUFSZ])
/* parse accession and optional version */
{
char* verDot = strchr(requestedAcc, '.');
if (verDot != NULL)
    {
    genbankDropVer(acc, requestedAcc);
    safecpy(ver, GENBANK_ACC_BUFSZ, verDot+1);
    }
else
    {
    safecpy(acc, GENBANK_ACC_BUFSZ, requestedAcc);
    ver[0] = '\0';
    }
}

static struct dnaSeq* getSeq(struct sqlConnection *conn, char* acc)
/* get sequence from fasta file */
{
HGID seqId;
char *faSeq = hGetSeqAndId(conn, acc, &seqId);
if (faSeq == NULL)
    {
    fprintf(stderr, "%s\tsequence not in database\n", acc);
    errCnt++;
    return NULL;
    }
return faFromMemText(faSeq);
}

static char *getCdsString(struct sqlConnection *conn, char *acc)
/* get the CDS string for an accession, or null if not found */
{
char query[256];

sqlSafef(query, sizeof(query),
      "SELECT cds.name FROM gbCdnaInfo,cds WHERE (gbCdnaInfo.acc = '%s') AND (gbCdnaInfo.cds != 0) AND (gbCdnaInfo.cds = cds.id)",
      acc);
return sqlQuickString(conn, query);
}

static boolean getCds(struct sqlConnection *conn, char *acc, int mrnaLen,
                      struct genbankCds *cds)
/* get the CDS range for an mRNA, warning and return FALSE if can't obtain
 * CDS or it is longer than mRNA. */
{
char *cdsStr = getCdsString(conn, acc);

if (cdsStr == NULL)
    {
    if (warnOnNoCds)
        fprintf(stderr, "%s\tno CDS defined\n", acc);
    return FALSE;
    }
if (!genbankCdsParse(cdsStr, cds))
    {
    if (warnOnNoCds)
        fprintf(stderr, "%s\tcan't parse CDS: %s\n", acc, cdsStr);
    free(cdsStr);
    return FALSE;
    }
if (cds->end > mrnaLen)
    {
    if (warnOnNoCds)
        fprintf(stderr, "%s\tCDS exceeds mRNA length: %s\n", acc, cdsStr);
    free(cdsStr);
    return FALSE;
    }
free(cdsStr);
return TRUE;
}

static void upperCaseCds(struct dnaSeq *dna, struct genbankCds *cds)
/* uppercase the CDNS */
{
tolowers(dna->dna);
toUpperN(dna->dna+cds->start, (cds->end-cds->start));
}

int getVersion(struct sqlConnection *conn, char *acc)
/* get version for acc, or 0 if not found */
{
char query[256];

sqlSafef(query, sizeof(query),
      "SELECT version FROM gbCdnaInfo WHERE (gbCdnaInfo.acc = '%s')",
      acc);
return sqlQuickNum(conn, query);
}

static void processVersion(struct sqlConnection *conn, char acc[GENBANK_ACC_BUFSZ],
                           char ver[GENBANK_ACC_BUFSZ])
/* If the acc has a version, check that the correct version was included.  If
 * a version is requested in the fasta, update acc argument. */
{
int dbVerNum = getVersion(conn, acc);
char dbVerStr[32];
safef(dbVerStr, sizeof(dbVerStr), "%d", dbVerNum);
if ((strlen(ver) > 0) && !sameString(dbVerStr, ver))
    {
    fprintf(stderr, "%s.%s requested, database has version %s\n", acc, ver, dbVerStr);
    errCnt++;
    }
if (inclVer)
    {
    char accTmp[GENBANK_ACC_BUFSZ];
    safef(accTmp, sizeof(accTmp), "%s.%d", acc, dbVerNum);
    safecpy(acc, GENBANK_ACC_BUFSZ, accTmp);
    }
}


static void writePeptide(FILE *outFa, char *acc, struct dnaSeq *dna, struct genbankCds *cds)
/* translate the sequence to a peptide and output */
{
char *pep = needMem(dna->size); /* more than needed */
char hold = dna->dna[cds->end];
dna->dna[cds->end] = '\0';
dnaTranslateSome(dna->dna+cds->start, pep, dna->size);
dna->dna[cds->end] = hold;
faWriteNext(outFa, acc, pep, strlen(pep));
freeMem(pep);
}

static void getAccMrna(char *requestedAcc, struct sqlConnection *conn, FILE *outFa)
/* get mrna for an accession */
{
char acc[GENBANK_ACC_BUFSZ];
char ver[GENBANK_ACC_BUFSZ];
boolean cdsOk = TRUE;
struct genbankCds cds;

parseAccVersion(requestedAcc, acc, ver);

struct dnaSeq *dna = getSeq(conn, acc);
if (dna == NULL)
    return;  

if (cdsUpper || peptides)
    {
    cdsOk = getCds(conn, acc, dna->size, &cds);
    if ((!cdsOk) && skipNoCds)
        {
        dnaSeqFree(&dna);
        return;
        }
    }

if (cdsOk && cdsUpper)
    upperCaseCds(dna, &cds);
if (inclVer || (strlen(ver) > 0))
    processVersion(conn, acc, ver);

if (peptides)
    writePeptide(outFa, acc, dna, &cds);
else
    faWriteNext(outFa, acc, dna->dna, dna->size);

dnaSeqFree(&dna);
}

static void getAccSeqTable(char *acc, struct sqlConnection *conn, FILE *outFa)
/* get mrna for an accession from a seqTable */
{
struct dnaSeq *dna =  hDnaSeqMustGetConn(conn, acc, seqTbl, extFileTbl);
faWriteNext(outFa, acc, dna->dna, dna->size);
dnaSeqFree(&dna);
}

static void getRna(char *database, char *accFile, char *outFaFile)
/* obtain mrna for a list of accessions */
{
struct sqlConnection *conn = sqlConnect(database);
struct lineFile *accLf = lineFileOpen(accFile, TRUE); 
FILE *outFa = mustOpen(outFaFile, "w"); 
char *line;
int lineSize;

while (lineFileNext(accLf, &line, &lineSize))
    {
    if (seqTbl == NULL)
        getAccMrna(trimSpaces(line), conn, outFa);
    else
        getAccSeqTable(trimSpaces(line), conn, outFa);
    }

if (ferror(outFa))
    errAbort("error writing %s", outFaFile);
carefulClose(&outFa);
lineFileClose(&accLf);
sqlDisconnect(&conn);

}

int main(int argc, char *argv[])
/* Process command line. */
{
char *database, *accFile, *outFaFile;

optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
database = argv[1];
accFile = argv[2];
outFaFile = argv[3];
if ((optionExists("seqTbl") && !optionExists("extFileTbl"))
    || (!optionExists("seqTbl") && optionExists("extFileTbl")))
    errAbort("must specified both or neither of -seqTbl and -extFileTbl");
seqTbl = optionVal("seqTbl", seqTbl);
extFileTbl = optionVal("extFileTbl", extFileTbl);

cdsUpper = optionExists("cdsUpper");
cdsUpperAll = optionExists("cdsUpperAll");
if ((seqTbl != NULL) && (cdsUpper || cdsUpperAll))
    errAbort("-cdsUpper and -cdsUpperAll not support with -seqTbl");
warnOnNoCds = !cdsUpperAll;
skipNoCds = cdsUpper;
if (cdsUpperAll)
    cdsUpper = TRUE;
inclVer = optionExists("inclVer");
if ((seqTbl != NULL) && inclVer)
    errAbort("-inclVer not support with -seqTbl, version is always included");
peptides = optionExists("peptides");
if ((seqTbl != NULL) && peptides)
    errAbort("-peptides not support with -seqTbl");
if (peptides)
    skipNoCds = TRUE;
if (peptides && (cdsUpper || cdsUpperAll))
    errAbort("can't specify -peptides with -cdsUpper or -cdsUpperAll");
getRna(database, accFile, outFaFile);
return (errCnt == 0) ? 0 : 1;
}

