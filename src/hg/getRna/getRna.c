/* getRna - get mrna sequences  */
#include "common.h"
#include "options.h"
#include "hdb.h"
#include "jksql.h"
#include "genbank.h"
#include "linefile.h"
#include "fa.h"

static char const rcsid[] = "$Id: getRna.c,v 1.7 2008/09/03 19:18:41 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"cdsUpper", OPTION_BOOLEAN},
    {"cdsUpperAll", OPTION_BOOLEAN},
    {"inclVer", OPTION_BOOLEAN},
    {"peptides", OPTION_BOOLEAN},
    {NULL, 0}
};

/* command line options */
boolean cdsUpper = FALSE;
boolean cdsUpperAll = FALSE;
boolean inclVer = FALSE;
boolean peptides = FALSE;

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
  "  -cdsUpperAll - like -cdsUpper, except keep sequeneces without CDS\n"
  "  -inclVer - include version with sequence id.\n"
  "  -peptides - translate mRNAs to peptides\n"
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

boolean getCds(struct sqlConnection *conn, char *acc, int mrnaLen,
               boolean warnOnNoCds, struct genbankCds *cds)
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

void upperCaseCds(struct dnaSeq *dna, struct genbankCds *cds)
/* uppercase the CDNS */
{
tolowers(dna->dna);
toUpperN(dna->dna+cds->start, (cds->end-cds->start));
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

void writePeptide(FILE *outFa, char *acc, struct dnaSeq *dna, struct genbankCds *cds)
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

void getAccMrna(char *acc, struct sqlConnection *conn, FILE *outFa)
/* get mrna for an accession */
{
HGID seqId;
char *faSeq;
struct dnaSeq *dna;
boolean cdsOk = TRUE;
char accBuf[512];
struct genbankCds cds;

faSeq = hGetSeqAndId(conn, acc, &seqId);
if (faSeq == NULL)
    {
    fprintf(stderr, "%s\tsequence not in database\n", acc);
    return;
    }
dna = faFromMemText(faSeq);

if (cdsUpper || peptides)
    cdsOk = getCds(conn, acc, dna->size, !cdsUpperAll, &cds);

if (cdsOk && cdsUpper)
    upperCaseCds(dna, &cds);
if ((cdsOk || cdsUpperAll) && inclVer)
    {
    int ver = getVersion(conn, acc);
    safef(accBuf, sizeof(accBuf), "%s.%d", acc, ver);
    acc = accBuf;
    }

if ((cdsOk || cdsUpperAll))
    {
    if (peptides)
        writePeptide(outFa, acc, dna, &cds);
    else
        faWriteNext(outFa, acc, dna->dna, dna->size);
    }

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
cdsUpperAll = optionExists("cdsUpperAll");
if (cdsUpperAll)
    cdsUpper = TRUE;
inclVer = optionExists("inclVer");
peptides = optionExists("peptides");
if (peptides && (cdsUpper || cdsUpperAll))
    errAbort("can't specify -peptides with -cdsUpper or -cdsUpperAll");
getRna(database, accFile, outFaFile);
return 0;
}

