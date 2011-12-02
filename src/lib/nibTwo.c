/* nibTwo - Something to let you transparently access either
 * .2bit or .nib files. */

#include "common.h"
#include "hash.h"
#include "dnaseq.h"
#include "nib.h"
#include "twoBit.h"
#include "nibTwo.h"


struct nibTwoCache *nibTwoCacheNew(char *pathName)
/* Get something that will more or less transparently get sequence from 
 * nib files or .2bit. */ 
{
struct nibTwoCache *ntc;
AllocVar(ntc);
ntc->pathName = cloneString(pathName);
ntc->isTwoBit = twoBitIsFile(pathName);
if (ntc->isTwoBit)
    ntc->tbf = twoBitOpen(pathName);
else
    ntc->nibHash = newHash(10);
return ntc;
}

void nibTwoCacheFree(struct nibTwoCache **pNtc)
/* Free up resources associated with nibTwoCache. */
{
struct nibTwoCache *ntc = *pNtc;
if (ntc != NULL)
    {
    freez(&ntc->pathName);
    if (ntc->isTwoBit)
        twoBitClose(&ntc->tbf);
    else
        {
	struct hashEl *el, *list = hashElListHash(ntc->nibHash);
	struct nibInfo *nib;
	for (el = list; el != NULL; el = el->next)
	     {
	     nib = el->val;
	     nibInfoFree(&nib);
	     }
	hashElFreeList(&list);
	hashFree(&ntc->nibHash);
	}
    freez(pNtc);
    }
}

struct dnaSeq *nibTwoCacheSeq(struct nibTwoCache *ntc, char *seqName)
/* Return all of sequence. This will have repeats in lower case. */
{
if (ntc->isTwoBit)
    return twoBitReadSeqFrag(ntc->tbf, seqName, 0, 0);
else
    {
    struct nibInfo *nib = nibInfoFromCache(ntc->nibHash, ntc->pathName, seqName);
    return nibLdPart(nib->fileName, nib->f, nib->size, 0, nib->size);
    }
}

struct dnaSeq *nibTwoCacheSeqPartExt(struct nibTwoCache *ntc, char *seqName, int start, int size,
                                     boolean doMask, int *retFullSeqSize)
/* Return part of sequence. If *retFullSeqSize is non-null then return full
 * size of sequence (not just loaded part) there.   Sequence will be lower
 * case if doMask is false, mixed case (repeats in lower)
 * if doMask is true. */
{
if (ntc->isTwoBit)
    {
    return twoBitReadSeqFragExt(ntc->tbf, seqName, start, start+size,
                                doMask, retFullSeqSize);
    }
else
    {
    struct nibInfo *nib = nibInfoFromCache(ntc->nibHash, ntc->pathName, seqName);
    int opts = (doMask ? NIB_MASK_MIXED : 0);
    if (retFullSeqSize != NULL)
        *retFullSeqSize = nib->size;
    return nibLdPartMasked(opts, nib->fileName, nib->f, nib->size, start, size);
    }
}

struct dnaSeq *nibTwoCacheSeqPart(struct nibTwoCache *ntc, char *seqName, int start, int size,
	int *retFullSeqSize)
/* Return part of sequence. If *retFullSeqSize is non-null then return full size of
 * sequence (not just loaded part) there. This will have repeats in lower case. */
{
return nibTwoCacheSeqPartExt(ntc, seqName, start, size, TRUE, retFullSeqSize);
}

struct dnaSeq *nibTwoLoadOne(char *pathName, char *seqName)
/* Return sequence from a directory full of nibs or a .2bit file. 
 * The sequence will have repeats in lower case. */
{
struct dnaSeq *seq;
if (twoBitIsFile(pathName))
    {
    struct twoBitFile *tbf = twoBitOpen(pathName);
    seq = twoBitReadSeqFrag(tbf, seqName, 0, 0);
    twoBitClose(&tbf);
    }
else
    {
    char path[512];
    safef(path, sizeof(path), "%s/%s.nib", pathName, seqName);
    seq = nibLoadAllMasked(NIB_MASK_MIXED, path);
    }
return seq;
}

int nibTwoGetSize(struct nibTwoCache *ntc, char *seqName)
/* Return size of sequence. */
{
if (ntc->isTwoBit)
    return twoBitSeqSize(ntc->tbf, seqName);
else
    return nibInfoFromCache(ntc->nibHash, ntc->pathName, seqName)->size;
}

