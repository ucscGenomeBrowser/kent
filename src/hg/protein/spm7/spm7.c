/* spm7 - Creates sorted list of mRNA-SP data file for further duplicates processing */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

char proteinName[20], mrnaName[20];
FILE *inf;
char line_in[10000];
char line[10000];
char newInfo[10000], oldInfo[10000];
char *oldMrnaStr, *oldProteinStr, *oldAlignStr;
char *mrnaStr, *proteinStr, *alignStr;

int aaLen;
int cdsLen;
char cond_str[256];
char *aaStr;

void usage()
/* Explain usage and exit. */
{   
errAbort(
  "spm7 - Create sorted list of mRNA-SP data file for further duplicates processing\n"
  "usage:\n"
  "   spm7 xxxx yyyy\n"
  "      xxxx is the release date of SWISS-PROT data\n"
  "      yyyy is the genome under construction\n"
  "example:\n"
  "   spm7 072003 hg16\n");
}
    
int main(int argc, char *argv[])
{
char *skippedKgId;
char *lastValidKgId;
    
struct sqlConnection *conn2, *conn3;
struct sqlResult *sr2;
char query2[256];
char **row2;
    
char *proteinID;
FILE   *o3, *o7;
char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd,
     *exonCount, *exonStarts, *exonEnds;

char *alignID;

char *chp;
int  i, j;

int  isDuplicate;
    
char *genomeDBname;
char *proteinDataDate;
char proteinsDB[40];
char spDB[40];
char *acc;

#define MAX_EXON 1000
int exStart[MAX_EXON], exEnd[MAX_EXON];
int exCount;

int aaStart[MAX_EXON], aaEnd[MAX_EXON];
    
char *sp, *ep;

int  aalen;
int  cdsS, cdsE;
int  eS, eE;
 
if (argc != 3) usage();
    
proteinDataDate = argv[1];
genomeDBname    = argv[2];
  
safef(spDB, sizeof(spDB), "sp%s", proteinDataDate);
safef(proteinsDB, sizeof(proteinsDB), "proteins%s", proteinDataDate);
 
o3 = fopen("j.dat", "w");
o7 = fopen("jj.dat", "w");

conn2= hAllocConn(genomeDBname);
conn3= hAllocConn(genomeDBname);
    
inf  = mustOpen("sorted.lis", "r");

strcpy(oldInfo, "");

skippedKgId   = cloneString("");
lastValidKgId = cloneString("");

isDuplicate   = 0;
oldMrnaStr    = cloneString("");
oldAlignStr   = cloneString("");
oldProteinStr = cloneString("");

mrnaStr       = cloneString("");
proteinStr    = cloneString("");
alignStr      = cloneString("");

while (fgets(line_in, 10000, inf) != NULL)
    {
    strcpy(line, line_in);

    chp = strstr(line, "\t");	/* chrom */
    chp ++;

    chp = strstr(chp, "\t");	/* cds block start position */
    chp ++;

    chp = strstr(chp, "\t");	/* cds block end   position */
    *chp = '\0';
    chp++;
    strcpy(newInfo, line);

    if (sameString(oldInfo, newInfo))
	{
	isDuplicate = 1;
	}
    else
	{
	/* remember previous record as old only if it is not a duplicate */
	if (!isDuplicate)
	    {
	    oldMrnaStr 	  = mrnaStr;
	    oldProteinStr = proteinStr;
	    oldAlignStr	  = alignStr;
	    }
	strcpy(oldInfo, newInfo);
	isDuplicate = 0;
	}

    chp = strstr(chp, "\t");	/* priority score */
    chp ++;
		
    chp = strstr(chp, "\t");	/* mRNA transcription length */ 
    chp ++;
		
    chp = strstr(chp, "\t");	/* mRNA date */
    chp ++;
	
    mrnaStr = chp;	
    chp = strstr(chp, "\t");	/* mRNA ID */
    *chp = '\0';
    chp ++;
    mrnaStr = cloneString(mrnaStr);

    proteinStr = chp;	
    chp = strstr(chp, "\t");	/* protein ID */
    *chp = '\0';
    chp ++;
    proteinStr = cloneString(proteinStr);

    alignID = chp;

    /* get rid of "end-of-line" character at the end of the string */
    alignStr = trimSpaces(alignID);

    if (isDuplicate)
	{
	/* only put out records for valid KG entries */
	if (!sameString(oldMrnaStr, skippedKgId) || sameString(oldMrnaStr, lastValidKgId))
	    {
	    fprintf(o7, "%s\t%s\t%s\t%s\n", oldMrnaStr, oldProteinStr, mrnaStr, proteinStr);
	    }
	}
    else
	{
	safef(query2, sizeof(query2), "select * from %sTemp.knownGene0 where alignID='%s';", genomeDBname, alignID);
	sr2 = sqlMustGetResult(conn2, query2);
    	row2 = sqlNextRow(sr2);
    	while (row2 != NULL)
	    {
 	    name 	= row2[0];
	    chrom 	= row2[1];
	    strand	= row2[2];
 	    txStart 	= row2[3];
	    txEnd       = row2[4];
	    cdsStart    = row2[5]; 
	    cdsEnd	= row2[6];
	    exonCount   = row2[7]; 
	    exonStarts  = row2[8]; 
	    exonEnds    = row2[9];	

	    proteinID = row2[10];
	    alignID   = row2[11];

	    sscanf(exonCount, "%d", &exCount);
	    sp = cloneString(exonStarts);
	    ep = cloneString(exonEnds);
	
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
	        if (eS != eE)
		    {
		    aaStart[j] = aalen;
		    aaEnd[j] = aaStart[j] + (eE- eS +1)/3 -1;
		    aalen = aalen + (eE- eS +1)/3;
			
		    j++;
		    }
		
		chp++;
		ep = chp;
		}
		
	    cdsLen = aalen;

            safef(cond_str, sizeof(cond_str), "val='%s'", proteinID);
            acc = sqlGetField(spDB, "displayId", "acc", cond_str);

            safef(cond_str, sizeof(cond_str), "acc='%s'", acc);
            aaStr=sqlGetField(spDB, "protein", "val", cond_str);
    	    aaLen = strlen(aaStr);

            if ((cdsLen >  50) || ((cdsLen * 100)/aaLen > 50))
		{
		fprintf(o3,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
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
			
			proteinID,
			alignID);
		lastValidKgId = cloneString(name);
		}
	    else
		{
		printf("skipping %s %d \n", name, cdsLen);
		skippedKgId = cloneString(name);
		} 
	    row2 = sqlNextRow(sr2);
	    }
	sqlFreeResult(&sr2);
	}
    }
hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(o3);
fclose(o7);
    
system("cat j.dat|sort|uniq  >knownGene.tab");
system("cat jj.dat|sort|uniq >duplicate.tab");
system("rm j.dat");
system("rm jj.dat");
return(0);
}

