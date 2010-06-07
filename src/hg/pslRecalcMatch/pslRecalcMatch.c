/* pslRecalcMatch - Recalculate match,mismatch,repMatch columns in psl file.  
 * This can be useful if the psl went through pslMap, or if you've added 
 * lower-case repeat masking after the fact. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "nib.h"
#include "twoBit.h"
#include "nibTwo.h"
#include "dnaLoad.h"
#include "dystring.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslRecalcMatch.c,v 1.3 2008/05/04 05:44:48 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslRecalcMatch - Recalculate match,mismatch,repMatch columns in psl file.\n"
  "This can be useful if the psl went through pslMap, or if you've added \n"
  "lower-case repeat masking after the fact\n"
  "usage:\n"
  "   pslRecalcMatch in.psl targetSeq querySeq out.psl\n"
  "where targetSeq is either a nib directory or a two bit file\n"
  "and querySeq is a fasta file, nib file, two bit file, or list\n"
  "of such files.  The psl's should be simple non-translated ones.\n"
  "This will work faster if the in.psl is sorted on target.\n"
  "options:\n"
  "   -ignoreQUniq - ignore everything after the last `-' in the qName field, that\n"
  "    is sometimes used to generate a unique identifier\n"
  );
}

static struct optionSpec options[] = {
   {"ignoreQUniq", OPTION_BOOLEAN},
   {NULL, 0},
};
static boolean ignoreQUniq = FALSE;


static char *getQName(char *qName)
/* get query name, optionally dropping trailing unique identifier.
 * WARNING: static return */
{
static struct dyString *buf = NULL;
if (ignoreQUniq)
    {
    if (buf == NULL)
        buf = dyStringNew(2*strlen(qName));
    dyStringClear(buf);
    char *dash = strrchr(qName, '-');
    if (dash == NULL)
        return qName;
    dyStringAppendN(buf, qName, (dash-qName));
    return buf->string;
    }
else
    return qName;
}

static void recalcMatches(struct psl *psl, struct dnaSeq *tSeq,
	int tSeqOffset, struct dnaSeq *qSeq)
/* Fill in psl with correct match/mismatch/repMatch. */
{
boolean isRev = (psl->strand[0] == '-');
int block;
int match = 0, repMatch = 0, nCount = 0, misMatch = 0;
if (psl->strand[1] != 0)
    errAbort("This utility only works on simple non-translated psls.");
if (isRev)
    reverseComplement(qSeq->dna, qSeq->size);
for (block=0; block < psl->blockCount; ++block)
    {
    int size = psl->blockSizes[block];
    char *qs = qSeq->dna + psl->qStarts[block];
    char *ts = tSeq->dna + psl->tStarts[block] - tSeqOffset;
    int i;
    for (i=0; i<size; ++i)
        {
	char q = qs[i], t = ts[i];
	char qu = toupper(q), tu = toupper(t);
	if (qu == 'N' || tu == 'N')
	    ++nCount;
	else if (isupper(t))
	    {
	    if (qu == tu)
		++match;
	    else
	        ++misMatch;
	    }
	else  /* islower(t) */
	    {
	    if (qu == tu)
		++repMatch;
	    else
	        ++misMatch;
	    }
	}
    }
if (isRev)
    reverseComplement(qSeq->dna, qSeq->size);
psl->match = match;
psl->misMatch = misMatch;
psl->repMatch = repMatch;
psl->nCount = nCount;
}

void pslRecalcMatch(char *inName, char *targetName, char *queryName, 
	char *outName)
/* pslRecalcMatch - Recalculate match,mismatch,repMatch columns in psl file.  
 * This can be useful if the psl went through pslMap, or if you've added 
 * lower-case repeat masking after the fact. */
{
struct nibTwoCache *tCache = nibTwoCacheNew(targetName);
struct dnaSeq *qSeqList = dnaLoadAll(queryName);
struct hash *qHash = dnaSeqHash(qSeqList);
struct psl *psl;
struct lineFile *lf = pslFileOpen(inName);
FILE *f = mustOpen(outName, "w");

while ((psl = pslNext(lf)) != NULL)
    {
    int tSize;
    struct dnaSeq *tSeqPart = nibTwoCacheSeqPart(tCache,
    	psl->tName, psl->tStart, psl->tEnd - psl->tStart, &tSize);
    struct dnaSeq *qSeq = hashMustFindVal(qHash, getQName(psl->qName));
    recalcMatches(psl, tSeqPart, psl->tStart, qSeq);
    pslTabOut(psl, f);
    dnaSeqFree(&tSeqPart);
    }
carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
ignoreQUniq = optionExists("ignoreQUniq");
pslRecalcMatch(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
