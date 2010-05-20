/* t2g.c - display details of text2genome stuff */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgc.h"
#include "trackDb.h"
#include "web.h"
#include "hash.h"
#include "obscure.h"

void printPubmedLink(char* pmid) 
{
    printf("<B>PubMed:</B>&nbsp;<A HREF=\"http://www.ncbi.nlm.nih.gov/pubmed/%s\" TARGET=_blank>%s</A><BR>\n", pmid, pmid);
}

void printPmcLink(char* pmcId) 
{
    printf("<B>PubMed&nbsp;Central:</B>&nbsp;<A HREF=\"http://www.ncbi.nlm.nih.gov/pmc/articles/PMC%s/?tool=pubmed\" TARGET=_blank>%s</A><BR>\n", pmcId, pmcId);
}

void printT2gLink(char* pmcId) 
{
    printf("<B>Text2Genome:</B>&nbsp;<A HREF=\"http://kumiho.smith.man.ac.uk/bergman/text2genome/inspector.cgi?pmcId=%s\" TARGET=_blank>%s</A><BR>\n", pmcId, pmcId);
}

void printLinks(char* pmid, char* pmcId) 
{
    printT2gLink(pmcId);
    printPubmedLink(pmid);
    printPmcLink(pmcId);
    printf("<BR>\n");
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
	printf("<b>%s</b>", row[2]);
	printf("<p style=\"font-size:96%%\">%s</p>", row[3]);
	printf("<p style=\"font-size:92%%\">%s</p>", row[4]);
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
int start = cgiInt("o");
int end = cgiInt("t");
char versionString[256];
char dateReference[256];
char headerTitle[512];
struct sqlConnection *conn = hAllocConn(database);

/* see if hgFixed.trackVersion exists */
boolean trackVersionExists = hTableExists("hgFixed", "trackVersion");

if (trackVersionExists)
    {
    char query[256];
    safef(query, sizeof(query), "select version,dateReference from hgFixed.trackVersion where db = '%s' AND name = 't2g' order by updateTime DESC limit 1", database);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;

    /* in case of NULL result from the table */
    versionString[0] = 0;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	safef(versionString, sizeof(versionString), "version %s",
		row[0]);
	safef(dateReference, sizeof(dateReference), "%s",
		row[1]);
	}
    sqlFreeResult(&sr);
    }
else
    {
    versionString[0] = 0;
    dateReference[0] = 0;
    }

if (versionString[0])
    safef(headerTitle, sizeof(headerTitle), "%s - %s", item, versionString);
else
    safef(headerTitle, sizeof(headerTitle), "%s", item);

genericHeader(tdb, headerTitle);

printf("<B>Position:</B>&nbsp;"
           "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                  hgTracksPathAndSettings(), database, seqName, start+1, end);
char startBuf[64], endBuf[64];
sprintLongWithCommas(startBuf, start + 1);
sprintLongWithCommas(endBuf, end);
printf("%s:%s-%s</A><BR>\n", seqName, startBuf, endBuf);
long size = end - start;
sprintLongWithCommas(startBuf, size);
printf("<B>Genomic Size:</B>&nbsp;%s<BR>\n", startBuf);

char* docId = printArticleInfo(conn, tdb, item);


if (docId!=0) 
{
    printSeqInfo(conn, tdb, docId);
}
hFreeConn(&conn);
}

