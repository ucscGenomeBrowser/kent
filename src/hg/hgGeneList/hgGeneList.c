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
char *kgID, *chrom, *txStart, *txEnd;
int iCnt = 1;

cart = theCart;
if (hIsMgcServer())
    cartWebStart(theCart, database, "MGC Known Genes List \n");
else
    cartWebStart(theCart, database, "UCSC Known Genes List \n");

getDbAndGenome(cart, &database, &genome, oldVars);
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

conn = hAllocConn(database);
conn2= hAllocConn(database);

sprintf(query2,"select kgID from %s.kgXref order by geneSymbol;",
	database);

/* use the following for quck testing */
/*sprintf(query2,"select kgID, geneSymbol, description, spID, refseq from %s.kgXref order by geneSymbol limit 10;", database);
*/
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    kgID = row2[0];
    
    sprintf(query,"select chrom, txSTart, txEnd  from %s.knownGene where name='%s'", database, kgID);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    chrom 	= row[0];
    txStart 	= row[1];
    txEnd   	= row[2];

    printf("<A href=\"/cgi-bin/hgGene?db=%s&hgg_gene=%s", 
    	   database, kgID);
    printf("&hgg_chrom=%s&hgg_start=%s&hgg_end=%s\">", chrom, txStart, txEnd);
    printf("%d</A><BR>\n", iCnt);
    iCnt++;
    if ((iCnt % 1000) == 0) fflush(stdout);
    
    sqlFreeResult(&sr);
    row2 = sqlNextRow(sr2);
    }
    
sqlFreeResult(&sr2);
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

