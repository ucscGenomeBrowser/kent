/* rmKGPepMrna generates new .tab files with unused mRNA and protein sequences from known genes db tables removed. */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

struct sqlConnection *conn, *conn2;

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
char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;

char cond_str[256];
char *kgID;
char *proteinID;
char *seq;
char *bioentryID;
 
char protDbName[100];
char biosqlDbName[100];
char *dbName;

FILE *o1, *o2;

struct dnaSeq *kgSeq;
    
if (argc != 3) usage();

o1 = fopen("j.dat",  "w");
o2 = fopen("jj.dat", "w");
    
dbName = argv[1];
sprintf(protDbName,   "proteins%s", argv[2]);
sprintf(biosqlDbName, "biosql%s",   argv[2]);

conn2= hAllocConn();
sprintf(query2,"select name from %s.knownGene;", dbName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    kgID    = row2[0];
    
    sprintf(cond_str, "name = '%s';", kgID);
    seq = sqlGetField(conn, dbName, "knownGenePep", "seq", cond_str);
    if (seq != NULL)
	{
        fprintf(o1, "%s\t%s\n", kgID, seq);fflush(o1);
	}
    else
	{
        sprintf(cond_str, "name = '%s';", kgID);
        proteinID = sqlGetField(conn, dbName, "knownGene", "proteinID", cond_str);
	if (proteinID != NULL)
	    {
            sprintf(cond_str, "display_id = '%s';", proteinID);
            bioentryID = sqlGetField(conn, biosqlDbName, "bioentry", "bioentry_id", cond_str);
	   
            sprintf(cond_str, "bioentry_id = '%s';", bioentryID);
            seq = sqlGetField(conn, biosqlDbName, "biosequence", "biosequence_str", cond_str);
	    if (seq == NULL)
		{
		fprintf(stderr, "NO protein seq for %s\n", kgID);fflush(stderr);
		}
	    else
		{
           	fprintf(o1, "%s\t%s\n", kgID, seq);
		}
	    }
	}

    sprintf(cond_str, "name = '%s';", kgID);
        
    seq = sqlGetField(conn, dbName, "knownGeneMrna", "seq", cond_str);
    if (seq != NULL)
    	{
        fprintf(o2, "%s\t%s\n", kgID, seq);fflush(o1);
        }
    else
        {
	kgSeq = hGenBankGetMrna(kgID, "knownGeneMrna");
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

