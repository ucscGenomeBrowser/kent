/* colKnownGene - columns having to do with knownGene and closely tables. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: knownGene.c,v 1.2 2003/06/21 05:54:42 kent Exp $";

static char *posFromRow3(char **row)
/* Convert chrom/start/end row to position. */
{
char buf[128];
char *chrom;
int start,end;

chrom = row[0];
start = sqlUnsigned(row[1]);
end = sqlUnsigned(row[2]);
safef(buf, sizeof(buf), "%s:%d-%d", chrom, start+1, end);
return cloneString(buf);
}

static char *genePredPosFromTable(char *table, char *geneId, 
	struct sqlConnection *conn)
/* Get genePred position from table. */
{
char query[256];
struct sqlResult *sr;
char *pos = NULL;
char **row;
safef(query, sizeof(query), "select chrom, txStart, txEnd from %s where name = '%s'",
	table, geneId);

sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    pos = posFromRow3(row);
sqlFreeResult(&sr);
return pos;
}

static char *genePredPosVal(struct column *col, char *geneId, 
	struct sqlConnection *conn)
/* Get genome position of genePred type table. */
{
return genePredPosFromTable(col->table, geneId, conn);
}

static void genePredPosPrint(struct column *col, char *geneId, 
	struct sqlConnection *conn)
/* Print genome position with hyperlink to browser. */
{
char *pos = col->cellVal(col, geneId, conn);
char *chrom;
int start, end;


hPrintf("<TD>");
if (pos == NULL)
    hPrintf("n/a");
else
    {
    char numBuf[32];
    hgParseChromRange(pos, &chrom, &start, &end);
    sprintLongWithCommas(numBuf, (start+end+1)/2);
    hPrintf("<A HREF=\"%s?db=%s&position=%s&%s\">",
	    hgTracksName(), database, pos, cartSidUrlString(cart));
    hPrintf("%s&nbsp;%s</A>", chrom, numBuf);
    freeMem(pos);
    }
hPrintf("</TD>");
}

static void genePredPosMethods(struct column *col, char *table)
/* Put in methods for genePred based positions. */
{
col->table = table;
col->exists = simpleTableExists;
col->cellVal = genePredPosVal;
col->cellPrint = genePredPosPrint;
}

char *knownPosVal(struct column *col, char *geneId, 
	struct sqlConnection *conn)
/* Get genome position of knownPos table.  Ok to have col NULL. */
{
char *pos = genePredPosFromTable("knownGene", geneId, conn);
if (pos == NULL)
    {
    char query[256];
    char buf[128];
    struct sqlResult *sr;
    char **row;
    safef(query, sizeof(query), "select tName, tStart, tEnd from %s where qName = '%s'",
	"all_mrna", geneId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	pos = posFromRow3(row);
    sqlFreeResult(&sr);
    }
return pos;
}

void setupColumnKnownPos(struct column *col, char *parameters)
/* Set up column that links to genome browser based on known gene
 * position. */
{
genePredPosMethods(col, "knownGene");
col->cellVal = knownPosVal;
}

void printKnownDetailsLink(struct column *col, char *geneId, 
	struct sqlConnection *conn)
/* Print a link to known genes details page. */
{
char *s = col->cellVal(col, geneId, conn);
if (s == NULL) 
    {
    hPrintf("<TD>n/a</TD>", s);
    }
else
    {
    char *pos = knownPosVal(NULL, geneId, conn);
    char *chrom;
    int start,end;
    hgParseChromRange(pos, &chrom, &start, &end);
    hPrintf("<TD>");
    hPrintf("<A HREF=\"../cgi-bin/hgc?%s&g=knownGene&i=%s\">",
	    cartSidUrlString(cart), geneId);
    hPrintf("%s</A></TD>", s);
    freeMem(pos);
    freeMem(s);
    }
}

void setupColumnLookupKnown(struct column *col, char *parameters)
/* Set up a column that links to details page for known genes. */
{
setupColumnLookup(col, parameters);
col->cellPrint = printKnownDetailsLink;
}

