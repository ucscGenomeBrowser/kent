/* twoBitDup - check to see if a twobit file has any identical sequences in it. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "twoBit.h"
#include "dnaseq.h"
#include "math.h"
#include "udc.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twoBitDup - check to see if a twobit file has any identical sequences in it\n"
  "usage:\n"
  "   twoBitDup file.2bit\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void twoBitDup(char *filename)
/* twoBitDup - check to see if a twobit file has any identical sequences in it. */
{
struct twoBitFile *tbf;

tbf = twoBitOpen(filename);
struct twoBitIndex *index;
int seqCount = slCount(tbf->indexList);
int hashSize = log2(seqCount) + 2;	 // +2 for luck
struct hash *seqHash = newHash(hashSize);

verbose(2, "hash size is %d\n", hashSize);

for (index = tbf->indexList; index != NULL; index = index->next)
    {
    verbose(2,"grabbing seq %s\n", index->name);
    int size;
    struct dnaSeq *seq = twoBitReadSeqFragExt(tbf, index->name,
	0, 0, FALSE, &size);
    struct hashEl *hel;
    if ((hel = hashLookup(seqHash, seq->dna)) != NULL)
	printf("%s and %s are identical\n", index->name, (char *)hel->val);
    else
	hashAdd(seqHash, seq->dna, index->name);
    freeDnaSeq(&seq);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
twoBitDup(argv[1]);
return 0;
}
