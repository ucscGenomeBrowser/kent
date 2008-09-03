/* getSeq - pages to get protein and nucleic acid sequence. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "jksql.h"
#include "cart.h"
#include "dnautil.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "hgSeq.h"
#include "hgNear.h"
#include "genePred.h"
#include "bed.h"

static char const rcsid[] = "$Id: getSeq.c,v 1.11 2008/09/03 19:20:41 markd Exp $";

static void printNameAndDescription(struct sqlConnection *conn, 
	struct genePos *gp, struct column *nameCol, struct column *descCol)
/* Look up name and description and print. */
{
char *name = NULL;
char *description = NULL;

if (nameCol != NULL)
    name = nameCol->cellVal(nameCol, gp, conn);
if (descCol != NULL)
    description = descCol->cellVal(descCol, gp, conn);
if (name != NULL)
    hPrintf(" %s", name);
if (description != NULL)
    hPrintf(" - %s", description);
freeMem(name);
freeMem(description);
}

static void getSeqFromBlob(struct sqlConnection *conn, struct column *colList,
	struct genePos *geneList, char *tableId)
/* Get sequence from blob field in table and print it as fasta. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct genePos *gp;
struct sqlConnection *conn2 = hAllocConn(database);
char *tableName = genomeSetting(tableId);
struct column *descCol = findNamedColumn("description");
struct column *nameCol = findNamedColumn("name");

hPrintf("<TT><PRE>");
for (gp = geneList; gp != NULL; gp = gp->next)
    {
    char *id = gp->name;
    safef(query, sizeof(query), 
    	"select seq from %s where name = '%s'", tableName, id);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	char *seq = row[0];
	hPrintf(">%s", id);
	printNameAndDescription(conn2, gp, nameCol, descCol);
	hPrintf("\n");
	writeSeqWithBreaks(stdout, seq, strlen(seq), 60);
	}
    sqlFreeResult(&sr);
    }
hPrintf("</TT></PRE>");
hFreeConn(&conn2);
}


static void getProtein( struct sqlConnection *conn, struct column *colList,
	struct genePos *geneList)
/* Print out proteins. */
{
getSeqFromBlob(conn, colList, geneList, "pepTable");
}

void getGeneMrna(struct sqlConnection *conn, 
	struct column *colList, struct genePos *geneList, char *tableId)
/* Get mRNA sequence for gene from gene prediction. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct genePos *gp;
struct sqlConnection *conn2 = hAllocConn(database);
struct column *descCol = findNamedColumn("description");
struct column *nameCol = findNamedColumn("name");
char *table = genomeSetting(tableId);
boolean hasBin = hOffsetPastBin(database, NULL, table);

hPrintf("<TT><PRE>");
for (gp = geneList; gp != NULL; gp = gp->next)
    {

    char *id = gp->name;
    safef(query, sizeof(query), 
    	"select * from %s where name='%s'"
	" and chrom='%s' and txStart=%d and txEnd=%d", 
    	table, id, gp->chrom, gp->start, gp->end);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct genePred *gene = genePredLoad(row+hasBin);
	struct bed *bed = bedFromGenePred(gene);
	struct dnaSeq *seq = hSeqForBed(database, bed);
	hPrintf(">%s (predicted mRNA)", id);
	printNameAndDescription(conn2, gp, nameCol, descCol);
	hPrintf("\n");
	writeSeqWithBreaks(stdout, seq->dna, seq->size, 50);
	dnaSeqFree(&seq);
	bedFree(&bed);
	genePredFree(&gene);
	}
    sqlFreeResult(&sr);
    }
hPrintf("</TT></PRE>");
hFreeConn(&conn2);
}

static void getMrna(struct sqlConnection *conn, struct column *colList,
	struct genePos *geneList)
/* Print out proteins. */
{
if (genomeOptionalSetting("mrnaTable") != NULL)
    getSeqFromBlob(conn, colList, geneList, "mrnaTable");
else
    getGeneMrna(conn, colList, geneList, "geneTable");
}

static boolean hasUtr5(struct genePred *gp)
/* Return TRUE if it looks like gene has a 5' UTR. */
{
if (gp->strand[0] == '-')
    return gp->txEnd != gp->cdsEnd;
else
    return gp->txStart != gp->cdsStart;
}

static struct dnaSeq *genePromoSeq(struct genePred *gp, 
	int upSize, int downSize)
/* Get promoter sequence for gene. */
{
struct dnaSeq *seq;
assert(upSize >= 0 && downSize >= 0);
if (gp->strand[0] == '-')
    {
    int start = gp->txEnd - downSize;
    int end = gp->txEnd + upSize;
    seq = hChromSeq(database, gp->chrom, start, end);
    reverseComplement(seq->dna, seq->size);
    }
else
    {
    int start = gp->txStart - upSize;
    int end = gp->txStart + downSize;
    seq = hChromSeq(database, gp->chrom, start, end);
    }
return seq;
}

static void getPromoter(struct sqlConnection *conn, struct column *colList,
	struct genePos *geneList)
