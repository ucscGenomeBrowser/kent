/* t2g.c - display details of text2genome stuff */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgc.h"
#include "hgColors.h"
#include "trackDb.h"
#include "web.h"
#include "hash.h"
#include "obscure.h"

#define PMCURL "http://www.ncbi.nlm.nih.gov/pmc/articles/PMC"

void printPubmedLink(char* pmid)
{
    printf("<B>PubMed:</B>&nbsp;<A HREF=\"http://www.ncbi.nlm.nih.gov/pubmed/%s\" TARGET=_blank>%s</A><BR>\n", pmid, pmid);
}

void printPmcLink(char* pmcId)
{
    printf("<B>PubMed&nbsp;Central:</B>&nbsp;<A HREF=\"%s%s\" TARGET=_blank>PMC%s</A><BR>\n", PMCURL, pmcId, pmcId);
}

void printT2gLink(char* pmcId)
{
    printf("<B>Text2Genome:</B>&nbsp;<A HREF=\"http://kumiho.smith.man.ac.uk/bergman/text2genome/inspector.cgi?pmcId=%s\" TARGET=_blank>%s</A><BR>\n", pmcId, pmcId);
}

void printLinks(char* pmid, char* pmcId)
{
    printPubmedLink(pmid);
    printPmcLink(pmcId);
    printT2gLink(pmcId);
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
	printf("<A HREF=\"%s%s\"><b>%s</b></A>\n", PMCURL, row[1], row[2]);
	printf("<p style=\"width:800px; font-size:96%%\">%s</p>\n", row[3]);
	printf("<p style=\"width:800px; font-size:92%%\">%s</p>\n", row[4]);
        docId = row[1];
	}
    sqlFreeResult(&sr);
    return docId;
}

void printSeqInfo(struct sqlConnection* conn, struct trackDb* tdb,
    char* docId, char* item, char* seqName, int start)
{
    /* print table of sequences */

    /* get all sequences for paper identified by docId*/
    char query[512];
    char* sequenceTable = hashMustFindVal(tdb->settingsHash, "sequenceTable");
    safef(query, sizeof(query), "SELECT concat_ws('|',seqId, sequence) FROM %s WHERE pmcId='%s'", sequenceTable, docId);
    struct slName *seqList = sqlQuickList(conn, query);

    /* get sequence-Ids for feature that was clicked on (item&startPos are unique) and put into hash */
    // there must be an easier way to do this...
    // couldn't find a function that splits a string and converts it to a list
    safef(query, sizeof(query), "SELECT seqIds,'' FROM t2g WHERE name='%s' "
	"and chrom='%s' and chromStart=%d", item, seqName, start);
    char* seqIdsString = sqlQuickString(conn, query);
    char* seqIds[1024];
    int partCount = chopString(seqIdsString, ",", seqIds, ArraySize(seqIds));
    int i;
    struct hash *seqIdHash = NULL;
    seqIdHash = newHash(0);
    for (i=0; i<partCount; i++)
	hashAdd(seqIdHash, seqIds[i], NULL);
    freeMem(seqIdsString);

    // output table
    webNewSection("Sequences in article");
    printf("<small>Sequences that map to the feature that was clicked "
	"are highlighted in bold</small>");
    webPrintLinkTableStart();

    struct slName *listEl = seqList;
    while (listEl != NULL)
        {
        char* parts[2];
        chopString(listEl->name, "|", parts, 2);
        char* seqId    = parts[0];
        char* seq      = parts[1];

        if (hashLookup(seqIdHash, seqId))
            printf("<TD BGCOLOR=\"#%s\"><TT><B>%s</B></TT></TD>",
		HG_COL_TABLE, seq);
        else
            printf("<TD style='background-color:#%s; color:#AAAAAA;'><TT>%s"
		"</TT></TD>\n", HG_COL_TABLE, seq);
        webPrintLinkTableNewRow();
        listEl=listEl->next;
        }
	webPrintLinkTableEnd();
    printTrackHtml(tdb);

    slFreeList(seqList);
    freeHash(&seqIdHash);
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
    printSeqInfo(conn, tdb, docId, item, seqName, start);
hFreeConn(&conn);
}
