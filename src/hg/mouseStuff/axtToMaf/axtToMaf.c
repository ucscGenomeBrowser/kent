/* axtToMaf - Convert from axt to maf format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"
#include "obscure.h"
#include "maf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToMaf - Convert from axt to maf format\n"
  "usage:\n"
  "   axtToMaf in.axt tSizes qSizes out.maf\n"
  "Where tSizes and qSizes is a file that contains\n"
  "the sizes of the target and query sequences.\n"
  "Very often this with be a chrom.sizes file\n"
  );
}

struct hash *loadIntHash(char *fileName)
/* Read in a file full of name/number lines into a hash keyed
 * by name with number values. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = newHash(0);

while (lineFileRow(lf, row))
    {
    int num = lineFileNeedNum(lf, row, 1);
    hashAdd(hash, row[0], intToPt(num));
    }
return hash;
}

int findInt(struct hash *hash, char *name)
/* Return integer value corresponding to name. */
{
return ptToInt(hashMustFindVal(hash, name));
}

void axtToMaf(char *in, char *tSizes, char *qSizes, char *out)
/* axtToMaf - Convert from axt to maf format. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
struct hash *tSizeHash = loadIntHash(tSizes);
struct hash *qSizeHash = loadIntHash(qSizes);
struct axt *axt;

mafWriteStart(f, "blastz");
while ((axt = axtRead(lf)) != NULL)
    {
    struct mafAli *ali;
    struct mafComp *comp;
    AllocVar(ali);
    ali->score = axt->score;
    AllocVar(comp);
    comp->src = axt->qName;
    axt->qName = NULL;
    comp->srcSize = findInt(qSizeHash, comp->src);
    comp->strand = axt->qStrand;
    comp->start = axt->qStart;
    comp->size = axt->qEnd - axt->qStart;
    comp->text = axt->qSym;
    axt->qSym = NULL;
    slAddHead(&ali->components, comp);
    AllocVar(comp);
    comp->src = axt->tName;
    axt->tName = NULL;
    comp->srcSize = findInt(tSizeHash, comp->src);
    comp->strand = axt->tStrand;
    comp->start = axt->tStart;
    comp->size = axt->tEnd - axt->tStart;
    comp->text = axt->tSym;
    axt->tSym = NULL;
    slAddHead(&ali->components, comp);
    mafWrite(f, ali);
    mafAliFree(&ali);
    axtFree(&axt);
    }
mafWriteEnd(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
axtToMaf(argv[1],argv[2],argv[3],argv[4]);
return 0;
}
