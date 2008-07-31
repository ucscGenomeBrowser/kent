/* rikenBestInCluster - Find best looking in Riken cluster. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "borf.h"
#include "rikenCluster.h"
#include "estOrientInfo.h"

static char const rcsid[] = "$Id: rikenBestInCluster.c,v 1.3.338.1 2008/07/31 02:24:43 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rikenBestInCluster - Find best looking in Riken cluster\n"
  "usage:\n"
  "   rikenBestInCluster database output.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hash *hashBorf(char *database)
/* Return hash of rikenBorf. */
{
struct hash *hash = newHash(16);
struct borf *borf;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;

sr = sqlGetResult(conn, "select * from rikenBorf");
while ((row = sqlNextRow(sr)) != NULL)
    {
    borf = borfLoad(row);
    hashAdd(hash, borf->name, borf);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return hash;
}

struct hash *hashOrient(char *database)
/* Return hash of rikenOrientInfo. */
{
struct hash *hash = newHash(16);
struct estOrientInfo *eoi;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset = 1;

sr = sqlGetResult(conn, "select * from rikenOrientInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    eoi = estOrientInfoLoad(row + rowOffset);
    hashAdd(hash, eoi->name, eoi);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return hash;
}

void writeBest(FILE *f, struct rikenUcscCluster *rc, struct hash *borfHash, 
	struct hash *orientHash)
/* Look in cluster for best Riken from ORF point of view and output it. */
{
struct borf *borf, *bestBorf = NULL;
struct estOrientInfo *eoi;
float bestScore = -1;
int i;

for (i=0; i<rc->rikenCount; ++i)
    {
    borf = hashMustFindVal(borfHash, rc->rikens[i]);
    if (borf->score > bestScore)
        {
	bestBorf = borf;
	bestScore = borf->score;
	}
    }
if (bestBorf != NULL)
    {
    eoi = hashMustFindVal(orientHash, bestBorf->name);
    fprintf(f, "%s\t%f\t%s\t%d\t%s:%d-%d\t%d\t%d\t%d\t%s\n", 
    	bestBorf->name, bestBorf->score, bestBorf->strand, eoi->intronOrientation,
	rc->chrom, rc->chromStart+1, rc->chromEnd,
    	rc->rikenCount, rc->genBankCount, rc->refSeqCount, rc->name);
    }
}

void rikenBestInCluster(char *database, char *fileName)
/* rikenBestInCluster - Find best looking in Riken cluster. */
{
FILE *f = mustOpen(fileName, "w");
struct hash *borfHash = hashBorf(database);
struct hash *orientHash = hashOrient(database);
/* Return hash of rikenOrientInfo. */
struct rikenUcscCluster *rc;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset = 1;

sr = sqlGetResult(conn, "select * from rikenCluster");
uglyf("sr = %p\n", sr);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rc = rikenUcscClusterLoad(row + rowOffset);
    writeBest(f, rc, borfHash, orientHash);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
rikenBestInCluster(argv[1], argv[2]);
return 0;
}
