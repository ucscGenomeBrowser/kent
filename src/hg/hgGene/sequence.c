/* sequence.c - Handle sequence stuff - formerly a section, now part of links. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "hui.h"
#include "hdb.h"
#include "web.h"
#include "dnautil.h"
#include "dbDb.h"
#include "axtInfo.h"
#include "obscure.h"
#include "hCommon.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: sequence.c,v 1.24 2008/08/27 17:49:26 kent Exp $";

static void printGenomicAnchor(char *table, char *itemName,
	char *chrom, int start, int end)
/* Print genomic sequence anchor. */
{
hPrintf("<A HREF=\"%s?%s", hgcName(),
   cartSidUrlString(cart));
hPrintf("&g=htcGeneInGenome&i=%s", itemName);
hPrintf("&c=%s&l=%d&r=%d", chrom, start, end);
hPrintf("&o=%s&table=%s", table, table);
hPrintf("\" class=\"toc\">");
}

void printGenomicSeqLink(struct sqlConnection *conn, char *geneId,
	char *chrom, int start, int end)
/* Figure out known genes table, position of gene, link it. */
{
char *table = genomeSetting("knownGene");
webPrintWideCellStart(3, HG_COL_TABLE);
printGenomicAnchor(table, geneId, chrom, start, end);
hPrintf("Genomic Sequence (%s:", chrom);
printLongWithCommas(stdout, start+1);
hPrintf("-");
printLongWithCommas(stdout, end);
hPrintf(")</A>");
webPrintLinkCellEnd();
}

static void printSeqLink(struct sqlConnection *conn, char *geneId,
	char *tableId, char *command, char *label, int colCount)
/* Print out link to mRNA or protein. */
{
char *table = genomeSetting(tableId);
boolean gotHyperlink = FALSE;
webPrintWideCellStart(colCount, HG_COL_TABLE);
if (sqlTableExists(conn, table))
    {
    char query[512];
    safef(query, sizeof(query), "select count(*) from %s where name = '%s'",
    	table, geneId);
    if (sqlExists(conn, query))
        {
	hPrintf("<A HREF=\"../cgi-bin/hgGene?%s&%s=1&hgg_gene=%s\" class=\"toc\">",
	       cartSidUrlString(cart), command, geneId);
	hPrintf("%s</A>", label);
	gotHyperlink = TRUE;
	}
    }
if (!gotHyperlink)
    hPrintf("%s", label);
webPrintLinkCellEnd();
}


void printMrnaSeqLink(struct sqlConnection *conn, char *geneId)
/* Print out link to fetch mRNA. */
{
char *title = "mRNA";
char *tableId = "knownGene";
if (genomeOptionalSetting("knownGeneMrna") != NULL)
    {
    title = "mRNA (may differ from genome)";
    tableId = "knownGeneMrna";
    }
printSeqLink(conn, geneId, tableId, hggDoGetMrnaSeq, title, 2);
}

void printProteinSeqLink(struct sqlConnection *conn, char *geneId)
/* Print out link to fetch protein. */
{
char *table = genomeSetting("knownGenePep");
char query[256];
char title[128];
safef(query, sizeof(query), 
	"select length(seq) from %s where name='%s'" , table,  geneId);
int protSize = sqlQuickNum(conn, query);
if (protSize > 0)
    {
    safef(title, sizeof(title), "Protein (%d aa)", protSize);
    printSeqLink(conn, geneId, "knownGenePep", hggDoGetProteinSeq,
	    title, 1);
    }
else
    {
    webPrintLinkCellStart();
    hPrintf("No protein");
    webPrintLinkCellEnd();
    }
}


void sequenceTablePrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the sequence table. */
{
char *table = genomeSetting("knownGene");
struct dyString *query = newDyString(0);
char **row;
struct sqlResult *sr;
char *chrom;
int start,end;

/* Print the current position. */
webPrintLinkTableStart();
printGenomicSeqLink(conn, geneId, curGeneChrom, curGeneStart, curGeneEnd);
printMrnaSeqLink(conn,geneId);
printProteinSeqLink(conn,geneId);
webPrintLinkTableEnd();

/* Print out any additional positions. */
dyStringPrintf(query, "select chrom,txStart,txEnd from %s", table);
dyStringPrintf(query, " where name = '%s'", curGeneId);
dyStringPrintf(query, " and (chrom != '%s'", curGeneChrom);
dyStringPrintf(query, " or txStart != %d", curGeneStart);
dyStringPrintf(query, " or txEnd != %d)", curGeneEnd);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct sqlConnection *conn2 = hAllocConn();
    chrom = row[0];
    start = atoi(row[1]);
    end = atoi(row[2]);
    webPrintLinkTableStart();
    printGenomicSeqLink(conn2, geneId, chrom, start, end);
    webPrintLinkTableEnd();
    hFreeConn(&conn2);
    }
sqlFreeResult(&sr);
freeDyString(&query);
}

struct section *sequenceSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create sequence section. */
{
struct section *section = sectionNew(sectionRa, "sequence");
section->print = sequenceTablePrint;
return section;
}

void showSeqFromTable(struct sqlConnection *conn, char *geneId,
	char *geneName, char *table)
/* Show some sequence from given table. */
{
char query[512];
struct sqlResult *sr;
char **row;
hPrintf("<TT><PRE>");

safef(query, sizeof(query), 
    "select seq from %s where name = '%s'", table, geneId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    char *seq = row[0];
    hPrintf(">%s (%s) length=%d\n", geneId, geneName, (seq!=NULL) ? (int)strlen(seq): 0);
    writeSeqWithBreaks(stdout, seq, strlen(seq), 60);
    }
sqlFreeResult(&sr);
hPrintf("</PRE></TT>");
}

static void showSeq(struct sqlConnection *conn, char *geneId,
	char *geneName, char *tableId)
/* Show some sequence. */
{
char *table = genomeSetting(tableId);
showSeqFromTable(conn, geneId, geneName, table);
}

static void showMrnaFromGenePred(struct sqlConnection *conn, 
	char *geneId, char *geneName)
/* Get mRNA sequence for gene from gene prediction. */
{
char *table = genomeSetting("knownGene");
struct sqlResult *sr;
char **row;
char query[256];
boolean hasBin = hIsBinned(table);

hPrintf("<TT><PRE>");
safef(query, sizeof(query), 
    "select * from %s where name='%s'"
    " and chrom='%s' and txStart=%d and txEnd=%d", 
    table, geneId, curGeneChrom, curGeneStart, curGeneEnd);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gene = genePredLoad(row+hasBin);
    struct bed *bed = bedFromGenePred(gene);
    struct dnaSeq *seq = hSeqForBed(bed);
    hPrintf(">%s (%s predicted mRNA)\n", geneId, geneName);
    writeSeqWithBreaks(stdout, seq->dna, seq->size, 50);
    dnaSeqFree(&seq);
    bedFree(&bed);
    genePredFree(&gene);
    }
else
    errAbort("Couldn't find %s at %s:%d-%d", geneId, 
    	curGeneChrom, curGeneStart, curGeneEnd);
sqlFreeResult(&sr);
hPrintf("</TT></PRE>");
}

void doGetMrnaSeq(struct sqlConnection *conn, char *geneId, char *geneName)
/* Get mRNA sequence in a simple page. */
{
if (genomeOptionalSetting("knownGeneMrna") != NULL)
    showSeq(conn, geneId, geneName, "knownGeneMrna");
else
    showMrnaFromGenePred(conn, geneId, geneName);
}

void doGetProteinSeq(struct sqlConnection *conn, char *geneId, char *geneName)
/* Get mRNA sequence in a simple page. */
{
showSeq(conn, geneId, geneName, "knownGenePep");
}

