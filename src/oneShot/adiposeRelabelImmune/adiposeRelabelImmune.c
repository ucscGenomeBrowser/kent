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
  "   adiposeRelabelImmune oldMeta.tsv immuneAnnotation.tsv newMeta.tsv\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void adiposeRelabelImmune(char *metaIn, char *immuneIn, char *metaOut)
/* adiposeRelabelImmune - Merge together two files from the adipose tissue data set in GSE129363 to add immune subclusterings.. */
{
struct hash *immuneHash = hashTwoColumnFile(immuneIn);
struct lineFile *lf = lineFileOpen(metaIn, TRUE);
FILE *f = mustOpen(metaOut, "w");
char *row[5];

char *cluster;
while (lineFileNextRow(lf, row, ArraySize(row)))
    {
    if (lf->lineIx == 1)  // header
	cluster = "cellType";
    else
	{
	char *cell = row[0];
	char *oldCluster = row[4];
	cluster = hashFindVal(immuneHash, cell);
	if (cluster == NULL)
	     cluster = oldCluster;
	}
    fprintf(f, "%s\t%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3], cluster);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
adiposeRelabelImmune(argv[1], argv[2], argv[3]);
return 0;
}
