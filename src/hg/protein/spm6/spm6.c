/* spm6 - generates sorted.lis and knownGene0.tab for further KG duplicates processing */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

#define PDB_YES        90000
#define SWISSPROT_YES   8000
#define REFSEQREVIWED    700
#define REFSEQPROVISIONAL 60

FILE   *o3, *o4, *o5;

int exStart[500], exEnd[500];
int aaStart[500], aaEnd[500];

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spm6 - generates sorted.lis and knownGene0.tab for further duplicates processing\n"
  "usage:\n"
  "   spm6 xxxx db ro_db\n"
  "      xxxx is the release date of SWISS-PROT data\n"
  "      db is the genome under construction, e.g.: kgDB\n"
  "      ro_db is the target genome to read data from\n"
  "example:\n"
  "   spm6 040115 kgDB hg16\n");
}
void getCDS(char *cdsStart, char *cdsEnd, char *exonCount, char *exonStarts_in, char *exonEnds_in)

{
int exCount;

char *sp, *ep;
char *chp;
int  i, j;
int  aalen;
int  cdsS, cdsE;
int  eS, eE;
int  estLen;
char *exonStarts, *exonEnds;

estLen = 0;
exonStarts = strdup(exonStarts_in);
exonEnds   = strdup(exonEnds_in);

sscanf(exonCount, "%d", &exCount);

sp = exonStarts;
ep = exonEnds;
sscanf(cdsStart, "%d", &cdsS);
sscanf(cdsEnd, "%d", &cdsE);
	
aalen = 0;
j=0;
for (i=0; i<exCount; i++)
    {
    chp = strstr(sp, ",");
    *chp = '\0';
    sscanf(sp, "%d", &(exStart[i]));
    chp++;
    sp = chp;

    chp = strstr(ep, ",");
    *chp = '\0';
    sscanf(ep, "%d", &(exEnd[i]));
	
    eS = exStart[i];
    eE = exEnd[i];
		
    if (cdsS > eS)
	{
	eS = cdsS;
	}
    if (cdsE < eE)
	{
	eE = cdsE;
	}
    if (eS > eE) 
	{
	eS = 0;
	eE = 0;
	}
    estLen = estLen + (eE-eS+1);
    if (eS != eE)
	{
	aaStart[j] = aalen;
	aaEnd[j] = aaStart[j] + (eE- eS +1)/3 -1;
	aalen = aalen + (eE- eS +1)/3;
			
	j++;
	}
    else
	{
	}
		
    chp++;
    ep = chp;
    }
for (i=0; i<exCount; i++)
    {
    if (exEnd[i] < cdsS)
	{
	// exon is outside of CDS at right
	// do nothing
	}
    else
	{
	if ((exStart[i] <= cdsS) && (cdsS <= exEnd[i])) 
	    {
	    fprintf(o4, "%d,", cdsS);
	    }
	else
	    {
	    if (exStart[i] <= cdsE)
		{
		fprintf(o4, "%d,", exStart[i]);
		}
	    }
	}
    }
fprintf(o4, "\t");
	
for (i=0; i<exCount; i++)
    {
    if (exEnd[i] < cdsS)
	{
	// exon is outside of CDS at right
	// do nothing
	}
    else
	{
	if ((exStart[i] <= cdsE) && (cdsE <= exEnd[i]))
	    {
	    fprintf(o4, "%d,", cdsE);
	    }
	else
	    {
	    if (cdsE > exEnd[i])
		{
		fprintf(o4, "%d,", exEnd[i]);
		}
	    }
	}
    }
fprintf(o4, "\t");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2;
char proteinName[40], mrnaName[40];
char query2[256];
char cond_str[500];

FILE *inf;
char line[1000];
int alignmentID;

struct sqlResult *sr2;
char **row2;
    
char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd,
     *exonCount, *exonStarts, *exonEnds;

char mrnaDate[500];
char *pdbID;

char *genomeDBname;
char *genomeReadOnly;
char *proteinDataDate;
char proteinsDB[40];

char *proteinDbSource;
    
int itxStart, itxEnd;
int transcriptLength;
int priority;    

bioSeq *mSeq;
HGID id;

if (argc != 4) usage();

proteinDataDate = argv[1];
genomeDBname    = argv[2];
genomeReadOnly = argv[3];

o3 = fopen("j.dat", "w");
o4 = fopen("jj.dat", "w");
o5 = fopen("align.lis", "w");
    
conn2= hAllocConn(genomeReadOnly);

inf   = mustOpen("best.lis", "r");
alignmentID = 0;

while (fgets(line, 1000, inf) != NULL)
    {
    sscanf(line, "%s\t%s", proteinName, mrnaName);

    priority = 0;
    sprintf(cond_str, "sp='%s'", proteinName);
    sprintf(proteinsDB, "proteins%s", proteinDataDate);
    pdbID= sqlGetField(proteinsDB, "pdbSP", "pdb", cond_str);
    if (pdbID != NULL)
	{
	priority = priority + PDB_YES;
	}
	
    sprintf(cond_str, "displayID='%s'", proteinName);
    proteinDbSource  = sqlGetField(proteinsDB, "spXref2", "biodatabaseID", cond_str);
    if (proteinDbSource == NULL)
	{
	printf("No proteinDbSource found for %s, skipping it ... \n", proteinName);
	break;
	}

    if (strcmp(proteinDbSource, "1") == 0)
	{
	priority = priority + SWISSPROT_YES;
	}

    // get mRNA date
    if (hRnaSeqAndIdx(mrnaName, &mSeq, &id, mrnaDate, conn2)== -1)
        {
        fprintf(stderr, "%s could not be found!!!\n", mrnaName);
        exit(1);
        }
	
    sprintf(query2, "select * from %sTemp.mrnaGene where name='%s';", genomeDBname, mrnaName);
    	
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
	{
 	name 	= row2[0];
	chrom 	= row2[1];
	strand	= row2[2];
 	txStart = row2[3];
	txEnd   = row2[4];
	cdsStart= row2[5]; 
	cdsEnd	= row2[6];
	exonCount = row2[7]; 
	exonStarts= row2[8]; 
	exonEnds  = row2[9];	

	fprintf(o4, "%s\t", chrom);
		
	getCDS(cdsStart, cdsEnd, exonCount, exonStarts, exonEnds);

	sscanf(txStart, "%d", &itxStart);
	sscanf(txEnd, "%d", &itxEnd);
	transcriptLength = itxEnd - itxStart;

	fprintf(o4, "%d\t%010d\t%s\t%s\t%s\t%d\n", 
		priority, transcriptLength, mrnaDate, name, proteinName, alignmentID);
	fprintf(o5, "%d\t%d\t%s\t%s\t%s\n", 
		alignmentID, priority,  name, proteinName, proteinDbSource);

	fprintf(o3,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\n",
 	    	name,
	    	chrom,
	    	strand,
     		txStart,
    		txEnd,
 	   	cdsStart,
    		cdsEnd,
	    	exonCount,
	    	exonStarts,
    		exonEnds,
		proteinName,
		alignmentID);
		
	alignmentID++;
	row2 = sqlNextRow(sr2);
	}
    sqlFreeResult(&sr2);
    }

hFreeConn(&conn2);
	
fclose(o3);
fclose(o4);
fclose(o5);
    
system("cat j.dat|sort|uniq >knownGene0.tab");
system("rm j.dat");
system("cat jj.dat|sort -r |uniq >sorted.lis");
system("rm jj.dat");
return(0);
}

