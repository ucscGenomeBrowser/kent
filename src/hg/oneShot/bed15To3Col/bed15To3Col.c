/* bed15To3Col - converts a bed 15 table into a 3 column data file */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bed15To3Col - this program converts a bed 15 table into a 3 column data file\n"
  "usage:\n"
  "   bed15To3Col db tableName\n"
  "      db is the database name\n"
  "      outFn is the output file name\n"
  "example: bed15To3Col hg18 sestanBrainAtlas sestanBrainAtlas3Col.tab\n");
}

int main(int argc, char *argv[])
{
char *database;

struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;
char *tableName;

int expCnt;
char *probeId;
char *expScores;

char *chp, *chp9;
char *outFn;

FILE *outf;

if (argc != 4) usage();
database  = argv[1];
tableName = argv[2];
outFn     = argv[3];

outf = mustOpen(outFn, "w");

conn2= hAllocConn(database);
sqlSafef(query2, sizeof query2, "select name, expCount, expScores from %s", tableName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
expCnt = 0;
while (row2 != NULL)
    {
    probeId = row2[0];
    expScores = row2[2];

    chp    = expScores;
    chp9   = strstr(chp, ",");
    expCnt = 0;
    while ((chp9 != NULL) && (chp != NULL))
    	{
	*chp9 = '\0';
    	fprintf(outf, "%s\t%d\t%s\n", probeId, expCnt, chp);
        chp = chp9;
	chp++;
	expCnt++;
	chp9 = strstr(chp, ",");
	}

    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
hFreeConn(&conn2);

fclose(outf);
return(0);
}

