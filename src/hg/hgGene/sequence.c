/* sequence.c - Handle sequence section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "hui.h"
#include "hdb.h"
#include "dnautil.h"
#include "dbDb.h"
#include "axtInfo.h"
#include "hCommon.h"
#include "hgGene.h"

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

static void genomicLink(struct sqlConnection *conn, char *geneId)
/* Figure out known genes table, position of gene, link it. */
{
char *table = genomeSetting("knownGene");
hPrintLinkCellStart();
printGenomicAnchor(table, geneId, curGeneChrom, curGeneStart, curGeneEnd);
hPrintf("Genomic</A>");
hPrintLinkCellEnd();
}

static void comparativeLink(struct sqlConnection *conn, char *geneId)
/* Print comparative genomic link. */
{
char *table = genomeSetting("knownGene");
if (sqlTableExists(conn, "axtInfo"))
    {
    struct dbDb *dbList = hGetAxtInfoDbs();
    if (dbList != NULL)
        {
	char *db2 = dbList->name;
	struct axtInfo *aiList = hGetAxtAlignments(db2);
	hPrintLinkCellStart();
	hPrintf("<A HREF=\"%s?%s", hgcName(), cartSidUrlString(cart) );
	hPrintf("&g=htcGenePsl&i=%s&c=%s&l=%d&r=%d", 
		geneId, curGeneChrom, curGeneStart, curGeneEnd);
	hPrintf("&o=%s&alignment=%s&db2=%s\"",
		table, cgiEncode(aiList->alignment), db2);
	hPrintf(" class=\"toc\">");
	hPrintf("Comparative</A>");
	hPrintLinkCellEnd();
	dbDbFreeList(&dbList);
	}
    }
http://hgwdev-kent.cse.ucsc.edu/cgi-bin/hgc?hgsid=61332&g=htcGenePsl&i=D26481&c=chr1&l=46764895&r=46777037&o=knownGene&alignment=Blastz+Best+in+Genome&db2=mm3&xyzzy=xyzzy#startcodon
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
comparativeLink(conn, geneId);
printMrnaLink(conn,geneId);
printProteinLink(conn,geneId);
hPrintLinkTableEnd();
}

struct section *sequenceSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create sequence section. */
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

