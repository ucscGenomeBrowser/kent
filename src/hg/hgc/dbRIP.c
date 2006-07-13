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

static char const rcsid[] = "$Id: dbRIP.c,v 1.1 2006/07/13 18:04:06 hiram Exp $";

extern void genericClickHandlerPlus(
        struct trackDb *tdb, char *item, char *itemForUrl, char *plus);

void dbRIP(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up dbRIP track info */
{
struct sqlConnection *conn = hAllocConn();
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

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
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
    printf("<B>Associated disease:</B>&nbsp;%s<BR>\n", loadItem->disease);
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
    printf("<B>Genome Source:</B>&nbsp;%s<BR>\n", loadItem->polySource);
    printf("<B>Remarks:</B>&nbsp;%s<BR>\n", loadItem->remarks);
    printf("<B>Polymorphism Frequencies and Genotypes:</B>&nbsp;%s<BR>\n", loadItem->genoRegion);
    if (strlen(loadItem->genoRegion) > 0)
	printf("<B>Genomic region:</B>&nbsp;%s<BR>\n", loadItem->genoRegion);
    else
	printf("<B>Genomic region:</B>&nbsp;NA<BR>\n");
    printPosOnChrom(loadItem->chrom, loadItem->chromStart, loadItem->chromEnd,
	loadItem->strand, TRUE, loadItem->name);
    printf("<HR>\n<B>Reference(s):</B>&nbsp;%s\n<HR>\n", loadItem->reference);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}
