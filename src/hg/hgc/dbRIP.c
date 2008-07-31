/* Handle details pages for dbRIP tracks. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hgc.h"
#include "hui.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "dbRIP.h"
#include "polyGenotype.h"

static char const rcsid[] = "$Id: dbRIP.c,v 1.7.100.1 2008/07/31 02:24:18 markd Exp $";


static int sortEthnicGroup(const void *e1, const void *e2)
/* used by slSort to sort the polyGenotype in order of ethnic group name */
{
const struct polyGenotype *p1 = *((struct polyGenotype**)e1);
const struct polyGenotype *p2 = *((struct polyGenotype**)e2);
return(strcmp(p1->ethnicGroup, p2->ethnicGroup));
}

static void polyTable(char *name)
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
struct polyGenotype *pgList = NULL;

sprintf(query, "select * from polyGenotype where name = '%s'", name);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct polyGenotype *pg;

    AllocVar(pg);
    pg = polyGenotypeLoad(row);
    slAddHead(&pgList, pg);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
if (slCount(pgList) > 0)
    {
    struct polyGenotype *pg;
    int totalPlusPlus = 0;
    int totalPlusMinus = 0;
    int totalMinusMinus = 0;
    int totalSamples = 0;
    double alleleFrequency = 0.0;
    double unbiasedHeterozygosity = 0.0;
    int sampleSize = 0;
    int plusAlleles = 0;
    int minusAlleles = 0;

    slSort(&pgList,sortEthnicGroup);

    printf("<B>Polymorphism Frequencies and Genotypes:</B><BR>\n");
    /*	This surrounding table avoids the problem of the border lines of
     *	the second table go missing.  Something about the style sheet ...
     */
    printf("<TABLE BGCOLOR=\"#FFFEE8\" CELLPADDING=0><TR><TD>\n");
    printf("<TABLE BORDER=2 bgcolor=\"#e4cdf2\">\n");
    printf("<TR bgcolor=\"#d2abf2\"><TH align=center>Ethnic Group</TH>");
    printf("<TH align=center>Sample Size</TH>");
    printf("<TH align=center>Allele Frequency</TH>");
    printf("<TH align=center>+/+</TH>");
    printf("<TH align=center>+/-</TH>");
    printf("<TH align=center>-/-</TH>");
    printf("<TH align=center>Unbiased Heterozygosity</TH>");
    printf("</TR>\n");
    for (pg = pgList; pg != NULL; pg = pg->next)
	{
	plusAlleles = (pg->plusPlus << 1) + pg->plusMinus;
	minusAlleles = (pg->minusMinus << 1) + pg->plusMinus;

	sampleSize = pg->plusPlus + pg->plusMinus + pg->minusMinus;
	if ((plusAlleles + minusAlleles) > 0)
	    {
	    alleleFrequency = (double)plusAlleles /
		(double)(plusAlleles + minusAlleles);
	    }
	else
	    alleleFrequency = 0.0;

	printf("<TR><TD align=left>%s</TD>", pg->ethnicGroup);
	printf("<TD align=center>%d</TD>", sampleSize);
	printf("<TD align=center>%.3f</TD>", alleleFrequency);
	printf("<TD align=center>%d</TD>", pg->plusPlus);
	printf("<TD align=center>%d</TD>", pg->plusMinus);
	printf("<TD align=center>%d</TD>", pg->minusMinus);

	/*	adjust by 2N/(2N-1) for small sample heterozygosity calculation
	 *	http://www.genetics.org/cgi/content/abstract/89/3/583
	 *	Masatoshi Nei (1978)
	 */
	if (sampleSize > 0)
	    {
	    unbiasedHeterozygosity=2*alleleFrequency*(1.0 - alleleFrequency);
	    unbiasedHeterozygosity *=
		(double)(sampleSize << 1) / (double)((sampleSize << 1) - 1);
	    }
	else
	    unbiasedHeterozygosity=2*alleleFrequency*(1.0 - alleleFrequency);

	printf("<TD align=center>%.3f</TD>", unbiasedHeterozygosity);
	printf("</TR>\n");

	totalSamples += sampleSize;
	totalPlusPlus += pg->plusPlus;
	totalMinusMinus += pg->minusMinus;
	totalPlusMinus += pg->plusMinus;
	}

    plusAlleles = (totalPlusPlus << 1) + totalPlusMinus;
    minusAlleles = (totalMinusMinus << 1) + totalPlusMinus;
    if ((plusAlleles + minusAlleles) > 0)
	{
	alleleFrequency = (double)plusAlleles /
		(double)(plusAlleles + minusAlleles);
	}
    else
	{
	alleleFrequency = 0.0;
	}
    printf("<TR><TH align=left>All Samples</TH>");
    printf("<TH align=center>%d</TH>", totalSamples);
    printf("<TH align=center>%.3f</TH>", alleleFrequency);
    printf("<TH align=center>%d</TH>", totalPlusPlus);
    printf("<TH align=center>%d</TH>", totalPlusMinus);
    printf("<TH align=center>%d</TH>", totalMinusMinus);
    if (totalSamples > 0)
	{
	unbiasedHeterozygosity = 2 * alleleFrequency * (1.0 - alleleFrequency);
	unbiasedHeterozygosity *= 
	    (double)(totalSamples << 1)/(double)((totalSamples << 1) - 1);
	}
    else
	unbiasedHeterozygosity = 2 * alleleFrequency * (1.0 - alleleFrequency);
    printf("<TH align=center>%.3f</TH>", unbiasedHeterozygosity);
    printf("</TR>\n");
    printf("</TABLE>\n");
    printf("</TD></TR></TABLE>\n");
    }
