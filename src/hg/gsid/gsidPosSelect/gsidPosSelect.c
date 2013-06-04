/* gsidPosSelection - Generate positive selection track data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "psl.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsidPosSelection - Generate positive selection track data\n"
  "usage:\n"
  "   gsidPosSelection database pslTable buildDb modelTable outfile\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void gsidPosSelection(char *database, char *pslTable, char *buildDb, char *modelTable, char *outfileName)
/* gsidPosSelection - Generate positive selection track data. */
{
struct sqlConnection *conn, *conn2;
struct sqlResult *sr;
char **row;
char query[2048];
int blockCnt = 0, i;

char query2[256];
struct sqlResult *sr2;
char **row3;
int aaPos;
char *aa;
float Pr;
FILE *outf;

int blkStart, blkSize, qStart;
int pos;

conn = hAllocConn(database);
conn2= hAllocConn(database);

outf = mustOpen(outfileName, "w");

/* loop thru all psitively selected sites */
sqlSafef(query2, sizeof query2, "select * from %s.%s", buildDb, modelTable);
sr2 = sqlMustGetResult(conn2, query2);
row3 = sqlNextRow(sr2);
              
while (row3 != NULL)
    {
    aaPos = atoi(row3[0]);
    aa    = row3[1];
    Pr    = atof(row3[2]);

    /* Query target->pslTable to get target-to-genomic mapping: */
    sqlSafef(query, sizeof(query), "select * from %s", pslTable);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    	{
    	struct psl *thePsl = pslLoad(row);
   
    	blockCnt = thePsl->blockCount; 
    	for (i=0; i<blockCnt; i++)
	    {
	    qStart   = thePsl->qStarts[i];
	    blkStart = thePsl->tStarts[i];
	    blkSize  = thePsl->blockSizes[i];
	    pos =  blkStart + (aaPos - qStart)*3 - 1;
	    if ((pos > blkStart) && (pos <= (blkStart+blkSize*3)))
		{
		fprintf(outf, "chr1\t%d\t%d\t%s\t%d\n", pos-2, pos+1, aa, (int)(Pr*1000));
	    	}
	    }
    	pslFree(&thePsl);
    	}
    sqlFreeResult(&sr);

    row3 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn);
hFreeConn(&conn2);
fclose(outf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *database, *pslTable, *buildDatabase, *modelTable, *outfileName;

optionInit(&argc, argv, options);
if (argc != 6)
    usage();
database      = argv[1];
pslTable      = argv[2];
buildDatabase = argv[3];
modelTable    = argv[4];
outfileName   = argv[5];

gsidPosSelection(database, pslTable, buildDatabase, modelTable, outfileName);
return 0;
}
