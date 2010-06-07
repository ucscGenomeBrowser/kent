/* gsidGetConsensus - create consensus sequence from MSA sequence data */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"

static char const rcsid[] = "$Id: gsidGetConsensus.c,v 1.2 2008/09/08 17:20:03 markd Exp $";

#define MAXSEQ 5000
#define MAXSEQLEN 5000
#define MAX_AA_BASE 21
#define MAX_DNA_BASE 5

static struct optionSpec optionSpecs[] = {
    {"seqType", OPTION_STRING},
    {"showGap", OPTION_STRING},
    {NULL, 0}
};

char seq[MAXSEQ][MAXSEQLEN];
char seqId[MAXSEQ][40];
int  cnt[MAXSEQLEN][MAX_AA_BASE];
char consensusSeq[MAXSEQLEN];
char consensusSeq2[MAXSEQLEN];

char *baseGenomeSeq;
int  seqLen;
boolean doDNA   = TRUE;
boolean showGap = FALSE;
int maxbase;

char dnaCode1[MAX_DNA_BASE][2] = {
"A",
"C",
"T",
"G",
"-"
};

/* lower case */
char dnaCode2[MAX_DNA_BASE][2] = {
"a",
"c",
"t",
"g",
"-"
};

char aaCode1[MAX_AA_BASE][2] = {
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

/* lower case AA code */
char aaCode2[MAX_AA_BASE][2] = {
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

char refBase[MAX_AA_BASE];
char refBase2[MAX_AA_BASE];

float identity[MAXSEQLEN]; 

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsidGetConsensus - create consensus sequence from MSA sequences \n"
  "usage:\n"
  "   gsidGetConsensus database seqTable faOutputFilename\n"
  "   -seqType=dna (or protein)\n"
  "   -showGap=yes oupt '-' instead of the 2nd most frequent base\n"
  "\n"
  "example: gsidGetConsensus -seqType=dna -showGap=yes hiv1 vax004Msa vax004Consensus.fa\n"
  );
}

void gsidGetConsensus(char *database, char *table, char *faOutputFile)
{
struct sqlConnection *conn2;
 
char query2[256];
struct sqlResult *sr2;
char **row2;
FILE *outf;

char base;

int ii;
int i, j, jj, k;
int seqCnt = 0;
int max, kmax, kmax2;

if (doDNA)
    {
    maxbase = MAX_DNA_BASE;
    for (i=0; i < maxbase; i++)
    	{
    	refBase[i]  = *dnaCode1[i];
    	refBase2[i] = *dnaCode2[i];
    	}
    }
else
    {
    maxbase = MAX_AA_BASE;
    for (i=0; i < maxbase; i++)
    	{
    	refBase[i]  = *aaCode1[i];
    	refBase2[i] = *aaCode2[i];
    	}
    }

conn2= hAllocConn(database);

outf = mustOpen(faOutputFile, "w");
	
sprintf(query2,"select id,seq from %s.%s", database, table);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);

/* read in all MSA sequences */
seqLen = 0;
ii=0;
while (row2 != NULL)
    {
    if (seqLen == 0) seqLen = strlen(row2[1]);
    if (strlen(row2[1]) != seqLen)
	{
	fprintf(stderr, "%s has seqLen=%d, skipping ...\n", row2[0], (int)strlen(row2[1]));
	goto skip;
	}
    strcpy(seqId[ii], row2[0]);
    strcpy(seq[ii], row2[1]);
    ii++;
skip:
    row2 = sqlNextRow(sr2);
    }

seqCnt = ii;
sqlFreeResult(&sr2);
hFreeConn(&conn2);

jj=0;
for (j=0; j<seqLen; j++)
    {
    for (k=0; k<maxbase; k++)
    	cnt[i][k] = 0;

    /* count for each base */    
    for (i=0; i<seqCnt; i++)
	{
	for (k=0; k<maxbase; k++)
	    {
	    base = toupper(seq[i][j]);
	    if (base == refBase[k]) 
		{
		cnt[j][k]++;
		}
	    }
	} 
   
    /* figure out the base with max and 2nd max count */
    max  = 0;
    kmax = 0;
    kmax2= 0;
    for (k=0; k<maxbase; k++)
	{
	if (cnt[j][k] > max) 
	    {
	    max = cnt[j][k];

	    /* keep track of the 2nd hightest */
            kmax2 = kmax;
	    kmax = k;
	    }
	}

    /* set consensus sequence base as the base with max count */
    consensusSeq[j] = refBase[kmax];
    
    /* set consensus2 sequence base as the base with 2nd max count (which would be in lower case)
       if max is '-' */
    if (refBase[kmax] == '-')
        {
        consensusSeq2[j] = refBase2[kmax2];
        }
    else
        {
        consensusSeq2[j] = refBase[kmax];
        }

    /* logic to generate conservation wiggle */
    /* TBD */
    /* 
    identity[j] = (float)max/(float)seqCnt;
    if (baseGenomeSeq[j] != '-')
	{
        // protein takes up 3 bases for each AA 
	//fprintf(outf, "chr1 %d %d %f\n",startPos+jj*3-1, startPos+jj*3+2, identity[j]);
	jj++;
	}
    */
    }

consensusSeq[seqLen] = '\0';
consensusSeq2[seqLen] = '\0';
fprintf(outf, ">%s\n", table);

if (showGap)
    {
    fprintf(outf, "%s\n", consensusSeq);
    }
else
    {
    fprintf(outf, "%s\n", consensusSeq2);
    }

fclose(outf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int ia;
char *database, *table;
char *faOutputFilename;
char *seqType;

optionInit(&argc, argv, optionSpecs);

seqType = optionVal("seqType", "dna");
if (sameWord(seqType, "protein")) doDNA = FALSE;
if (sameWord(seqType, "dna"))     doDNA = TRUE;

if (sameWord(optionVal("showGap", "no"), "yes")) showGap = TRUE;

if (argc != 4) usage();

ia=1;
database 	= argv[ia];
ia++;
table  		= argv[ia];
ia++;
faOutputFilename= argv[ia];

gsidGetConsensus(database, table, faOutputFilename);
return 0;
}

