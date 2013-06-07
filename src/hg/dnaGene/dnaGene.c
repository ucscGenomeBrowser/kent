/* dnaGene - collect DNA based RefSeq genes.  To be used by Known Genes track */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dnaGene - collect DNA based RefSeq genes.  To be used by Known Genes track.\n"
  "usage:\n"
  "   dnaGene xxxx yyyy\n"
  "      xxxx is the genome  database name\n"
  "      yyyy is the protein database name\n"
  "example: dnaGene hg15 proteins0405\n");
}

char *dbName;
char *genomeReadOnly;
char tempDbName[40];

/* check if a locusID points to a KG mRNA */
boolean checkMrna(char *locusID)
{
struct sqlConnection *conn, *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;
boolean result;
char cond_str[256];

char *chp;
char *gbAC;    
char *gbID;  
char *knownGeneID;

conn   = hAllocConn();
conn2  = hAllocConn();
result = FALSE;

sqlSafef(query2, sizeof query2, "select gbAC from %s.locus2Acc0 where locusID=%s and seqType='m';",
		tempDbName, locusID);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    gbAC  	= row2[0];
    gbID = strdup(gbAC);
	
    chp = strstr(gbID, ".");
    if (chp != NULL) *chp = '\0';

    sprintf(cond_str, "name = '%s';", gbID);
    knownGeneID = sqlGetField(dbName, "knownGene", "name", cond_str);
    if (knownGeneID != NULL)
	{
	result=TRUE;
	break;
	}
    row2 = sqlNextRow(sr2);
    }
	
hFreeConn(&conn);
hFreeConn(&conn2);
sqlFreeResult(&sr2);
return(result);
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3, *conn5;
char query2[256], query3[256], query5[256];
struct sqlResult *sr2, *sr3, *sr5;
char **row2, **row3, **row5;
char cond_str[512];

char *chp;
FILE *o1, *o2;

boolean hasKGmRNA;

char *proteinDisplayID;
char *gbAC;    
    
char *locusID;		/* LocusLink ID */
char *refAC;		/* Refseq accession.version */
char *giNCBI2;		/* NCBI gi for the protein record associated with the CDS */
char *revStatus;	/* review status */
char *proteinAC2;	/* protein accession.version */
char *taxID2;		/* tax id */

char *protDbName;

char *refSeq;

char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd,
     *exonCount, *exonStarts, *exonEnds;

char *gseq, *hseq, *swissprot;
int alignmentID=0;

if (argc != 4) usage();
    
dbName = argv[1];
protDbName = argv[2];
genomeReadOnly = argv[3];

sprintf(tempDbName, "%sTemp", dbName);

hSetDb(genomeReadOnly);

conn = hAllocConn();
conn2= hAllocConn();
conn3= hAllocConn();
conn5= hAllocConn();


o1 = fopen("dnaGene.tab", "w");
o2 = fopen("j.dat", "w");
 
// scan all RefSeq entries

sqlSafef(query2, sizeof query2, "select * from %s.locus2Ref0;", tempDbName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    locusID 	= row2[0];
    refAC 	= row2[1];
    giNCBI2 	= row2[2];
    revStatus 	= row2[3];
    proteinAC2 	= row2[4];
    taxID2 	= row2[5];
		
    refSeq = strdup(refAC);
    chp = strstr(refAC, ".");
    if (chp != NULL) *chp = '\0';

    proteinDisplayID = NULL;

    /* check if the locusID of this RefSeq points to a KG mRNA */
    hasKGmRNA = checkMrna(locusID);	

    /* check if this RefSeq has 'g' type sequence(s) referenced */
    sprintf(cond_str, "locusID=%s and seqType='g';", locusID);
    gseq = sqlGetField(tempDbName, "locus2Acc0", "gbac", cond_str);

    /* process only 'g' type record which does not have corresponding KG entry */
    if ((!hasKGmRNA) && (gseq != NULL))
	{
	sprintf(cond_str, "name='%s'", refAC);
	hseq = sqlGetField(genomeReadOnly, "refGene", "name", cond_str);
	if (hseq != NULL)
	    {
	    sprintf(cond_str, "refseq='%s';", refAC);
	    swissprot = sqlGetField(protDbName, "hugo", "swissprot", cond_str);
	    if (swissprot != NULL) 
		{
		if (strlen(swissprot) >0)	
		    {
		    // HUGO has an entry with swissprot ID, get display ID
		    sprintf(cond_str, "accession='%s';", swissprot);
		    proteinDisplayID = sqlGetField(protDbName, 
						   "spXref2", "displayID", cond_str);
		    if (proteinDisplayID == NULL) 
			{
			fprintf(stderr, "%s: a HUGO.swissprot, ", swissprot);
					fprintf(stderr, "but not a SP Primary AC.\n");
			fflush(stdout);
			}
		    }
		else
		    {
		    //printf("HGNC has a non-NULL but empty swissprot field ");
		    //printf("for %s\n", refAC);fflush(stdout);
		    }
		}

	    // not finding it in HUGO does not mean not a valid one for sure
	    if (proteinDisplayID == NULL)
		{
		// get gbAC and check if spXref2 actually has it
		sqlSafef(query3, sizeof query3, "select gbAC from %s.locus2Acc0 where locusID=%s;", 
				tempDbName, locusID);
        	sr3 = sqlMustGetResult(conn3, query3);
        	row3 = sqlNextRow(sr3);
        	
		while (row3 != NULL)
                    {
                    gbAC = row3[0];
		    chp = strstr(gbAC, ".");
		    if (chp != NULL) *chp = '\0';
		    sprintf(cond_str, "extAC='%s'", gbAC);
		    proteinDisplayID = sqlGetField(protDbName, 
						   "spXref2", "displayID", cond_str);
		    if (proteinDisplayID == NULL) 
			{
			//printf("%s %s is in refGene, but has no SWISS-PROT.\n", 
			//	locusID, refAC);
			//fflush(stdout);
			}
		    else
			{
			//printf("%s %s got 2nd chance.\n", refAC, gbAC);fflush(stdout);
			break;	
			}		
        	    row3 = sqlNextRow(sr3);
		    }
		sqlFreeResult(&sr3);
		}

	    if (proteinDisplayID != NULL)
		{
		// generate KG entry
					
                sqlSafef(query5, sizeof query5, "select * from %s.refGene where name='%s';", genomeReadOnly, refAC);
		sr5 = sqlMustGetResult(conn5, query5);
    		row5 = sqlNextRow(sr5);
    		while (row5 != NULL)
	    	    {
 	    	    name 	= row5[0];
	    	    chrom 	= row5[1];
	    	    strand	= row5[2];
 	    	    txStart 	= row5[3];
	    	    txEnd   	= row5[4];
	    	    cdsStart	= row5[5]; 
	    	    cdsEnd	= row5[6];
	    	    exonCount   = row5[7]; 
	    	    exonStarts  = row5[8]; 
	    	    exonEnds    = row5[9];	

		    fprintf(o1, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\tdna%d\n",
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
				
				proteinDisplayID,
				alignmentID);
		    alignmentID++;
		
		    fprintf(o2, "%s\t%c\t%s\n", name, 'g', proteinAC2);
		    row5 = sqlNextRow(sr5);
		    }
		sqlFreeResult(&sr5);
		}
	    }
	}
    row2 = sqlNextRow(sr2);
    }
		
fclose(o1);
fclose(o2);
sqlFreeResult(&sr2);
hFreeConn(&conn);
hFreeConn(&conn2);
hFreeConn(&conn5);
system("sort j.dat|uniq >dnaLink.tab");
system("rm j.dat");
return(0);
}

