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

static char const rcsid[] = "$Id: sortGenes.c,v 1.11 2009/01/17 02:07:18 galt Exp $";

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

/* if marker labels were present when the file was uploaded, they are saved here */
char cgmName[256];
safef(cgmName, sizeof(cgmName), "%s.cgm", gg->binFileName);
struct lineFile *m = lineFileMayOpen(cgmName, TRUE);
char *cgmRow[4];
cgmRow[0] = "";    /* dummy row */
cgmRow[1] = "";
cgmRow[2] = "0";
cgmRow[3] = "0";

FILE *g = NULL;
int markerCount = 0;
struct tempName snpTn;

if (m)
    {
    /* Create custom column output file. */
    trashDirFile(&snpTn, "hgg", "marker", ".mrk");   // TODO maybe something better?
    g = mustOpen(snpTn.forCgi, "w");
    // TODO DEBUG maybe a better value for name here? 
    fprintf(g, 
	"column name=\"%s Markers\" shortLabel=\"%s Markers over threshold\" longLabel=\"%s Markers in regions over threshold\" " 
	"visibility=on priority=99 "
	//"itemUrl=http://www.ncbi.nlm.nih.gov/SNP/snp_ref.cgi?type=rs&rs=%%s "
	//"labelUrl=http://genome.ucsc.edu search=fuzzy"
        "\n"
        , gg->shortLabel
        , gg->shortLabel
        , gg->shortLabel
	);
    }

/*** Build up hash of all transcriptHash that are in region. */
struct hash *transcriptHash = hashNew(16);

/* This loop handles one chromosome at a time.  It depends on
 * the bedList being sorted by chromosome. */
for (bed = bedList; bed != NULL; )
    {

    // DEBUG remove
    //fprintf(stderr, "GALT: hgGenome sortGenes chrom: %s\n", bed->chrom); fflush(stderr);

    /* Make binKeeper and stuff in all regions in this chromosome into it. */
    char *chrom = bed->chrom;
    int chromSize = hChromSize(database, chrom);
    struct binKeeper *bk = binKeeperNew(0, chromSize);
    while (bed != NULL && sameString(chrom, bed->chrom))
	{
	binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
	bed = bed->next;
	}

    struct binKeeper *bkGenes = NULL;
    if (m)
       bkGenes = binKeeperNew(0, chromSize);

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
	    {
	    hashStore(transcriptHash, name);
	    if (m)
		binKeeperAdd(bkGenes, start, end, cloneString(name));
	    }
	}
    sqlFreeResult(&sr);

    if (m)
	{
	/* Read cgm file if it exists, looking at all markers on this chromosome
	 * and if they overlap any of the regions and genes then output them. */
	do 
	    {
	    // marker, chrom, chromStart, val
	    char *marker = cgmRow[0];
	    char *chr = cgmRow[1];
	    int start = sqlUnsigned(cgmRow[2]);
	    int end = start+1;
	    double val = sqlDouble(cgmRow[3]);
            int cmp = strcmp(chr,chrom);
            if (cmp > 0)
                break;
            if (cmp == 0)
		{
		if (val >= threshold)
		    {
		    struct binElement *el, *bkList = binKeeperFind(bkGenes, start, end);
		    for (el = bkList; el; el=el->next)
			{
			/* output to custom column trash file */
			fprintf(g, "%s %s\n", (char *)el->val, marker);
			}
		    if (bkList)
			{
			++markerCount;
			slFreeList(&bkList);
			}
		    }
		}
	    }
	while (lineFileRow(m, cgmRow));
	}

    /* Clean up for this chromosome. */
    binKeeperFree(&bk);

    if (m)
	{
	/* For speed, we do not free up the values (cloned the kg names earlier) */
	binKeeperFree(&bkGenes);  
	}

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
if (m)
    {
    hPrintf("There are %d markers in the regions over threshold that overlap knownGenes.<BR>\n", markerCount);
    hPrintf("Installed a Gene Sorter custom column called \"%s Markers\" with these markers.<BR>\n", gg->shortLabel);
    }

//if (m)  // DEBUG
//    hPrintf("DEBUG: %s.<BR>\n", snpTn.forCgi);

/* close custom column output file */
if (m)
    {
    lineFileClose(&m);
    carefulClose(&g);
    }

/* Stuff cart variable with name of file. */
char keyCartName[256];
safef(keyCartName, sizeof(keyCartName), "%s%s.keyFile",
	advFilterPrefix, idColumn);
cartSetString(cart, keyCartName, keyTn.forCgi);

cartSetString(cart, customFileVarName, snpTn.forCgi);

char snpVisCartName[256];
safef(snpVisCartName, sizeof(snpVisCartName), "%s%s.vis",
	colConfigPrefix, gg->shortLabel);
cartSetString(cart, snpVisCartName, "1");
// The examples show comment-lines starting with pound-char '#',
//  but these lines cause error messages,
//  we should just make it ignore these lines.
//  Is there any chance that we should try to make it 
//  be an appended field?
//  Did the err messages cause it to fail to actually write the output file? weird.

// near.col.MyLab_Data.vis 1
// near.customFile ../trash/near_genome_4e80_ef98e0.col

// change hgg and gs to use another subdir path for the col temp file?

hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=GET>\n");
cartSaveSession(cart);
hPrintf("<CENTER>");
cgiMakeButton("submit", "go to gene sorter");
hPrintf("</CENTER>");
hPrintf("</FORM>");

cartWebEnd();
}
