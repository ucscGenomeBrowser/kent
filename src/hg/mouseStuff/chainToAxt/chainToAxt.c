/* chainToAxt - Convert from chain to axt file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "chain.h"
#include "nib.h"
#include "axt.h"
#include "chainToAxt.h"

static char const rcsid[] = "$Id: chainToAxt.c,v 1.2 2003/06/14 07:43:39 kent Exp $";

int maxGap = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainToAxt - Convert from chain to axt file\n"
  "usage:\n"
  "   chainToAxt in.chain tNibDir qNibDir out.axt\n"
  "options:\n"
  "   -maxGap=maximum gap sized allowed without breaking, default %d\n"
  , maxGap
  );
}

static struct optionSpec options[] = {
   {"maxGap", OPTION_INT},
   {NULL, 0},
};

struct dnaSeq *nibInfoLoadSeq(struct nibInfo *nib, int start, int size)
/* Load in a sequence in mixed case from nib file. */
{
return nibLdPartMasked(NIB_MASK_MIXED, nib->fileName, nib->f, nib->size, 
	start, size);
}

struct dnaSeq *nibInfoLoadStrand(struct nibInfo *nib, int start, int end,
	char strand)
/* Load in a mixed case sequence from nib file, from reverse strand if
 * strand is '-'. */
{
struct dnaSeq *seq;
int size = end - start;
assert(size >= 0);
if (strand == '-')
    {
    reverseIntRange(&start, &end, nib->size);
    seq = nibInfoLoadSeq(nib, start, size);
    reverseComplement(seq->dna, seq->size);
    }
else
    {
    seq = nibInfoLoadSeq(nib, start, size);
    }
return seq;
}

void doIt(char *inName, char *tNibDir, char *qNibDir, char *outName)
/* chainToAxt - Convert from chain to axt file. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct chain *chain = NULL;
struct axt *axtList = NULL, *axt = NULL;
struct dnaSeq *qSeq = NULL, *tSeq = NULL;
struct hash *nibHash = hashNew(0);
FILE *f = mustOpen(outName, "w");
struct nibInfo *qNib = NULL, *tNib = NULL;

while ((chain = chainRead(lf)) != NULL)
    {
    qNib = nibInfoFromCache(nibHash, qNibDir, chain->qName);
    tNib = nibInfoFromCache(nibHash, tNibDir, chain->tName);
    qSeq = nibInfoLoadStrand(qNib, chain->qStart, chain->qEnd, chain->qStrand);
    tSeq = nibInfoLoadStrand(tNib, chain->tStart, chain->tEnd, '+');
    axtList = chainToAxt(chain, qSeq, chain->qStart, tSeq, chain->tStart,
    	maxGap);
    for (axt = axtList; axt != NULL; axt = axt->next)
        axtWrite(axt, f);
    axtFreeList(&axtList);
    freeDnaSeq(&qSeq);
    freeDnaSeq(&tSeq);
    chainFree(&chain);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
maxGap = optionInt("maxGap", maxGap);
if (argc != 5)
    usage();
doIt(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
