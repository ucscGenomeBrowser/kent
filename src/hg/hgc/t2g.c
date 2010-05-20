/* t2g.c - display details of text2genome stuff */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgc.h"
#include "trackDb.h"
#include "web.h"
#include "hash.h"

void printPubmedLink(char* pmid) 
{
    printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/pubmed/%s\">PubMed</A>", pmid);
}

void printPmcLink(char* pmcId) 
{
    printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/pmc/articles/PMC%s/?tool=pubmed\">PubmedCentral</A>", pmcId);
}

void printT2gLink(char* pmcId) 
{
    printf("<A HREF=\"http://kumiho.smith.man.ac.uk/bergman/text2genome/inspector.cgi?pmcId=%s\">text2genome</A>", pmcId);
}

void printLinks(char* pmid, char* pmcId) 
{
    printf("Links: ");
    printf("<SMALL>");
    printT2gLink(pmcId);
    printf(", ");
    printPubmedLink(pmid);
    printf(", ");
    printPmcLink(pmcId);
    printf("</SMALL><P>");
}

char* printArticleInfo(struct sqlConnection *conn, struct trackDb* tdb, char* item) 
/* Header with information about paper, return documentId */
{
    char query[512];
    char* articleTable = hashMustFindVal(tdb->settingsHash, "articleTable");

    safef(query, sizeof(query), "SELECT pmid, pmcId, title,authors, abstract FROM %s WHERE displayId='%s'", articleTable, item);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    char *docId=0;
    if ((row = sqlNextRow(sr)) != NULL)
	{
	printLinks(row[0], row[1]);
	printf("<b>%s</b><p>", row[2]);
	printf("<small>%s</small><p>", row[3]);
	printf("<small>%s</small>", row[4]);
        docId = row[1];
	}
    sqlFreeResult(&sr);
    return docId;
}

void printSeqInfo(struct sqlConnection* conn, struct trackDb* tdb,  char* docId) {
    /* print table of sequences */
    char query[512];
    char* sequenceTable = hashMustFindVal(tdb->settingsHash, "sequenceTable");
    safef(query, sizeof(query), "SELECT seqId, sequence FROM %s WHERE pmcId='%s'", sequenceTable, docId);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;

    webNewSection("Sequences in article");
	webPrintLinkTableStart();
    while ((row = sqlNextRow(sr)) != NULL)
	{
        webPrintLinkCell(row[1]);
        webPrintLinkTableNewRow();
        //printf("%s<br>", row[1]);
	}
	webPrintLinkTableEnd();

    sqlFreeResult(&sr);
    printTrackHtml(tdb);
}

void doT2gDetails(struct trackDb *tdb, char *item)
/* text2genome.org custom display */
{
char **row = NULL;
char query[512];
struct sqlResult *sr = NULL;
struct sqlConnection *conn = hAllocConn(database); // where the heck does "database" come from?
safef(query, sizeof(query), "select chrom,chromStart,chromEnd,strand from %s "
      "where name = '%s'", tdb->table, item);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
char *chr = row[0];
int start = sqlUnsigned(row[1]);
int end = sqlUnsigned(row[2]);
char strand[2];
strand[0] = row[3][0];
strand[1] = (char)NULL;
//cartWebStart(cart, database, "Article information ");
genericHeader(tdb, "Article information");
printf("<B>Item:</B>&nbsp;%s<BR>\n", item);
printPos(chr, start, end, strand, TRUE, item);
sqlFreeResult(&sr);
char* docId=0;
docId = printArticleInfo(conn, tdb, item);

if (docId!=0) 
{
    printSeqInfo(conn, tdb, docId);
}
hFreeConn(&conn);
}
