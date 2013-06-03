/* kgPickPrep - prepare input data for kgPick, replacing the cds field with a cds ID */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgPickPrep -  prepare input data for kgPick\n"
  "usage:\n"
  "   kgPickPrep tempDb outfile\n"
  "      tempDb is the temp KG DB name\n"
  "      outfile is the output file name\n"
  "example: kgPickPrep kgHg17ETemp kgCandidateZ.tab\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;
char *kgTempDb;
char *outfileName;
FILE *outf;
int  alignCnt = 0;
int  i;
char *cds;
char *oldCds;
int cdsCnt = 0;

if (argc != 3) usage();
kgTempDb    = argv[1];
outfileName = argv[2];

outf = mustOpen(outfileName, "w");
conn2= hAllocConn();
oldCds = strdup("");

/* go through each KG candidate */
sqlSafef(query2, sizeof(query2), "select * from %s.kgCandidateY", kgTempDb);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    cds = row2[0];
    if (!sameWord(cds, oldCds))
    	{
	cdsCnt++;
	free(oldCds);
	oldCds = strdup(cds);
	}

   /* put out the cds ID */
   fprintf(outf, "%d\t", cdsCnt);

   /* put out the rest of the fields */
   for (i=1; i<7; i++) fprintf(outf, "%s\t",row2[i]); 
   fprintf(outf, "\n");
    
   row2 = sqlNextRow(sr2);
   }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
fclose(outf);
return(0);
}

