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

static char const rcsid[] = "$Id: getSeq.c,v 1.4 2003/08/21 16:58:40 kent Exp $";

static void printNameAndDescription(struct sqlConnection *conn, char *id)
/* Look up name and description and print. */
{
char **row;
char query[256];
struct sqlResult *sr;
char *name = "";
char *description = "";
safef(query, sizeof(query),
	"select geneSymbol,description from kgXref where kgID = '%s'", id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    name = row[0];
    description = row[1];
    }
hPrintf(" %s - %s", name, description);
sqlFreeResult(&sr);
}

static void getSeqFromBlob(struct sqlConnection *conn, struct genePos *geneList,
	char *tableName)
/* Get sequence from blob field in table and print it as fasta. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct genePos *gp;
struct sqlConnection *conn2 = hAllocConn();

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
	printNameAndDescription(conn2, id);
	hPrintf("\n");
	writeSeqWithBreaks(stdout, seq, strlen(seq), 60);
	}
    sqlFreeResult(&sr);
    }
hPrintf("</TT></PRE>");
hFreeConn(&conn2);
}

static void getProtein( struct sqlConnection *conn, struct genePos *geneList)
/* Print out proteins. */
{
getSeqFromBlob(conn, geneList, "knownGenePep");
}

static void getMrna(struct sqlConnection *conn, struct genePos *geneList)
/* Print out proteins. */
{
getSeqFromBlob(conn, geneList, "knownGeneMrna");
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
    seq = hChromSeq(gp->chrom, start, end);
    reverseComplement(seq->dna, seq->size);
    }
else
    {
    int start = gp->txStart - upSize;
    int end = gp->txStart + downSize;
    seq = hChromSeq(gp->chrom, start, end);
    }
return seq;
}

static void getPromoter(struct sqlConnection *conn, struct genePos *geneList)
/* Print out promoters. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct genePos *gp;
struct sqlConnection *conn2 = hAllocConn();
int upSize = cartInt(cart, proUpSizeVarName);
int downSize = cartInt(cart, proDownSizeVarName);
boolean fiveOnly = cartBoolean(cart, proIncludeFiveOnly);

hPrintf("<TT><PRE>");
for (gp = geneList; gp != NULL; gp = gp->next)
    {
    char *id = gp->name;
    safef(query, sizeof(query), "select * from knownGene where name='%s'", id);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct genePred *gene = genePredLoad(row);
	if (!fiveOnly || hasUtr5(gene))
	    {
	    struct dnaSeq *seq = genePromoSeq(gene, upSize, downSize);
	    hPrintf(">%s (promoter %d %d)", id, upSize, downSize);
	    printNameAndDescription(conn2, id);
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

static void getGenomic(struct sqlConnection *conn, struct genePos *geneList)
/* Put up dialog to get genomic sequence. */
{
struct hTableInfo *hti = hFindTableInfo(NULL, "knownGene");
hPrintf("<H2>Get Genomic Sequence Near Gene</H2>");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=GET>\n");
cartSaveSession(cart);
hgSeqOptionsHtiCart(hti, cart);
hPrintf("<BR>\n");
cgiMakeButton(getGenomicSeqVarName, "Get DNA");
hPrintf("</FORM>");
}

void doGetGenomicSeq(struct sqlConnection *conn, struct column *colList,
	struct genePos *geneList)
/* Retrieve genomic sequence sequence according to options. */
{
struct hTableInfo *hti = hFindTableInfo(NULL, "knownGene");
struct genePos *gp;
char query[256];
struct sqlResult *sr;
char **row;

hPrintf("<TT><PRE>");
for (gp = geneList; gp != NULL; gp = gp->next)
    {
    char *id = gp->name;
    safef(query, sizeof(query), "select * from knownGene where name='%s'", id);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct genePred *gene = genePredLoad(row);
	struct bed *bed = bedFromGenePred(gene);
	hgSeqBed(hti, bed);
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
    getProtein(conn, geneList);
else if (sameString(how, "mRNA"))
    getMrna(conn, geneList);
else if (sameString(how, "promoter"))
    getPromoter(conn, geneList);
else if (sameString(how, "genomic"))
    getGenomic(conn, geneList);
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
cgiMakeButton(getSeqVarName, "Get DNA");
hPrintf("</FORM>\n");
}

