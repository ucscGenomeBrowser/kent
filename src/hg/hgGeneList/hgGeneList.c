/* hgGeneList - Generate Known Genes List. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "obscure.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "dbDb.h"
#include "hgFind.h"
#include "hCommon.h"
#include "hui.h"

struct cart *cart = NULL;
struct hash *oldVars = NULL;

void doMiddle(struct cart *theCart)
/* Set up pretty web display. */
{
struct sqlConnection *conn, *conn2;
struct sqlConnection *connCentral = hConnectCentral();
char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
char buf[128];
char *answer;
char *database;
char *genome, *genomeDesc;  
char *geneSymbol, *kgID, *spID, *refseq, *geneDescription, *chrom, *txStart, *txEnd;
char notAvailable[20] = {"N/A"};
int iCnt = 0;

cart = theCart;
if (hIsMgcServer())
    cartWebStart(theCart, "MGC Known Genes List \n");
else
    cartWebStart(theCart, "UCSC Known Genes List \n");

database = cgiOptionalString("db");

sprintf(query, "select genome from dbDb where name = '%s'", database);
answer = sqlQuickQuery(connCentral, query, buf, sizeof(buf));
if (answer == NULL)
    {
    printf("<br>'%s' is not a valid genome database name.", database);
    cartWebEnd();
    return;
    }
else
    {
    genome = strdup(answer);
    }
if (!hTableExistsDb(database, "knownGene"))
    {
    printf("<br>Database %s currently does not have UCSC Known Genes.", database);
    cartWebEnd();
    return;
    }

sprintf(query, "select description from dbDb where name = '%s'", database);
genomeDesc = strdup(sqlQuickQuery(connCentral, query, buf, sizeof(buf)));
hDisconnectCentral(&connCentral);

printf("<H2>%s Genome (%s Assembly)</H2>\n", genome, genomeDesc);
fflush(stdout);
printf("<TABLE BORDER  BGCOLOR=\"FFFEe0\" BORDERCOLOR=\"cccccc\" CELLSPACING=0 CELLPADDING=2>");
printf("<TR><TH>Gene Symbol</TH><TH>mRNA</TH><TH>Protein</TH><TH>RefSeq</TH>");
printf("<TH>Chrom</TH><TH>Start</TH><TH>End</TH><TH>Description</TH></TR>\n");

conn = hAllocConn();
conn2= hAllocConn();

sprintf(query2,"select kgID,geneSymbol,description,spID, refseq from %s.kgXref order by geneSymbol;",
	database);

/* use the following for quck testing */
/*sprintf(query2,"select kgID, geneSymbol, description, spID, refseq from %s.kgXref order by geneSymbol limit 10;", database);
*/

sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    kgID 		= row2[0];
    geneSymbol 	 	= row2[1];
    geneDescription	= row2[2];
    spID		= row2[3];
    refseq		= row2[4];
    if ((refseq == NULL)||(sameWord(refseq, ""))) refseq = notAvailable;
    
    sprintf(query,"select chrom, txSTart, txEnd  from %s.knownGene where name='%s'", database, kgID);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    chrom 	= row[0];
    txStart 	= row[1];
    txEnd   	= row[2];

    printf("<TR><TD>");
    printf("<A href=\"http://hgwdev-fanhsu.cse.ucsc.edu/cgi-bin/hgGene?db=%s&hgg_gene=%s", 
    	   database, kgID);
    printf("&hgg_chrom=%s&hgg_start=%s&hgg_end=%s\">", chrom, txStart, txEnd);
    printf("%s</A></TD>", geneSymbol);
    printf("<TD>%s</TD>", kgID);
    printf("<TD>%s</TD>", spID);
    printf("<TD>%s</TD>", refseq);
    printf("<TD>%s</TD>", chrom);
    printf("<TD ALIGN=right>%s</TD>", txStart);
    printf("<TD ALIGN=right>%s</TD>", txEnd);
    printf("<TD>%s</TD></TR>\n", geneDescription);

    iCnt++;
    if ((iCnt % 1000) == 0) fflush(stdout);
    
    sqlFreeResult(&sr);
    row2 = sqlNextRow(sr2);
    }
    
printf("</TABLE>");

cartWebEnd();
}

char *excludeVars[] = {NULL};
int main(int argc, char *argv[])
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

