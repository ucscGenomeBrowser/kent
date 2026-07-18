
/* chainInDel - output a bed file with indels within the given chain. */
#include "common.h"
#include <ctype.h>
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "twoBit.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainInDel - output a bed file with indels within the given chain\n"
  "usage:\n"
  "   chainIndel file.chain label indels.bed\n"
  "options:\n"
  "   -t2bit=target.2bit   left-normalize indels using the target sequence.\n"
  "                        Pure deletions are shifted to their leftmost\n"
  "                        equivalent position within a repeat/homopolymer.\n"
  "   -q2bit=query.2bit    additionally left-normalize pure insertions (requires\n"
  "                        -t2bit).  Without these options the output is\n"
  "                        identical to the historic behavior.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"t2bit", OPTION_STRING},
   {"q2bit", OPTION_STRING},
   {NULL, 0},
};

/* Optional 2bit sources for left-normalization.  When NULL the tool behaves
 * exactly as it always has. */
static struct twoBitFile *tTbf = NULL;
static struct twoBitFile *qTbf = NULL;
static struct hash *tSeqHash = NULL;   /* tName -> struct dnaSeq * */
static struct hash *qSeqHash = NULL;   /* qName+strand -> struct dnaSeq * (chain orientation) */

static char *getTseq(char *name)
/* Return cached whole target sequence (chain/browser orientation). */
{
struct dnaSeq *seq = hashFindVal(tSeqHash, name);
if (seq == NULL)
    {
    seq = twoBitReadSeqFrag(tTbf, name, 0, 0);
    hashAdd(tSeqHash, name, seq);
    }
return seq->dna;
}

static char *getQseq(char *name, char strand)
/* Return cached whole query sequence in chain orientation (reverse
 * complemented for '-' strand chains, matching the block q coordinates). */
{
char key[512];
safef(key, sizeof(key), "%s%c", name, strand);
struct dnaSeq *seq = hashFindVal(qSeqHash, key);
if (seq == NULL)
    {
    seq = twoBitReadSeqFrag(qTbf, name, 0, 0);
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);
    hashAdd(qSeqHash, key, seq);
    }
return seq->dna;
}

static boolean baseEq(char a, char b)
/* Case-insensitive base compare. */
{
return toupper(a) == toupper(b);
}

int chainCmp(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct chain *a = *((struct chain **)va);
const struct chain *b = *((struct chain **)vb);
int ret = strcmp(a->qName, b->qName);
if (ret)
    return ret;
ret = a->tStart - b->tStart;
if (ret)
    return ret;

return  a->qStart - b->qStart;

}

static void parseChrom(struct chain *chains,  char *label, FILE *inDelF)
{
slSort(&chains, chainCmp);

struct chain *chain;
for(chain = chains; chain ; chain = chain->next)
    {
    struct cBlock *cb = chain->blockList;
    for(; cb->next != NULL;  cb = cb->next)
        {
        int tStart = cb->tEnd;              /* gap on target */
        int tEnd   = cb->next->tStart;
        int qGap   = cb->next->qStart - cb->qEnd;
        int tGap   = tEnd - tStart;

        /* Left-normalize a pure deletion using the target sequence: slide the
         * gap left while the base leaving the left block matches the base at
         * the right end of the deleted segment (repeat/homopolymer ambiguity). */
        if (tTbf != NULL && tGap > 0 && qGap == 0)
            {
            char *t = getTseq(chain->tName);
            int minT = cb->tStart + 1;      /* stay within the left block */
            while (tStart > minT && baseEq(t[tStart-1], t[tEnd-1]))
                { tStart--; tEnd--; }
            }
        /* Left-normalize a pure insertion: slide the zero-width target gap
         * left while the reference base to the left matches the last inserted
         * base on the query side.  Needs both target and query sequence. */
        else if (tTbf != NULL && qTbf != NULL && tGap == 0 && qGap > 0)
            {
            char *t = getTseq(chain->tName);
            char *q = getQseq(chain->qName, chain->qStrand);
            int p  = tStart;                /* insertion point on target */
            int qe = cb->next->qStart;      /* end of inserted window on query */
            int minT = cb->tStart + 1;      /* stay within the left block */
            int minQ = cb->qStart + 1;
            while (p > minT && qe-1 >= minQ && baseEq(t[p-1], q[qe-1]))
                { p--; qe--; }
            tStart = tEnd = p;
            }

        fprintf(inDelF,"%s\t%d\t%d\t%s\t%d\n", chain->tName,
            tStart, tEnd, label, qGap);

        }
    }
}

struct chainHead
{
struct chainHead *next;
struct chain *chains;
};

struct chainHead *readChains(char *inChain)
{
struct lineFile *lf = lineFileOpen(inChain, TRUE);
struct hash *hash = newHash(0);
struct chainHead *chainHead, *chainHeads = NULL;
struct chain *chain;

while ((chain = chainRead(lf)) != NULL)
    {
    if ((chainHead = hashFindVal(hash, chain->tName)) == NULL)
        {
        AllocVar(chainHead);
        slAddHead(&chainHeads, chainHead);
        hashAdd(hash, chain->tName, chainHead);
        }

    slAddHead(&chainHead->chains,  chain);
    }

return chainHeads;
}

void chainInDel(char *inChain, char *label, char *inDels)
/* chainInDel - output a set of rearrangement breakpoints. */
{
struct chainHead *chainHeads = readChains(inChain);
FILE *inDelF = mustOpen(inDels, "w");

for (; chainHeads != NULL; chainHeads = chainHeads->next)
    {
    struct chain  *chains = chainHeads->chains;
    parseChrom(chains,  label, inDelF);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

char *t2bit = optionVal("t2bit", NULL);
char *q2bit = optionVal("q2bit", NULL);
if (q2bit != NULL && t2bit == NULL)
    errAbort("-q2bit requires -t2bit");
if (t2bit != NULL)
    {
    dnaUtilOpen();
    tTbf = twoBitOpen(t2bit);
    tSeqHash = newHash(0);
    }
if (q2bit != NULL)
    {
    qTbf = twoBitOpen(q2bit);
    qSeqHash = newHash(0);
    }

chainInDel(argv[1], argv[2], argv[3]);
return 0;
}