else
    printf("<B>Polymorphism Frequencies and Genotypes:</B>&nbsp;NA<BR>\n");
}

void dbRIP(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up dbRIP track info */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char table[64];
boolean hasBin;
struct dbRIP *loadItem;
struct dyString *query = newDyString(512);
char **row;
boolean firstTime = TRUE;
int start = cartInt(cart, "o");
int itemCount = 0;

genericHeader(tdb, item);

hFindSplitTable(database, seqName, tdb->tableName, table, &hasBin);
dyStringPrintf(query, "select * from %s where chrom = '%s' and ",
	       table, seqName);
hAddBinToQuery(winStart, winEnd, query);
dyStringPrintf(query, "name = '%s' and chromStart = %d", item, start);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    ++itemCount;
    loadItem = dbRIPLoad(row+hasBin);
    printf("<B>Database ID:</B>&nbsp;%s<BR>\n", loadItem->name);
    printf("<B>Original ID:</B>&nbsp;%s<BR>\n", loadItem->originalId);
    printf("<B>Class:</B>&nbsp;%s<BR>\n", loadItem->polyClass);
    printf("<B>Family:</B>&nbsp;%s<BR>\n", loadItem->polyFamily);
    printf("<B>Subfamily:</B>&nbsp;%s<BR>\n", loadItem->polySubfamily);
    printf("<B>Associated Disease:</B>&nbsp;%s<BR>\n", loadItem->disease);
    printf("<B>Sequence of L1 insertion and 400bp flanking on each side:</B>");
    printf("(<font color=blue>5' flanking-</font><font color=green>");
    printf("<u>TSD1</u>-</font><font color=red>REPEAT SEQUENCE</font>");
    printf("<font color=green>-<u>TSD2</u>-</font><font color=blue>3'");
    printf("flanking</font>)<br>(\"<font color=green><u>nnnnn</u></font>\"");
    printf(" --&gt; unknown TSD; \"<font color=red>NNNNNNNNNN</font>\" --&gt;");
    printf(" unknown Repeat Sequence):<BR>\n<PRE><font color=blue>%s", loadItem->polySeq);
    printf("</font></PRE><BR>\n");
    printf("<B>Forward Primer:</B>&nbsp;%s<BR>\n", loadItem->forwardPrimer);
    printf("<B>Reverse Primer:</B>&nbsp;%s<BR>\n", loadItem->reversePrimer);
    if (loadItem->tm > 0.0)
	printf("<B>Annealing Temperature:</B>&nbsp;%.1f&nbsp;&deg;C.<BR>\n", loadItem->tm);
    else
	printf("<B>Annealing Temperature:</B>&nbsp;NA<BR>\n");
    if (loadItem->filledSize > 0)
	printf("<B>PCR Product Size (Filled):</B>&nbsp;%d&nbsp;bp.<BR>\n", loadItem->filledSize);
    else
	printf("<B>PCR Product Size (Filled):</B>&nbsp;NA<BR>\n");
    if (loadItem->emptySize > 0)
	printf("<B>PCR Product Size (Empty):</B>&nbsp;%d&nbsp;bp.<BR>\n", loadItem->emptySize);
    else
	printf("<B>PCR Product Size (Empty):</B>&nbsp;NA<BR>\n");
    printf("<B>Ascertaining Method:</B>&nbsp;%s<BR>\n", loadItem->ascertainingMethod);
    printf("<B>Insertion found in reference sequence:</B>&nbsp;%s<BR>\n",
	startsWith("UCSC",loadItem->polySource) ? "yes" : "no");
    printf("<B>Remarks:</B>&nbsp;%s<BR>\n", loadItem->remarks);
    if (strlen(loadItem->genoRegion) > 0)
	printf("<B>Gene Context:</B>&nbsp;%s<BR>\n", loadItem->genoRegion);
    else
	printf("<B>Gene Context:</B>&nbsp;NA<BR>\n");
    polyTable(loadItem->name);
    printPosOnChrom(loadItem->chrom, loadItem->chromStart, loadItem->chromEnd,
	loadItem->strand, TRUE, loadItem->name);
    printf("<HR>\n<B>Reference(s):</B>&nbsp;%s\n<HR>\n", loadItem->reference);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}
