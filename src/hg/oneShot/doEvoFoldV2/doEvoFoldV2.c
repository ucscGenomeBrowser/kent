/* doEvoFoldV2 - This is a one shot program used for the EvoFoldV2 track build */
#include "common.h"
#include "hCommon.h"
#include "dnautil.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doEvoFoldV2 - This program builds Java command lines needed to generate .png files of EvoFoldV2 RNA structures.\n"
  "usage:\n"
  "   doEvoFoldV2 db outFileName chrom\n"
  "      db is the database name\n"
  "      outFileName is the filename of the output file, which consists of Java command lines\n"
  "      chrom is the target chromosome name\n"
  "example: doEvoFoldV2 hg19 dochr22 chr22\n");
}

char *avId, *omimId;
FILE *outf;
char *tgtChrom;

char *secStr, *id, *chrom;
int  chromStart, chromEnd;
char strand;
char javaCmd[4096];

int main(int argc, char *argv[])
{
char *database;
char *outFn;
struct dnaSeq *seq;

struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;

if (argc != 4) usage();

database = argv[1];
conn2= hAllocConn(database);

outFn   = argv[2];
outf    = mustOpen(outFn, "w");

tgtChrom = argv[3];

sprintf(query2,"select secStr, name, chrom, chromStart, chromEnd, strand from evofoldV2 where chrom='%s'", tgtChrom);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    secStr   	= row2[0];
    id  	= row2[1];
    chrom 	= row2[2];
    chromStart 	= atoi(row2[3]);
    chromEnd   	= atoi(row2[4]);
    strand     	= *row2[5];
    seq = hChromSeq(database, chrom, chromStart, chromEnd);
    touppers(seq->dna);
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);

    memSwapChar(seq->dna, seq->size, 'T', 'U');

    safef(javaCmd, sizeof(javaCmd),
       "java -cp VARNAv3-7.jar fr.orsay.lri.varna.applications.VARNAcmd -sequenceDBN %s -structureDBN '%s' -o evoFoldV2/%s/%s.png",
          seq->dna,  secStr, chrom, id);
    
    fprintf(outf, "%s\n", javaCmd);

    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(outf);
hFreeConn(&conn2);
return(0);
}
