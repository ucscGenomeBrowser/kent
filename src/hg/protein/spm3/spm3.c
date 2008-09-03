/* spm3 - from all mRNAs in a genome (e.g. rn3) referenced by SWISS-PROT 
          generate a list of proteins and a list of protein/mRNA pairs */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spm3 - from all mRNAs in a genome (e.g. rn3) referenced by SWISS-PROT \n"
  "       generate a list of proteins and a list of protein/mRNA pairs.\n"
  "usage:\n"
  "   spm3 xxxx yyyy\n"
  "      xxxx is the release date of SWISS-PROT data\n"
  "      yyyy is the genome underconstructino\n"
  "example: spm3 070403 rn3\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
 
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

char *accession;
char *displayID;
char *division;
char *extDB;
char *extAC;

char *proteinDataDate;
char *genomeRelease;
 
FILE   *o2, *o3;
char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd,
     *exonCount, *exonStarts, *exonEnds;

char *bioDBID, *bioentryID;

if (argc != 3) usage();
proteinDataDate = argv[1];
genomeRelease   = argv[2];
   
o2 = fopen("jj.dat", "w");
o3 = fopen("j.dat", "w");
conn2= hAllocConn(hDefaultDb());
conn3= hAllocConn(hDefaultDb());
	
sprintf(query2,"select * from %sTemp.refGene;", genomeRelease);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    name 	= row2[0];
    chrom 	= row2[1];
    strand	= row2[2];
    txStart 	= row2[3];
    txEnd   	= row2[4];
    cdsStart	= row2[5]; 
    cdsEnd	= row2[6];
    exonCount = row2[7]; 
    exonStarts= row2[8]; 
    exonEnds  = row2[9];	

    sprintf(query3, "select * from proteins%s.spXref2 where extAC='%s' and extDB='EMBL';",
    	    proteinDataDate, name);

    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	      
    while (row3 != NULL)
	{
   	accession = row3[0];
       	displayID = row3[1];	 
        division  = row3[2];  
	extDB	  = row3[3];     
	extAC	  = row3[4];
	bioentryID= row3[5];
	bioDBID	  = row3[6];

	if (! ( (strcmp(bioDBID, "1") == 0) || 
		(strcmp(bioDBID, "2") == 0) || 
		(strcmp(bioDBID, "3") == 0)
	      )
	   )
	    {
	    printf("non-recognized bioDB index %s encountered.\n", bioDBID);
	    printf("displayId=%s bioDBID=%s\n", displayID, bioDBID);
	    fflush(stdout);
	    exit(1);
	    }

	fprintf(o2, "%s\n", displayID);
	fprintf(o3, "%s\t%s\n", displayID, extAC);
	row3 = sqlNextRow(sr3);
	}
    sqlFreeResult(&sr3);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(o2);
fclose(o3);
    
system("cat j.dat |sort|uniq >proteinMrna.tab");
system("cat jj.dat|sort|uniq >protein.lis");
system("rm j.dat");
system("rm jj.dat");
return(0);
}

