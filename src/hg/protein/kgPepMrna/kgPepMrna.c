/* kgPepMrna generates new .tab files with unused mRNA and protein sequences from known genes db tables removed. */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgPepMrna - generate new .tab files with unused mRNA and protein sequences from known genes db tables removed."
  "usage:\n"
  "   kgPepMrna tempKgDb roDb YYMMDD\n"
  "      tempKGDb is the temp KG build database name\n"
  "      roDb is the read only genome database name\n"
  "      YYMMDD is the release date of the UniProt and proteins DBs\n"
  "example: kgPepMrna kgHg17FTemp hg17 0405\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;

char cond_str[256];
char *kgID;
char *proteinID;
char *protAcc;
char *seq;
 
char *tempKgDb; 
char *roDbName;
char *protDbDate;

char proteinsDbName[100];
char spDbName[100];

FILE *o1, *o2;

struct dnaSeq *kgSeq;
    
if (argc != 4) usage();

o1 = fopen("j.dat",  "w");
o2 = fopen("jj.dat", "w");
    
tempKgDb   = argv[1];
roDbName  = argv[2];
protDbDate = argv[3];

sprintf(proteinsDbName, "proteins%s", protDbDate);
sprintf(spDbName,   "sp%s", protDbDate);

conn= hAllocConn(roDbName);
conn2= hAllocConn(roDbName);
sprintf(query2,"select name, proteinID from %s.knownGene;", roDbName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    kgID      = row2[0];
    proteinID = row2[1];
    
    sprintf(cond_str, "val = '%s';", proteinID);
    protAcc = sqlGetField(spDbName, "displayId", "acc", cond_str);
    if (protAcc != NULL)
    	{
    	sprintf(cond_str, "acc = '%s';", protAcc);
    	seq = sqlGetField(spDbName, "protein", "val", cond_str);
	}
    else
    	{
    	sprintf(cond_str, "acc = '%s';", proteinID);
    	seq = sqlGetField(spDbName, "varProtein", "val", cond_str);
    	if (seq == NULL)
    	    {
	    fprintf(stderr, 
	    "%s does not have a AA sequence in either protein or varProtein table.\n", proteinID);
	    exit(1);
	    }
	}
        
    fprintf(o1, "%s\t%s\n", kgID, seq);fflush(o1);

    sprintf(cond_str, "name = '%s';", kgID);
        
    seq = sqlGetField(tempKgDb, "mrnaSeq", "seq", cond_str);
    if (seq != NULL)
    	{
        fprintf(o2, "%s\t%s\n", kgID, seq);fflush(o1);
        }
    else
        {
	kgSeq = hGenBankGetMrna(roDbName, kgID, NULL);
	   
	if (kgSeq != NULL)
	    {
            fprintf(o2, "%s\t%s\n", kgID, kgSeq->dna);fflush(o1);
            }
	else
	    {
	    fprintf(stderr, "No mRNA seq for %s\n", kgID);fflush(stderr);
	    }
	}
    row2 = sqlNextRow(sr2);
    }

hFreeConn(&conn);
hFreeConn(&conn2);
sqlFreeResult(&sr2);

fclose(o1);
fclose(o2);
    
system("cat j.dat |sort|uniq > knownGenePep.tab");
system("cat jj.dat|sort|uniq > knownGeneMrna.tab");
system("rm j.dat");
system("rm jj.dat");

return(0);
}

