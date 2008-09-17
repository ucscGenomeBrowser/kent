/* Sort Genes - handle click on sort genes button. Set up
 * things so can go to gene sorter on genes inside regions
 * over threshold. */

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "jksql.h"
#include "bed.h"
#include "cheapcgi.h"
#include "chromGraph.h"
#include "binRange.h"
#include "hdb.h"
#include "hPrint.h"
#include "../near/hgNear/hgNear.h"
#include "hgGenome.h"
#include "trashDir.h"

static char const rcsid[] = "$Id: sortGenes.c,v 1.9 2008/09/17 18:36:35 galt Exp $";

struct colTrack
/* Genome browser track/gene sorter column correspondence. */
    {
    char *track;
    char *column;
    };

void sortGenes(struct sqlConnection *conn)
/* Put up sort gene page. */
{
cartWebStart(cart, database, "Finding Candidate Genes for Gene Sorter");
if (!hgNearOk(database))
    errAbort("Sorry, gene sorter not available for this database.");

/* Get list of regions. */
struct genoGraph *gg = ggFirstVisible();
double threshold = getThreshold();
struct bed3 *bed, *bedList = regionsOverThreshold(gg);

/* Figure out what table and column are the sorter's main gene set. */
struct hash *genomeRa = hgReadRa(genome, database, "hgNearData", 
	"genome.ra", NULL);
char *geneTable = hashMustFindVal(genomeRa, "geneTable");
char *idColumn = hashMustFindVal(genomeRa, "idColumn");

/*** Build up hash of all transcriptHash that are in region. */
struct hash *transcriptHash = hashNew(16);

/* This loop handles one chromosome at a time.  It depends on
 * the bedList being sorted by chromosome. */
for (bed = bedList; bed != NULL; )
    {
    /* Make binKeeper and stuff in all regions in this chromosome into it. */
    char *chrom = bed->chrom;
    int chromSize = hChromSize(database, chrom);
    struct binKeeper *bk = binKeeperNew(0, chromSize);
    while (bed != NULL && sameString(chrom, bed->chrom))
	{
	binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	bed = bed->next;
	}

    /* Query database to find out bounds of all genes on this chromosome
     * and if they overlap any of the regions then put them in the hash. */
    char query[512];
    safef(query, sizeof(query), 
    	"select name,txStart,txEnd from %s where chrom='%s'", geneTable, chrom);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *name = row[0];
	int start = sqlUnsigned(row[1]);
	int end = sqlUnsigned(row[2]);
	if (binKeeperAnyOverlap(bk, start, end))
	    hashStore(transcriptHash, name);
	}

    /* Clean up for this chromosome. */
    binKeeperFree(&bk);
    }

/* Get list of all transcripts in regions. */
struct hashEl *el, *list = hashElListHash(transcriptHash);

/* Create file with all matching gene IDs. */
struct tempName keyTn;
trashDirFile(&keyTn, "hgg", "key", ".key");
FILE *f = mustOpen(keyTn.forCgi, "w");
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s\n", el->name);
carefulClose(&f);

/* Print out some info. */
hPrintf("Thresholding <i>%s</i> at %g. ", gg->shortLabel, threshold);
hPrintf("There are %d regions covering %lld bases.<BR>\n",
    slCount(bedList), bedTotalSize((struct bed*)bedList) );
hPrintf("Installed a Gene Sorter filter that selects only genes in these regions.<BR>\n");

/* Stuff cart variable with name of file. */
char keyCartName[256];
safef(keyCartName, sizeof(keyCartName), "%s%s.keyFile",
	advFilterPrefix, idColumn);
cartSetString(cart, keyCartName, keyTn.forCgi);

hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=GET>\n");
cartSaveSession(cart);
hPrintf("<CENTER>");
cgiMakeButton("submit", "go to gene sorter");
hPrintf("</CENTER>");
hPrintf("</FORM>");

cartWebEnd();
}
