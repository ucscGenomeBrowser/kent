/* kgPick - select the best repersentative mRNA/protein pair from KG candidates 
   having identical CDS Structure */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

char printedMrna[40], printedProt[40], printedCds[40];

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgPick - select the best repersentative mRNA/protein pair\n"
  "usage:\n"
  "   kgPick tempDb outfile\n"
  "      tempDb is the temp KG DB name\n"
  "      roDb   is the read only genome DB name\n"
  "      spDb   is the proteinsYYMMDDt DB name\n"
  "      outfile is the output file name\n"
  "      dupOutfile is the duplicate mrna/prot output file name\n"
  "example: kgPick kgHg17ETemp hg17 proteins050415 kg2.tmp dupSpMrna.tmp\n");
}

/* convert a possible compund mrnaID ("protID_mrnaID") to just the mrnaID. */
char *convert(char *compound)
{
char *chp;
    
chp = compound;
if ((*chp == 'N') && (*(chp+1) == 'M') && (*(chp+2) == '_'))
    {
    return compound;
    }
else
    {
    chp = strstr(compound, "_");
    if (chp == NULL) return compound;

    chp ++;
    }
return chp;
}	
    
void printKg(struct sqlConnection *conn, struct sqlConnection *conn2, 
             char *kgTempDb, char *spDb, char *cdsId, char *mrnaID, char *protAcc, 
	     char *alignID, FILE *outf)
{
char query[256];
struct sqlResult *sr;
char **row;
int i;
char condStr[256];
char *displayId;

safef(query, sizeof(query), "select * from %s.kgCandidate where alignID='%s'", kgTempDb, alignID);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
while (row != NULL)
    {
    fprintf(outf, "%s\t", convert(row[0]));
    for (i=1; i<10; i++) 
    	{
	fprintf(outf, "%s\t", row[i]);
	}
    
    /* get protein display ID */
    sprintf(condStr, "acc='%s'", protAcc);
    displayId = sqlGetField(spDb, "displayId", "val", condStr);
    if (displayId == NULL) displayId = protAcc;
    
    fprintf(outf, "%s\t%s\t%s\n", displayId, alignID, cdsId);
    
    /* remember what have been printed */
    safef(printedCds, sizeof(printedCds), cdsId);
    safef(printedMrna, sizeof(printedCds), row[0]);
    safef(printedProt, sizeof(printedProt), displayId);
    row = sqlNextRow(sr);
    }
sqlFreeResult(&sr);
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3, *conn4, *conn5;
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;
char condStr[256];
char *kgTempDb;
char *outfileName, *dupOutfileName;
FILE *outf, *dupOutf;
char *score;
char *protAcc;
char *ranking;
char *cdsId, *mrnaID, *alignID;
char *protDbId;
char *spDb;
boolean gotRefseq, gotNonRef;
boolean isRefseq;
int printCnt;
char *roDb;
char *refseqId;

if (argc != 6) usage();
kgTempDb       = argv[1];
roDb           = argv[2];
spDb           = argv[3];
outfileName    = argv[4];
dupOutfileName = argv[5];

outf    = mustOpen(outfileName, "w");
dupOutf = mustOpen(dupOutfileName, "w");

conn2= hAllocConn(roDb);
conn3= hAllocConn(roDb);
conn4= hAllocConn(roDb);
conn5= hAllocConn(roDb);

strcpy(printedCds, "");
strcpy(printedMrna, "");
strcpy(printedProt, "");

/* go through each unique cds structure */
safef(query2, sizeof(query2), "select distinct cdsId from %s.kgCandidateZ", kgTempDb);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);

while (row2 != NULL)
    {
    /* get all mrna/prot pairs with this CDS structure */
    cdsId = row2[0];
   
    /* ranking reflects CDS quaility and preference for RefSeq and MGC */
    /* Swiss-Prot is prefered than TrEMBL */
    /* finally, higher prot-Mrna alignment score is prefered */
    safef(query3, sizeof(query3), 
    "select * from %s.kgCandidateZ where cdsId='%s' order by ranking, protDbId, score desc", 
    	    kgTempDb, cdsId);
    sr3  = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	      
    gotRefseq    = FALSE;
    gotNonRef    = FALSE;
    
    printCnt = 0;
    while (row3 != NULL)
        {
        cdsId    = row3[0];
	ranking  = row3[1];
	protDbId = row3[2];
	score    = row3[3];
	mrnaID   = row3[4];
	protAcc  = row3[5];
	alignID  = row3[6];

    	isRefseq    = FALSE;
	
	if ((mrnaID[0] == 'N') && (mrnaID[1] == 'M') && (mrnaID[2] == '_'))
	    {
	    isRefseq  = TRUE;
	    }
	
	if (isRefseq)
	    {
	    /* print one qualified RefSeq */
	    if (!gotRefseq)
	    	{
		/* double check again to see if the RefSeq is still valid */
    		sprintf(condStr, "name='%s'", mrnaID);
    		refseqId = sqlGetField(roDb, "refGene", "name", condStr);
		if (refseqId != NULL)
		    {
		    if (printCnt < 1)
		    	{
		    	printKg(conn4, conn5, kgTempDb, spDb, cdsId, mrnaID, protAcc, 
		    		alignID, outf);
		    	printCnt++;
		    	gotRefseq = TRUE;
		    	}
		    }
		else
		    {
		    fprintf(stderr, "%s no longer found in refGene.\n", mrnaID);
		    }
		}
	    }
	else
	    {
	    /* print out only one non-RefSeq entry for this CDS structure */
	    if (!gotRefseq && (!gotNonRef))
	    	{
	        printKg(conn4, conn5, kgTempDb, spDb, cdsId, mrnaID, protAcc, alignID, outf);
		printCnt++;
		gotNonRef = TRUE;
		}
	    }
	
	/* save duplicates of mrna/prot that has same CDS but does not make it to final KG set */
	if (sameWord(printedCds, cdsId))
	    {
	    if ( (!sameWord(printedMrna, mrnaID)) || (!sameWord(printedProt, protAcc)) )
	    	{
	    	fprintf(dupOutf, "%s\t%s\t%s\t%s\n", convert(printedMrna), printedProt, convert(mrnaID), protAcc);
	   	}
	    }
        row3 = sqlNextRow(sr3);
	}
    
    sqlFreeResult(&sr3);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(outf);
fclose(dupOutf);
return(0);
}

