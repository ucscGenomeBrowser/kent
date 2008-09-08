/* gsidAaMsa - create conservation wiggle track for GSID protein MSA */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"

static char const rcsid[] = "$Id: gsidAaMsa.c,v 1.3 2008/09/08 17:20:03 markd Exp $";

#define MAXSEQ 5000
#define MAXSEQLEN 5000
#define MAXBASE 21

char seq[MAXSEQ][MAXSEQLEN];
char seqId[MAXSEQ][40];
int  cnt[MAXSEQLEN][MAXBASE];
char consensusSeq[MAXSEQLEN];
char consensusSeq2[MAXSEQLEN];

char *baseGenomeSeq;
int  baseSeqLen;

char aaCode1[MAXBASE][2] = {
"A",
"V",
"L",
"I",
"P",
"F",
"W",
"M",
"G",
"S",
"T",
"C",
"Y",
"N",
"Q",
"D",
"E",
"K",
"R",
"H",
"-"
};

char aaCode2[MAXBASE][2] = {
"a",
"v",
"l",
"i",
"p",
"f",
"w",
"m",
"g",
"s",
"t",
"c",
"y",
"n",
"q",
"d",
"e",
"k",
"r",
"h",
"-"
};

char refBase[MAXBASE];
char refBase2[MAXBASE];
char aliSeq[MAXSEQLEN];
float identity[MAXSEQLEN]; 

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsidAaMsa - create conservation wiggle track for GSID protein MSA \n"
  "usage:\n"
  "   gsidAaMsa database table baseAcc startPos wigOutput concensusOutput\n"
  "\n"
  "example: gsidAaMsa hiv1 vax04AliSeq HXB2 6348 vax04Msa.wig vax04Consnsus.fa\n"
  );
}

void gsidAaMsa(char *database, char *table, char *baseAcc, int startPos, 
char *outWigFn, char *outConsFn)
{
struct sqlConnection *conn2;
 
char query2[256];
struct sqlResult *sr2;
char **row2;
FILE *outf, *outf2;

char base;

int ii;
int i, j, jj, k;
int seqCnt = 0;
int max, kmax, kmax2;

for (i=0; i < MAXBASE; i++)
    {
    refBase[i]  = *aaCode1[i];
    refBase2[i] = *aaCode2[i];
    }

conn2= hAllocConn(database);

outf = mustOpen(outWigFn, "w");
	
sprintf(query2,"select seq from %s.%s where id='%s'", database, table, baseAcc);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
baseGenomeSeq = cloneString(row2[0]);
baseSeqLen=strlen(baseGenomeSeq);
sqlFreeResult(&sr2);

sprintf(query2,"select * from %s.%s", database, table);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);

ii=0;
while (row2 != NULL)
    {
    strcpy(seqId[ii], row2[0]);
    strcpy(seq[ii], row2[1]);
    
    ii++;
    row2 = sqlNextRow(sr2);
    }

seqCnt = ii;
sqlFreeResult(&sr2);
hFreeConn(&conn2);

/* print header */
fprintf(outf, "browser position chr1:1-9000\n");
fprintf(outf, "track type=wiggle_0\n");

jj=0;
for (j=0; j<baseSeqLen; j++)
    {
    for (k=0; k<MAXBASE; k++)
    	cnt[i][k] = 0;
    
    for (i=0; i<seqCnt; i++)
	{
	for (k=0; k<MAXBASE; k++)
	    {
	    base = toupper(seq[i][j]);
	    if (base == refBase[k]) 
		{
		cnt[j][k]++;
		}
	    }
	} 
    max  = 0;
    kmax = 0;
    kmax2= 0;
    for (k=0; k<MAXBASE; k++)
	{
	if (cnt[j][k] > max) 
	    {
	    max = cnt[j][k];

	    /* keep track of the 2nd hightest */
            kmax2 = kmax;
	    kmax = k;
	    }
	}
    consensusSeq[j] = refBase[kmax];
    if (refBase[kmax] == '-')
        {
        consensusSeq2[j] = refBase2[kmax2];
        }
    else
        {
        consensusSeq2[j] = refBase[kmax];
        }

    aliSeq[j] = refBase[kmax];
    identity[j] = (float)max/(float)seqCnt;
    if (baseGenomeSeq[j] != '-')
	{
        /* protein takes up 3 bases for each AA */
	fprintf(outf, "chr1 %d %d %f\n",startPos+jj*3-1, startPos+jj*3+2, identity[j]);
	jj++;
	}
    }

fclose(outf);

consensusSeq[baseSeqLen] = '\0';
consensusSeq2[baseSeqLen] = '\0';
outf2 = mustOpen(outConsFn, "w");
fprintf(outf2, ">%s Protein MSA Consensus Sequence\n", table);
fprintf(outf2, "%s\n", consensusSeq2);
fclose(outf2);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int ia;
char *database, *table;
char *baseAcc;
int startPos;
char *outFn, *outConsFn;

if (argc != 7) usage();

ia=1;
database 	= argv[ia];
ia++;
table  		= argv[ia];
ia++;
baseAcc 	= argv[ia];
ia++;
startPos	= atoi(argv[ia]);
ia++;
outFn 		= argv[ia];
ia++;
outConsFn 	= argv[ia];

gsidAaMsa(database, table, baseAcc, startPos, outFn, outConsFn);

return 0;
}

