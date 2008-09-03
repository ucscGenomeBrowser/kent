/* dv2 - build dvBed table */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

/* global counter used to collect stats */
int  np0, np1, np2, nn0, nn1, nn2, nn, np = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dv - build dvBed table\n"
  "usage:\n"
  "   dv protDb hgDb outTabfile\n"
  "      protDb is the UniProt DB, e.g. unitProt\n"
  "      hgDb is the genome DB, e.g. hg18\n"
  "      outTabfile is the output, e.g. dvBed.tab\n"
  "example: dv uniProt hg18 dvBed.tab\n");
}

void processVariant(char *database, char *proteinID, int aaSeqLen, int varStart, int varLen, 
		    char *origSeq,  char *varSeq, char *varId, FILE *outf)
{
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection  *conn;

char *qNameStr;
char *qSizeStr;
char *qStartStr;
char *qEndStr;
char *tNameStr=NULL;
char *tSizeStr;
char *tStartStr;
char *tEndStr;
char *blockCountStr;
char *blockSizesStr;
char *qStartsStr;
char *tStartsStr;

char *strand       = NULL;
int blockCount=0;
char *exonStartStr = NULL;
int exonGenomeStartPos, exonGenomeEndPos;
char *exonGenomeStartStr = NULL;
char *chp, *chp2, *chp3;
char *exonSizeStr  = NULL;
int j;

int exonStart, exonEnd;
int lastStart;
int lastLen;
int accumLen;
int exonLen;
int dvStart;
int varEnd;
struct dnaSeq *dnaseq;

conn= hAllocConn();
/* NOTE: the query below may not always return single answer, */
/* and kgProtMap and knownGene alignments may not be identical, so pick the closest one. */

safef(query,sizeof(query), "select qName, qSize, qStart, qEnd, tName, tSize, tStart, tEnd, blockCount, blockSizes, qStarts, tStarts, strand from %s.%s where qName='%s';",
        database, "kgProtMap", proteinID);
sr  = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

if (row == NULL)
    {
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    return;
    }

while (row != NULL)
    {
    qNameStr        = cloneString(row[0]);
    qSizeStr        = cloneString(row[1]);
    qStartStr       = cloneString(row[2]);
    qEndStr         = cloneString(row[3]);
    tNameStr        = cloneString(row[4]);
    tSizeStr        = cloneString(row[5]);
    tStartStr       = cloneString(row[6]);
    tEndStr         = cloneString(row[7]);
    blockCountStr   = cloneString(row[8]);
    blockSizesStr   = cloneString(row[9]);
    qStartsStr      = cloneString(row[10]);
    tStartsStr      = cloneString(row[11]);
    strand          = cloneString(row[12]);

    blockCount 	   = atoi(blockCountStr);

if (sameWord(strand, "+"))
    {
    exonStartStr   = qStartsStr;
    exonGenomeStartStr = tStartsStr;
    exonSizeStr    = blockSizesStr;
    
    accumLen = 0;
    lastStart = 0;
    chp2 = exonGenomeStartStr;
    chp3 = blockSizesStr;
    for (j=0; j< blockCount; j++)
    	{
	chp = strstr(chp2, ",");
	*chp = '\0';
	exonStart = atoi(chp2);
	chp2 = chp+1;
	
	chp = strstr(chp3, ",");
	*chp = '\0';
	exonLen = atoi(chp3);
	chp3 = chp + 1;
	
	if (((accumLen + exonLen)/3) >= varStart)
	    {
	    dvStart = exonStart + (varStart - accumLen/3)*3;
	    np++;
	    dnaseq = hDnaFromSeq(tNameStr, dvStart, dvStart+varLen*3, dnaUpper); 
	    
	    /* check if first AA of variant agrees with genomic codon */
	    if (lookupCodon(dnaseq->dna) == origSeq[0]) 
		{
		np0++;
	        fprintf(outf, "%s\t%d\t%d\t%s\n", tNameStr, dvStart, dvStart+varLen*3, varId);
		}
	    else
		{
	        /* try next position */
		dvStart = exonStart + (varStart - accumLen/3)*3 - 1;
		
		dnaseq = hDnaFromSeq(tNameStr, dvStart, dvStart+varLen*3, dnaUpper); 
		if (lookupCodon(dnaseq->dna) == origSeq[0]) 
			{
			np1++;
	        	fprintf(outf, "%s\t%d\t%d\t%s\n", 
				tNameStr, dvStart, dvStart+varLen*3, varId);
			}
		else
			{
			/* could further scan wider range for valid AA mapping */
			}
     		}
	    break;
	    }
	    
	lastStart = exonStart;
	accumLen = accumLen + exonLen;
	}
    }
else
    {
    exonStartStr   = qStartsStr;
    exonGenomeStartStr = tStartsStr;
    exonSizeStr    = blockSizesStr;

    varEnd = aaSeqLen - varStart - varLen;
    
    accumLen = 0;
    lastStart = 0;
    chp2 = exonGenomeStartStr;
    chp3 = blockSizesStr;
    for (j=0; j< blockCount; j++)
    	{
	chp = strstr(chp2, ",");
	*chp = '\0';
	exonStart = atoi(chp2);
	chp2 = chp+1;
	
	chp = strstr(chp3, ",");
	*chp = '\0';
	exonLen = atoi(chp3);
	chp3 = chp + 1;

	if (((accumLen + exonLen)/3) >= varEnd)
	    {
	    nn++;
	    dvStart =  exonStart + (varEnd - accumLen/3)*3;
	
	    dnaseq = hDnaFromSeq(tNameStr, dvStart, dvStart+varLen*3, dnaUpper); 
	    reverseComplement(dnaseq->dna, (long)3);
  	    if (lookupCodon(dnaseq->dna) == origSeq[0])
            	{
		nn0++;
	    	fprintf(outf, "%s\t%d\t%d\t%s\n", tNameStr, dvStart, dvStart+varLen*3, varId);
	    	}
	    else
	    	{
	    	dvStart =  exonStart + (varEnd - accumLen/3)*3 - 1;
	
	    	dnaseq = hDnaFromSeq(tNameStr, dvStart, dvStart+varLen*3, dnaUpper); 
	    	reverseComplement(dnaseq->dna, (long)3);
  	    	if (lookupCodon(dnaseq->dna) == origSeq[0])
            	    {
		    nn1++;

	    	    fprintf(outf, "%s\t%d\t%d\t%s\n", tNameStr, dvStart, dvStart+varLen*3, varId);
		    }
		else
		    {
		    /* deal with this later */
		    }
		}
	    
	    fflush(outf);
	    break;
	    }
	    
	lastStart = exonStart;
	accumLen = accumLen + exonLen;
	}
    }

    row = sqlNextRow(sr);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
  
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;
char *proteinDb;
char *database;
char *acc, *start, *end;

char *accession;
char *displayID;
char *division;
char *extDB;
char *extAC;

char *proteinDataDate;
char *genomeRelease;
int  i;

char *swissID, *pdb;
char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd,
     *exonCount, *exonStarts, *exonEnds;
int iStart;
char *varId;

char *aaSeq;
char *lenStr;
int  varLen;
char *orig;
int aaSeqLen;
char *outFileName;
char baseAa, variantAa;
char *origSeq;
char *varSeq;
FILE   *outf;

if (argc != 4) usage();
proteinDb = argv[1];
database  = argv[2];
outFileName   = argv[3];

hSetDb(database);
outf = mustOpen(outFileName, "w");
conn2 = sqlConnect(proteinDb);
conn3 = sqlConnect(proteinDb);
	
sprintf(query2,"select * from %s.dv", database);

sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    acc 	= row2[0];
    start 	= row2[1];
    lenStr 	= row2[2];
    origSeq 	= row2[3];
    varSeq      = row2[4];
    varId       = row2[5];
    
    iStart = atoi(start);
    varLen = atoi(lenStr);
   
    /* get protein sequence */
    sprintf(query3, "select val from %s.protein where acc='%s'", proteinDb, acc);
    sr3      = sqlMustGetResult(conn3, query3);
    row3     = sqlNextRow(sr3);
    aaSeq    = row3[0];
    aaSeqLen = strlen(aaSeq);
    
    if (strlen(varSeq) >= 255) 
	{
        fprintf(stderr,"varSeq longer than 255 AA: \t%s\t%s\n", varSeq, varId);
	*(varSeq+255) = '\0';
	}

    processVariant(database, acc, aaSeqLen, iStart, varLen, origSeq, varSeq, varId, outf); 
    sqlFreeResult(&sr3);
    
    row2 = sqlNextRow(sr2);
    }
    
sqlFreeResult(&sr2);
fclose(outf);
fflush(stdout);

printf("np=%d np0=%d np1=%d not covered = %d\n", np, np0, np1, np - np0 - np1);fflush(stdout);
printf("nn=%d nn0=%d nn1=%d not covered = %d\n", nn, nn0, nn1, nn - nn0 - nn1);fflush(stdout);
return(0);
}

