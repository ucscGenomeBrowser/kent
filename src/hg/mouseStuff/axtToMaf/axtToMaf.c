/* axtToMaf - Convert from axt to maf format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"
#include "obscure.h"
#include "maf.h"

static char const rcsid[] = "$Id: axtToMaf.c,v 1.4 2003/05/09 15:21:42 kent Exp $";

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
  "Options:\n"
  "    -qPrefix=XX. - add XX. to start of query sequence name in maf\n"
  "    -tPrefex=YY. - add YY. to start of target sequence name in maf\n"
  );
}

char *qPrefix = "";
char *tPrefix = "";

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
    char srcName[128];
    AllocVar(ali);
    ali->score = axt->score;
    AllocVar(comp);
    safef(srcName, sizeof(srcName), "%s%s", qPrefix, axt->qName);
    comp->src = cloneString(srcName);
    comp->srcSize = findInt(qSizeHash, axt->qName);
    comp->strand = axt->qStrand;
    comp->start = axt->qStart;
    comp->size = axt->qEnd - axt->qStart;
    comp->text = axt->qSym;
    axt->qSym = NULL;
    slAddHead(&ali->components, comp);
    AllocVar(comp);
    safef(srcName, sizeof(srcName), "%s%s", tPrefix, axt->tName);
    comp->src = cloneString(srcName);
    comp->srcSize = findInt(tSizeHash, axt->tName);
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
qPrefix = optionVal("qPrefix", qPrefix);
tPrefix = optionVal("tPrefix", tPrefix);
if (argc != 5)
    usage();
axtToMaf(argv[1],argv[2],argv[3],argv[4]);
return 0;
}
