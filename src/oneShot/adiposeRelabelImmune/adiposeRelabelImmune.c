/* adiposeRelabelImmune - Merge together two files from the adipose tissue data set in GSE129363 to add immune subclusterings.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "adiposeRelabelImmune - Merge together two files from the adipose tissue data set in GSE129363 to add immune subclusterings.\n"
  "usage:\n"
  "   adiposeRelabelImmune meta.tsv\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void adiposeRelabelImmune(char *metaOut)
/* adiposeRelabelImmune - Merge together two files from the adipose tissue data set in GSE129363 to add immune subclusterings.. */
{
char *allIn = "CellClusterAnnotation.txt";
char *immuneIn = "ImmuneCell_ClusterAnnotation.txt";
char *metaIn = "GSE129363_Discovery_Cohort_CellAnnotation.txt";
struct hash *immuneHash = hashTwoColumnFile(immuneIn);
struct hash *allHash = hashTwoColumnFile(allIn);
struct lineFile *lf = lineFileOpen(metaIn, TRUE);
FILE *f = mustOpen(metaOut, "w");
char *row[4];

char *cluster;
while (lineFileNextRow(lf, row, ArraySize(row)))
    {
    if (lf->lineIx == 1)  // header
	cluster = "cellType";
    else
	{
	char *cell = row[0];
	char *oldCluster = hashFindVal(allHash, cell);
	cluster = hashFindVal(immuneHash, cell);
	if (cluster == NULL)
	     cluster = oldCluster;
	if (cluster == NULL)
	     errAbort("Can't find cluster for cell %s", cell);
	}
    fprintf(f, "%s\t%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3], cluster);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
adiposeRelabelImmune(argv[1]);
return 0;
}
