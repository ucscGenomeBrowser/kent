/* sequence.c - Handle sequence section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hgGene.h"
#include "hui.h"
#include "dnautil.h"

static void printGenomicAnchor(char *table, char *itemName,
	char *chrom, char *start, char *end)
/* Print genomic sequence anchor. */
{
hPrintf("<A HREF=\"http://hgwdev-kent.cse.ucsc.edu/cgi-bin/hgc?%s",
   cartSidUrlString(cart));
hPrintf("&g=htcGeneInGenome&i=%s", itemName);
hPrintf("&c=%s&l=%s&r=%s", chrom, start, end);
hPrintf("&o=%s&table=%s", table, table);
hPrintf("\" class=\"toc\">");
}

static void genomicLink(struct sqlConnection *conn, char *geneId)
/* Figure out known genes table, position of gene, link it. */
{
char *table = genomeSetting("knownGene");
char query[512];
struct sqlResult *sr;
char **row;

safef(query, sizeof(query),
	"select chrom,txStart,txEnd from %s where name='%s'",
	table, geneId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    hPrintLinkCellStart();
    printGenomicAnchor(table, geneId, row[0], row[1], row[2]);
    hPrintf("Genomic</A>");
    hPrintLinkCellEnd();
    }
sqlFreeResult(&sr);
}

static void printSeqLink(struct sqlConnection *conn, char *geneId,
	char *tableId, char *command, char *label)
/* Print out link to mRNA or protein. */
{
char *table = genomeSetting(tableId);
if (sqlTableExists(conn, table))
    {
    char query[512];
    safef(query, sizeof(query), "select count(*) from %s where name = '%s'",
    	table, geneId);
    if (sqlExists(conn, query))
        {
	hPrintLinkCellStart();
	hPrintf("<A HREF=\"../cgi-bin/hgGene?%s&%s=1\" class=\"toc\">",
	       cartSidUrlString(cart), command);
	hPrintf("%s</A>", label);
	hPrintLinkCellEnd();
	}
    }
}


static void printMrnaLink(struct sqlConnection *conn, char *geneId)
/* Print out link to fetch mRNA. */
{
printSeqLink(conn, geneId, "knownGeneMrna", hggDoGetMrnaSeq,
	"mRNA (may differ from genome)");
}

static void printProteinLink(struct sqlConnection *conn, char *geneId)
/* Print out link to fetch mRNA. */
{
printSeqLink(conn, geneId, "knownGenePep", hggDoGetProteinSeq,
	"Protein");
}


static void sequencePrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the sequence section. */
{
hPrintLinkTableStart();
genomicLink(conn, geneId);
printMrnaLink(conn,geneId);
printProteinLink(conn,geneId);
hPrintLinkTableEnd();
}

struct section *sequenceSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create links section. */
{
struct section *section = sectionNew(sectionRa, "sequence");
section->print = sequencePrint;
return section;
}

static void showSeq(struct sqlConnection *conn, char *geneId,
	char *geneName, char *tableId)
/* Show some sequence. */
{
char *table = genomeSetting(tableId);
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
    hPrintf(">%s (%s)\n", geneId, geneName);
    writeSeqWithBreaks(stdout, seq, strlen(seq), 60);
    }
sqlFreeResult(&sr);
hPrintf("</PRE></TT>");
}

void doGetMrnaSeq(struct sqlConnection *conn, char *geneId, char *geneName)
/* Get mRNA sequence in a simple page. */
{
showSeq(conn, geneId, geneName, "knownGeneMrna");
}

void doGetProteinSeq(struct sqlConnection *conn, char *geneId, char *geneName)
/* Get mRNA sequence in a simple page. */
{
showSeq(conn, geneId, geneName, "knownGenePep");
}

