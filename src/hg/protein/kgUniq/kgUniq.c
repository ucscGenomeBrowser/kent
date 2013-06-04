/* kgUniq - Pick a unique protein among multiple candidates with identical CDS structure */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

FILE *inf;
char line_in[500];
char line[500];
char line2[500];
char newInfo[500], oldInfo[500];
char *oldMrnaStr, *oldProteinStr, *oldAlignStr;
char *mrnaStr, *proteinStr;

void usage()
/* Explain usage and exit. */
{   
errAbort(
  "kgUniq - Pick a unique protein among multiple candidates with identical CDS structure\n"
  "usage:\n"
  "	kgUniq tempDb infile outfile\n"
  "      tempDb  is the temp DB name for KG build\n"
  "      infile  is the input  file name\n"
  "      outfile is the output file name\n"
  "      dupOutfile is the output file name for duplicates\n"
  "example:\n"
  "   kgUniq kgMm6ATemp sp050315 kgSorted.tab knownGene.gp dupSpMrna.tab\n");
}
    
int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;
struct sqlResult *sr2;
char query2[256];
char **row2;
char condStr[255];

char *uniProtDb;
char *score;
FILE *outf;
FILE *dupOutf;
char *chrom, *cdsStart, *cdsEnd;
char *displayID;
char *oldDisplayID;

char *chp0, *chp, *chp1;
int  i;

int  isDuplicate;
    
char *kgTempDb;
char *infileName, *outfileName, *dupOutfileName;

if (argc != 6) usage();
    
kgTempDb    = argv[1];
uniProtDb  = argv[2];
infileName  = argv[3];
outfileName = argv[4];
dupOutfileName = argv[5];
  
inf     = mustOpen(infileName, "r");
outf    = mustOpen(outfileName, "w");
dupOutf = mustOpen(dupOutfileName, "w");
conn    = hAllocConn();
conn2= hAllocConn();

strcpy(oldInfo, "");

isDuplicate   = 0;
oldMrnaStr    = cloneString("");
oldAlignStr   = cloneString("");
oldProteinStr = cloneString("");
oldDisplayID  = cloneString("");

mrnaStr       = cloneString("");
proteinStr    = cloneString("");

while (fgets(line_in, 500, inf) != NULL)
    {
    strcpy(line, line_in);
    strcpy(line2, line_in);

    chp = strstr(line, "\t");	
    *chp = '\0';
    mrnaStr = strdup(line);
    
    chp ++;
    chp1 = chp;
    chp = strstr(chp, "\t");	
    *chp = '\0';
    chrom = strdup(chp1);
    
    chp ++;
    chp1 = chp;
    chp = strstr(chp, "\t");	
    *chp = '\0';
    cdsStart = strdup(chp1);
    
    chp ++;
    chp1 = chp;
    chp = strstr(chp, "\t");	
    *chp = '\0';
    cdsEnd = strdup(chp1);
    chp1 = line2 + (chp - line);
    *chp1 = '\0';
 
    chp ++;
    chp1 = chp;
    chp  = strstr(chp, "\t");	
    *chp = '\0';
    score= strdup(chp1);
   
    chp ++;
    chp1 = chp;
    chp  = strstr(chp, "\n");	
    *chp = '\0';
    proteinStr= strdup(chp1);
    
    strcpy(newInfo, line2);
    if (sameString(oldInfo, newInfo))
	{
	isDuplicate = 1;
 	safef(condStr, sizeof(condStr), "acc='%s'", proteinStr);
        displayID = sqlGetField(conn, uniProtDb, "displayId", "val", condStr);	
	if (displayID == NULL) 
	    {
	    printf("!!! %s not found\n", proteinStr);fflush(stdout);
	    }
 	safef(condStr, sizeof(condStr), "acc='%s'", oldProteinStr);
        oldDisplayID = sqlGetField(conn, uniProtDb, "displayId", "val", condStr);	
	if (oldDisplayID == NULL) 
	    {
	    printf("!!! %s not found\n", oldProteinStr);fflush(stdout);
	    }
	fprintf(dupOutf, 
		"%s\t%s\t%s\t%s\n", oldMrnaStr, oldDisplayID, mrnaStr, displayID);fflush(stdout);
	}
    else
	{
	/* remember previous record as old only if it is not a duplicate */
	if (!isDuplicate)
	    {
	    oldMrnaStr 	  = mrnaStr;
	    oldProteinStr = proteinStr;
	    }
	strcpy(oldInfo, newInfo);
	isDuplicate = 0;

	sqlSafef(query2, sizeof(query2), 
	      "select * from %s.kgCandidate2 where name='%s' and proteinID='%s' and chrom='%s' and cdsStart='%s' and cdsEnd='%s'", 
	      kgTempDb, mrnaStr, proteinStr, chrom, cdsStart, cdsEnd);
	sr2 = sqlMustGetResult(conn2, query2);
    	row2 = sqlNextRow(sr2);
    	while (row2 != NULL)
	    {
	    for (i=0; i<10; i++) fprintf(outf, "%s\t", row2[i]);
	    if (!sameWord(proteinStr, row2[10]))
	    	{
		printf("\n??? %s\t%s\n", proteinStr, row2[10]);fflush(stdout);
		}
		
 	    safef(condStr, sizeof(condStr), "acc='%s'", proteinStr);
            displayID = sqlGetField(conn, uniProtDb, "displayId", "val", condStr);	
	    if (displayID == NULL) 
	    	{
		printf("!!! %s not found\n", proteinStr);fflush(stdout);
		}
	    fprintf(outf, "%s\t", displayID);
	    fprintf(outf, "%s\n", row2[11]);
	    row2 = sqlNextRow(sr2);
	    }
	sqlFreeResult(&sr2);
	}
    }
fclose(inf);
fclose(outf);
fclose(dupOutf);
return(0);
}

