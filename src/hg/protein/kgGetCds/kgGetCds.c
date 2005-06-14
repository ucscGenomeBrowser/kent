/* kgGetCds - create a gene candidate table with CDS info */

#include "common.h"
#include "genePred.h"
#include "genePredReader.h"
#include "jksql.h"
#include "bits.h"
#include "hdb.h"

static char const rcsid[] = "$Id: kgGetCds.c,v 1.2 2005/06/04 15:57:23 fanhsu Exp $";

char cdsBloc[2000][30];
void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "kgGetCds - create a gene candidate table with CDS info\n"
    "\nUsage:\n"
    "   kgGetCds tempKgDb geneTable outfile\n"
    "\n"
    "   tempKgDb: 	temp KG build DB\n"
    "   geneTable:	KG gene candidate table\n"
    "   outfile:	output file\n"
    "\n"
    "Example:\n"
    "  kgGetCds kgHg17FTemp kgCandidateX jY.tmp\n"
    );
}

void processAlign(char *kgTempDb, char *alignID, int cdsCnt, FILE *outf)
{
struct sqlConnection *conn2, *conn3, *conn4;
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;
char *score;
char *chrom;
char *protAcc;
char *mrnaID;
char *ranking;
char *protDbId;
char condStr[255];
int  i;
char *parProtAcc;
boolean first;
char *chp;

conn2= hAllocConn();
conn3= hAllocConn();
conn4= hAllocConn();

safef(query2, sizeof(query2), "select * from %s.kgCandidate where alignID='%s'", kgTempDb, alignID);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    mrnaID = row2[0];
    chrom = row2[1];
    ranking = row2[11];
    
    /* check if it is a composite mrnaID */
    /* if yes, select from entries with both protein and mrna specified */
    if (alignID[0] == 'U') 
    	{
	chp = strstr(row2[0], "_");
	*chp = '\0';
	protAcc = row2[0];
	chp ++;
	mrnaID = chp;
    	safef(query3, sizeof(query3), 
    	      "select protAcc, score from %s.protMrnaScore where mrnaAcc='%s' and protAcc='%s'",
	      kgTempDb, mrnaID, protAcc);
	}
    else
    	{
    	safef(query3, sizeof(query3), 
    	      "select protAcc, score from %s.protMrnaScore where mrnaAcc='%s' order by score desc",
	      kgTempDb, mrnaID);
	}
	
    sr3  = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	      
    while(row3 != NULL)
        {
	protAcc = row3[0];
	score   = row3[1];

        safef(condStr, sizeof(condStr), "accession='%s'", protAcc);
	protDbId = sqlGetField(conn4, "proteins050415", "spXref3", "biodatabaseID", condStr);

	if (protDbId == NULL)
	    {
	    fprintf(stderr, "Can't find protId for %s\n", protAcc);fflush(stdout);exit(1);
	    }

	fprintf(outf, "%s:", chrom);
	for (i=0; i<cdsCnt; i++) fprintf(outf, "%s", cdsBloc[i]);
	fprintf(outf, "\t%s\t%s\t%8s\t%s\t%s\t%s\n", 
		ranking, protDbId, score, mrnaID, protAcc, alignID);

	/* for composite type, process just one record */ 
        if (alignID[0] == 'U') break; 
	row3 = sqlNextRow(sr3);
	}
    sqlFreeResult(&sr3);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
hFreeConn(&conn2);
hFreeConn(&conn3);
hFreeConn(&conn4);
}

void kgGetCds(char *db, char *geneTable, FILE *outf)
/* get CDS info */
{
struct sqlConnection *conn = NULL;
struct genePred *gp;
int cdsCnt;
struct genePredReader *gpr;
int iExon, exonStart, exonEnd;

if (db != NULL)
    conn = sqlConnect(db);

gpr = genePredReaderQuery(conn, geneTable, NULL);
while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    cdsCnt = 0;
    for (iExon = 0; iExon < gp->exonCount; iExon++)
    	{
    	if (genePredCdsExon(gp, iExon, &exonStart, &exonEnd))
    	    {
	    sprintf(cdsBloc[cdsCnt], "%d-%d;", exonStart, exonEnd);
	    
	    cdsCnt++;
	    }
    	}
    if (cdsCnt > 0) 
    	{
	processAlign(db, gp->name, cdsCnt, outf);
	}
    else
    	{
	fprintf(stderr, "%s does not have cds.\n", gp->name);
	}
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *kgTempDb;
char *kgCandidateTable;
char *outFileName;
FILE *outf;

if (argc != 4)
    {
    usage("wrong number of args");
    }
else
    {
    kgTempDb         = argv[1];
    kgCandidateTable = argv[2];
    outFileName	     = argv[3];

    outf = mustOpen(outFileName, "w");

    kgGetCds(kgTempDb, kgCandidateTable, outf);
    fclose(outf);
    }
return 0;
}
