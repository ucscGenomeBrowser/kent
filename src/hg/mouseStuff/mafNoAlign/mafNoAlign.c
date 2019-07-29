/* mafNoAlign - output a bed where there are no other sequences mapping to the reference sequence */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafNoAlign - output a bed where there are no other sequences mapping to the reference sequence\n"
  "NOTE: code expects first sequence in blocks to be the reference and to be on the '+' strand\n"
  "      and the sequence name to be in the form species.chromosome.\n"
  "usage:\n"
  "   mafNoAlign in.maf out.bed\n"
  );
}

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void mafNoAlign(char *in, char *out)
/* Output a bed where there are no other sequences mapping to the reference sequence. */
{
struct mafFile *mf = mafOpen(in);
FILE *f = mustOpen(out, "w");
struct mafAli *ma = NULL;
int prevChromSize = 0;
char *prevChrom = NULL;
int prevEnd = 0;

while ((ma = mafNext(mf)) != NULL)  // for each block in the maf
    {
    struct mafComp *mc;

    // ignore empty blocks
    for (mc = ma->components->next;  mc != NULL;  mc = mc->next)
        {
        if (mc->size != 0)  // here's some sequence
            break;
        }
    if (mc == NULL)  // empty block, ignore it
        {
        mafAliFree(&ma);
        continue;
        }

    // get chrom of reference sequence
    struct mafComp *mcRef = ma->components;
    char *chrom = strchr(mcRef->src, '.');
    if (chrom == NULL)
        errAbort("maf has reference not in species.chrom format on line %d\n",mf->lf->lineIx);
    chrom++;

    // if this is a new chrom, output the piece at the end of the previous chrom (if any)
    if ((prevChrom != NULL) && differentString(chrom, prevChrom))
        {
        if (prevEnd != prevChromSize)
            fprintf(f, "%s\t%d\t%d\n", prevChrom, prevEnd, prevChromSize);
        prevChrom = NULL;
        }

    // if this is a new chrom, save some data and restart our range
    if (prevChrom == NULL)  
        {
        prevChrom = cloneString(chrom);
        prevChromSize = mcRef->srcSize;
        prevEnd = 0;
        }

    // if this block starts after the end of the previous block, it's a gap
    int start = mcRef->start;
    if (prevEnd != start)
        fprintf(f, "%s\t%d\t%d\n", prevChrom, prevEnd, start);

    // remember the end to this block
    prevEnd = start + ma->components->size;

    mafAliFree(&ma);
    }

if ((prevChrom != NULL) && (prevEnd != prevChromSize))
    fprintf(f, "%s\t%d\t%d\n", prevChrom, prevEnd, prevChromSize);

mafFileFree(&mf);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
mafNoAlign(argv[1], argv[2]);
return 0;
}
