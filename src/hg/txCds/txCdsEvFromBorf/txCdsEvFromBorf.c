/* txCdsEvFromBorf - Convert borfBig format to txCdsEvidence (tce) in the effort 
 * of annotating the coding regions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "borf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsEvFromBorf - Convert borfBig format to txCdsEvidence (tce) in the effort \n"
  "of annotating the coding regions.\n"
  "usage:\n"
  "   txCdsEvFromBorf input.borf input.fa output.tce\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *faReadAllIntoHash(char *fileName)
/* Return hash full of dnaSeq (lower case) from file */
{
struct dnaSeq *seq, *list = faReadAllDna(fileName);
struct hash *hash = hashNew(18);
for (seq = list; seq != NULL; seq = seq->next)
    {
    if (hashLookup(hash, seq->name))
        errAbort("%s duplicated in %s", seq->name, fileName);
    hashAdd(hash, seq->name, seq);
    }
return hash;
}

void txCdsEvFromBorf(char *inBorf, char *txFa, char *outTce)
/* txCdsEvFromBorf - Convert borfBig format to txCdsEvidence (tce) in the effort 
 * of annotating the coding regions.. */
{
struct lineFile *lf = lineFileOpen(inBorf, TRUE);
struct hash *txHash = faReadAllIntoHash(txFa);
char *row[BORF_NUM_COLS];
FILE *f = mustOpen(outTce, "w");
while (lineFileRowTab(lf, row))
    {
    struct borf b;
    borfStaticLoad(row, &b);
    if (b.strand[0] == '+' && b.score >= 50)
	{
	struct dnaSeq *txSeq = hashFindVal(txHash, b.name);
	if (txSeq == NULL)
	    errAbort("%s is in %s but not %s", b.name, inBorf, txFa);
	int score = (b.score - 50)*5;
	if (score > 1000) score = 1000;
	if (score < 0) score = 0;
	fprintf(f, "%s\t", b.name);
	fprintf(f, "%d\t", b.cdsStart);
	fprintf(f, "%d\t", b.cdsEnd);
	fprintf(f, "%s\t", "bestorf");
	fprintf(f, "%s\t", ".");
	fprintf(f, "%d\t", score);
	fprintf(f, "%d\t", startsWith("atg", txSeq->dna + b.cdsStart));
	fprintf(f, "%d\t", isStopCodon(txSeq->dna + b.cdsEnd - 3));
	fprintf(f, "%d\t", 1);	
	fprintf(f, "%d,\t", b.cdsStart);
	fprintf(f, "%d,\n", b.cdsEnd - b.cdsStart);
	}
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
txCdsEvFromBorf(argv[1], argv[2], argv[3]);
return 0;
}
