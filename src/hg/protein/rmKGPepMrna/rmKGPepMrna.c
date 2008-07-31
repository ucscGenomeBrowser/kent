/* rmKGPepMrna generates new .tab files with unused mRNA and protein sequences from known genes db tables removed. */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "rmKGPepMrna - generate new .tab files with unused mRNA and protein sequences from known genes db tables removed."
  "usage:\n"
  "   rmKGPepMrna xxxx yyyy\n"
  "      xxxx is the genome  database name\n"
  "      yyyy is the release date of the biosql and proteins DBs\n"
  "example: rmKGPepMrna hg15 0405\n");
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
char *seq;
char *acc;
 
char protDbName[100];
char spDbName[100];
char *dbName;
char *ro_dbName;

FILE *o1, *o2;

struct dnaSeq *kgSeq;
    
if (argc != 4) usage();

o1 = fopen("j.dat",  "w");
o2 = fopen("jj.dat", "w");
    
dbName = argv[1];
ro_dbName = argv[3];
sprintf(protDbName,   "proteins%s", argv[2]);
sprintf(spDbName, "sp%s",   argv[2]);

conn= hAllocConn(ro_dbName);
conn2= hAllocConn(ro_dbName);
sprintf(query2,"select name from %s.knownGene;", dbName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    kgID    = row2[0];
    
    sprintf(cond_str, "name = '%s';", kgID);
    seq = sqlGetField(dbName, "knownGenePep", "seq", cond_str);
    if (seq != NULL)
	{
        fprintf(o1, "%s\t%s\n", kgID, seq);fflush(o1);
	}
    else
	{
        sprintf(cond_str, "name = '%s';", kgID);
        proteinID=sqlGetField(dbName, "knownGene", "proteinID", cond_str);
	if (proteinID != NULL)
	    {
            sprintf(cond_str, "val = '%s';", proteinID);
            acc = sqlGetField(spDbName, "displayId", "acc", cond_str);
	    if (acc == NULL)
		{
fprintf(stderr, "NO acc.displayId.%s: %s from name.knownGene.%s: %s\n", spDbName, proteinID, dbName, kgID);
		fflush(stderr);
		}
	    else
		{
		sprintf(cond_str, "acc = '%s';", acc);
		seq = sqlGetField(spDbName, "protein", "val", cond_str);
		if (seq == NULL)
		    {
		    fprintf(stderr, "NO protein seq for %s\n", kgID);
		    fprintf(stderr, "proteinID.knownGene.%s: %s, acc.displayID.%s: %s\n", dbName, proteinID, spDbName, acc);
		    fflush(stderr);
		    }
		    else
		    {
		    fprintf(o1, "%s\t%s\n", kgID, seq);
		    }
		}
	    } else {
fprintf(stderr, "kgID: %s not in knownGenePep or knownGene\n", kgID);
	    }
	}

    sprintf(cond_str, "name = '%s';", kgID);
        
    seq = sqlGetField(dbName, "knownGeneMrna", "seq", cond_str);
    if (seq != NULL)
    	{
        fprintf(o2, "%s\t%s\n", kgID, seq);fflush(o1);
        }
    else
        {
	kgSeq = hGenBankGetMrna(dbName, kgID, NULL);
	   
	if (kgSeq != NULL)
	    {
            fprintf(o2, "%s\t%s\n", kgID, kgSeq->dna);fflush(o1);
            }
	else
	    {
	    fprintf(stderr, "NO mRNA seq for %s\n", kgID);fflush(stderr);
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

