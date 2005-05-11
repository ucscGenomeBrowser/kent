/* getRna - get mrna sequences  */
#include "common.h"
#include "options.h"
#include "hdb.h"
#include "jksql.h"
#include "genbank.h"
#include "linefile.h"
#include "fa.h"

static char const rcsid[] = "$Id: getRna.c,v 1.4 2005/05/11 16:44:36 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"cdsUpper", OPTION_BOOLEAN},
    {"inclVer", OPTION_BOOLEAN},
    {NULL, 0}
};

/* command line options */
boolean cdsUpper = FALSE;
boolean inclVer = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getRna - Get mrna for GenBank or RefSeq sequences found in a database\n"
  "usage:\n"
  "   getRna [options] database accFile outfa\n"
  "\n"
  "Get mrna for all accessions in accFile, writing to a fasta file. \n"
  "\n"
  "Options:\n"
  "  -cdsUpper - lookup CDS and output it as upper case. If CDS annotation\n"
  "    can't be obtained, the sequence is skipped with a warning.\n"
  "  -inclVer - include version with sequence id.\n"
  "\n");
}

char *getCdsString(struct sqlConnection *conn, char *acc)
/* get the CDS string for an accession, or null if not found */
{
char query[256];

safef(query, sizeof(query),
      "SELECT cds.name FROM gbCdnaInfo,cds WHERE (gbCdnaInfo.acc = '%s') AND (gbCdnaInfo.cds != 0) AND (gbCdnaInfo.cds = cds.id)",
      acc);
return sqlQuickString(conn, query);
}

boolean upperCaseCds(struct sqlConnection *conn, char *acc, struct dnaSeq *dna)
/* get the CDS for a sequence and make it upper case; warning and
 * return FALSE if can't obtain CDS */
{
char *cdsStr = getCdsString(conn, acc);
unsigned cdsStart, cdsEnd;

if (cdsStr == NULL)
    {
    fprintf(stderr, "%s\tno CDS defined\n", acc);
    return FALSE;
    }
if (!genbankParseCds(cdsStr, &cdsStart, &cdsEnd))
    {
    fprintf(stderr, "%s\tcan't parse CDS: %s\n", acc, cdsStr);
    free(cdsStr);
    return FALSE;
    }
if (cdsEnd > dna->size)
    {
    fprintf(stderr, "%s\tCDS exceeds mRNA length: %s\n", acc, cdsStr);
    free(cdsStr);
    return FALSE;
    }
free(cdsStr);

/* mark CDS upper case */
tolowers(dna->dna);
toUpperN(dna->dna+cdsStart, (cdsEnd-cdsStart));
return TRUE;
}

int getVersion(struct sqlConnection *conn, char *acc)
/* get version for acc, or 0 if not found */
{
char query[256];

safef(query, sizeof(query),
      "SELECT version FROM gbCdnaInfo WHERE (gbCdnaInfo.acc = '%s')",
      acc);
return  sqlQuickNum(conn, query);
}

void getAccMrna(char *acc, struct sqlConnection *conn, FILE *outFa)
/* get mrna for an accession */
{
HGID seqId;
char *faSeq;
struct dnaSeq *dna;
boolean isOk = TRUE;
char accBuf[512];

faSeq = hGetSeqAndId(conn, acc, &seqId);
if (faSeq == NULL)
    {
    fprintf(stderr, "%s\tsequence not in database\n", acc);
    return;
    }
dna = faFromMemText(faSeq);

if (cdsUpper)
    isOk = upperCaseCds(conn, acc, dna);
if (isOk && inclVer)
    {
    int ver = getVersion(conn, acc);
    safef(accBuf, sizeof(accBuf), "%s.%d", acc, ver);
    acc = accBuf;
    }

if (isOk)
    faWriteNext(outFa, acc, dna->dna, dna->size);

dnaSeqFree(&dna);
}

void getRna(char *database, char *accFile, char *outFaFile)
/* obtain mrna for a list of accessions */
{
struct sqlConnection *conn = sqlConnect(database);
struct lineFile *accLf = lineFileOpen(accFile, TRUE); 
FILE *outFa = mustOpen(outFaFile, "w"); 
char *line;
int lineSize;

hSetDb(database);

while (lineFileNext(accLf, &line, &lineSize))
    {
    getAccMrna(trimSpaces(line), conn, outFa);
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
cdsUpper = optionExists("cdsUpper");
inclVer = optionExists("inclVer");
getRna(database, accFile, outFaFile);
return 0;
}

