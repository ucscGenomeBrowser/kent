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
char *mrnaStr, *proteinStr, *alignStr;

void usage()
/* Explain usage and exit. */
{   
errAbort(
  "kgUniq - Pick a unique protein among multiple candidates with identical CDS structure\n"
  "usage:\n"
  "   kgUniq tempDb infile outfile\n"
  "      tempDb  is the temp DB name for KG build\n"
  "      infile  is the input  file name\n"
  "      outfile is the output file name\n"
  "example:\n"
  "   kgUniq kgMm6ATemp kgSorted.tab kg.gp\n");
}
    
int main(int argc, char *argv[])
{
struct sqlConnection *conn2;
struct sqlResult *sr2;
char query2[256];
char **row2;
    
char *proteinID;
char *score;
FILE *outf;
char *name, *chrom, *cdsStart, *cdsEnd;

char *chp0, *chp, *chp1;
int  i;

int  isDuplicate;
    
char *kgTempDb;
char *infileName, *outfileName;

if (argc != 4) usage();
    
kgTempDb    = argv[1];
infileName  = argv[2];
outfileName = argv[3];
  
inf  = mustOpen(infileName, "r");
outf = fopen(outfileName, "w");
conn2= hAllocConn();

strcpy(oldInfo, "");

isDuplicate   = 0;
oldMrnaStr    = cloneString("");
oldAlignStr   = cloneString("");
oldProteinStr = cloneString("");

mrnaStr       = cloneString("");
proteinStr    = cloneString("");
alignStr      = cloneString("");

while (fgets(line_in, 500, inf) != NULL)
    {
    strcpy(line, line_in);
    strcpy(line2, line_in);

    chp = strstr(line, "\t");	
    *chp = '\0';
    name = strdup(line);
    
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
    proteinID = strdup(chp1);
    
    strcpy(newInfo, line2);
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

	safef(query2, sizeof(query2), 
	      "select * from %s.kgTemp2 where name='%s' and proteinID='%s' and chrom='%s' and cdsStart='%s' and cdsEnd='%s'", 
	      kgTempDb, name, proteinID, chrom, cdsStart, cdsEnd);
	sr2 = sqlMustGetResult(conn2, query2);
    	row2 = sqlNextRow(sr2);
    	while (row2 != NULL)
	    {
	    for (i=0; i<11; i++) fprintf(outf, "%s\t", row2[i]);
	    fprintf(outf, "%s\n", row2[11]);fflush(stdout);
	    row2 = sqlNextRow(sr2);
	    }
	sqlFreeResult(&sr2);
	}
    }
    
return(0);
}