/* Print out promoters. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct genePos *gp;
char *table = genomeSetting("geneTable");
struct sqlConnection *conn2 = hAllocConn(database);
int upSize = cartInt(cart, proUpSizeVarName);
int downSize = cartInt(cart, proDownSizeVarName);
boolean fiveOnly = cartBoolean(cart, proIncludeFiveOnly);
struct column *descCol = findNamedColumn("description");
struct column *nameCol = findNamedColumn("name");
boolean hasBin = hOffsetPastBin(database, NULL, table);

hPrintf("<TT><PRE>");
for (gp = geneList; gp != NULL; gp = gp->next)
    {
    char *id = gp->name;
    safef(query, sizeof(query), 
    	"select * from %s where name='%s'"
	" and chrom='%s' and txStart=%d and txEnd=%d", 
    	table, id, gp->chrom, gp->start, gp->end);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct genePred *gene = genePredLoad(row+hasBin);
	if (!fiveOnly || hasUtr5(gene))
	    {
	    struct dnaSeq *seq = genePromoSeq(gene, upSize, downSize);
	    hPrintf(">%s (promoter %d %d)", id, upSize, downSize);
	    printNameAndDescription(conn2, gp, nameCol, descCol);
	    hPrintf("\n");
	    writeSeqWithBreaks(stdout, seq->dna, seq->size, 50);
	    dnaSeqFree(&seq);
	    }
	genePredFree(&gene);
	}
    }
hPrintf("</TT></PRE>");
hFreeConn(&conn2);
}

static void getGenomic(struct sqlConnection *conn, 
	struct column *colList, struct genePos *geneList)
/* Put up dialog to get genomic sequence. */
{
struct hTableInfo *hti = hFindTableInfo(database, NULL, genomeSetting("geneTable"));
hPrintf("<H2>Get Genomic Sequence Near Gene</H2>");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=GET>\n");
cartSaveSession(cart);
hgSeqOptionsHtiCart(hti, cart);
hPrintf("<BR>\n");
cgiMakeButton(getGenomicSeqVarName, "get sequence");
hPrintf("</FORM>");
}

void doGetGenomicSeq(struct sqlConnection *conn, struct column *colList,
	struct genePos *geneList)
/* Retrieve genomic sequence sequence according to options. */
{
char *table = genomeSetting("geneTable");
struct hTableInfo *hti = hFindTableInfo(database, NULL, table);
struct genePos *gp;
char query[256];
struct sqlResult *sr;
char **row;
boolean hasBin = hOffsetPastBin(database, NULL, table);

hPrintf("<TT><PRE>");
for (gp = geneList; gp != NULL; gp = gp->next)
    {
    char *id = gp->name;
    safef(query, sizeof(query), "select * from %s where name='%s'", 
    	table, id);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct genePred *gene = genePredLoad(row+hasBin);
	struct bed *bed = bedFromGenePred(gene);
	hgSeqBed(database, hti, bed);
	bedFree(&bed);
	genePredFree(&gene);
	}
    }
hPrintf("</TT></PRE>");
}

void doGetSeq(struct sqlConnection *conn, struct column *colList, 
	struct genePos *geneList, char *how)
/* Put up the get sequence page. */
{
if (sameString(how, "protein"))
    getProtein(conn, colList, geneList);
else if (sameString(how, "mRNA"))
    getMrna(conn, colList, geneList);
else if (sameString(how, "promoter"))
    getPromoter(conn, colList, geneList);
else if (sameString(how, "genomic"))
    getGenomic(conn, colList, geneList);
else
    errAbort("Unrecognized %s value %s", getSeqHowVarName, how);
}

static void howRadioButton(char *how)
/* Put up a getSeqHow radio button. */
{
char *howName = getSeqHowVarName;
char *oldVal = cartUsualString(cart, howName, "protein");
cgiMakeRadioButton(howName, how, sameString(how, oldVal));
}

void doGetSeqPage(struct sqlConnection *conn, struct column *colList)
/* Put up the get sequence page asking how to get sequence. */
{
hPrintf("<H2>Get Sequence</H2>");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=GET>\n");
cartSaveSession(cart);
hPrintf("Select sequence type:<BR>\n");
howRadioButton("protein");
hPrintf("Protein<BR>\n");
howRadioButton("mRNA");
hPrintf("mRNA<BR>\n");
howRadioButton("promoter");
hPrintf("Promoter including ");
cgiMakeIntVar(proUpSizeVarName,
	cartUsualInt(cart, proUpSizeVarName, 1000), 4);
hPrintf(" bases upstream and ");
cgiMakeIntVar(proDownSizeVarName,
	cartUsualInt(cart, proDownSizeVarName, 50), 3);
hPrintf(" downstream.<BR>\n");
hPrintf("&nbsp;&nbsp;&nbsp;");
cgiMakeCheckBox(proIncludeFiveOnly, 
    cartUsualBoolean(cart, proIncludeFiveOnly, TRUE));
hPrintf("Include only those with annotated 5' UTRs<BR>");
howRadioButton("genomic");
hPrintf("Genomic<BR>\n");
cgiMakeButton(getSeqVarName, "get sequence");
hPrintf("</FORM>\n");
}

