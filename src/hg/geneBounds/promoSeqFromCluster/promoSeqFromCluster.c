/* promoSeqFromCluster - Get promoter regions from cluster. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "dnaseq.h"
#include "fa.h"
#include "jksql.h"
#include "hdb.h"
#include "bed.h"

static char const rcsid[] = "$Id: promoSeqFromCluster.c,v 1.5 2008/09/03 19:18:37 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "promoSeqFromCluster - Get promoter regions from cluster\n"
  "usage:\n"
  "   promoSeqFromCluster database size clusterFile.kgg outDir out.lft\n"
  "clusterFile is the output from Eisen's cluster program in kmeans\n"
  "settings.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void fetchPromoter(char *database, char *rangeId, int size, FILE *f, FILE *liftFile)
/* Get promoter region and write it to file. */
{
struct sqlResult *sr;
char **row;
struct bed *bed;
char query[256];
char *table = "rnaCluster";
int hasBin = hIsBinned(database, table);
struct sqlConnection *conn = hAllocConn(database);
struct dnaSeq *seq;
int start, end, chromSize;
boolean isRc;

snprintf(query, sizeof(query), "select * from %s where name = '%s'", table, rangeId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row + hasBin, 12);
    chromSize = hChromSize(database, bed->chrom);
    isRc = (bed->strand[0] == '-');
    if (isRc)
        {
	start = bed->chromEnd;
	end = start + size;
	if (end > chromSize) 
	    end = chromSize;
	}
    else
        {
	end = bed->chromStart;
	start = end - size;
	if (start < 0) 
	    start = 0;
	}
    seq = hChromSeq(database, bed->chrom, start, end);
    if (bed->strand[0] == '-')
        reverseComplement(seq->dna, seq->size);
    faWriteNext(f, bed->name, seq->dna, seq->size);
    fprintf(liftFile, "%d\t%s\t%d\t%s\t%d\t%s\n",
    	start, bed->name, seq->size, bed->chrom, chromSize, bed->strand);
    freeDnaSeq(&seq);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void promoSeqFromCluster(char *database, int size, char *clusterFile, 
	char *outDir, char *outBed)
/* promoSeqFromCluster - Get promoter regions from cluster. */
{
char fileName[512];
struct lineFile *lf = lineFileOpen(clusterFile, TRUE);
FILE *f = NULL;
FILE *liftFile = mustOpen(outBed, "w");
char *row[3];
char *id, *group, *lastGroup = NULL;

makeDir(outDir);

lineFileRow(lf, row); /* Skip first row which just has labels. */

while (lineFileRow(lf, row))
    {
    id = row[0];
    group = row[2];
    if (lastGroup == NULL || !sameString(group, lastGroup))
        {
	/* If it's a new group make a new file for it. */
	freez(&lastGroup);
	lastGroup = cloneString(group);
	carefulClose(&f);
	snprintf(fileName, sizeof(fileName), "%s/%s.fa", outDir, group);
	f = mustOpen(fileName, "w");
	}
    fetchPromoter(database, id, size, f, liftFile);
    printf(".");
    fflush(stdout);
    }
printf("\n");
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 6)
    usage();
if (!isdigit(argv[2][0]))
    usage();
promoSeqFromCluster(argv[1], atoi(argv[2]), argv[3], argv[4], argv[5]);
return 0;
}
