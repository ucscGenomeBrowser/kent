/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "dystring.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "dnaLoad.h"
#include "bed.h"
#include "snp.h"
#include "cgapSage/cgapSage.h"
#include "cgapSage/cgapSageLib.h"
#include "cgapSageFind.h"

int slPairValCmpReverse(const void *va, const void *vb)
/* Compare two slPairs for sorting in reverse order on */
/* the assumption that the stuff in the ->vals are ints. */
{
const struct slPair *a = *((struct slPair **)va);
const struct slPair *b = *((struct slPair **)vb);
int aVal = ptToInt(a->val);
int bVal = ptToInt(b->val);
return bVal - aVal;
}

void sortSlPairList(struct hashEl *listEl)
/* To be called by hashTraverseEls() so sort each list in */
/* the hash. */
{
struct slPair **pPairList = (struct slPair **)&listEl->val;
slSort(pPairList, slPairValCmpReverse);
}

void addFreqToHash(struct hash *freqHash, char *tag, char *id, int val)
/* Add the frequency to the end of the list in the hash. */
/* This is done at each line in the frequencies file. */
{ 
struct hashEl *el = hashStore(freqHash, tag);
struct slPair *newOne;
struct slPair **pList = (struct slPair **)&el->val;
AllocVar(newOne);
newOne->name = cloneString(id);
newOne->val = intToPt(val);
slAddHead(pList, newOne);
}

struct hash *getFreqHash(char *freqFile)
/* Read the frequency file in, and store it in a hash and return that. */
{
struct hash *freqHash = newHash(23);
struct lineFile *lf = lineFileOpen(freqFile, TRUE);
char *words[3];
/* Assume there's a header and skip it. */
lineFileSkip(lf, 1);
while (lineFileRowTab(lf, words))
    {
    int val;
    lineFileNeedFullNum(lf, words, 1);
    lineFileNeedFullNum(lf, words, 2);
    val = (int)sqlUnsigned(words[2]);
    addFreqToHash(freqHash, words[0], words[1], val);
    }
lineFileClose(&lf);
hashTraverseEls(freqHash, sortSlPairList);
return freqHash;
}

struct hash *getTotTagsHash(char *libsFile)
/* Read in the library file and hash up the total tags. */
{
struct hash *totTagsHash = newHash(9);
struct cgapSageLib *libs = cgapSageLibLoadAllByTab(libsFile);
struct cgapSageLib *lib;
for (lib = libs; lib != NULL; lib = lib->next)
    {
    char buf[16];
    safef(buf, sizeof(buf), "%d", lib->libId);
    hashAddInt(totTagsHash, buf, (int)lib->totalTags);
    }
cgapSageLibFreeList(&libs);
return totTagsHash;
}

void hashElSlPairListFree(struct hashEl **pEl)
/* Free up the list in one of the hashEls. */
{
struct slPair **pList = (struct slPair **)pEl;
slPairFreeList(pList);
}

int pickApartSeqName(char **pName)
/* Change /path/chr:start-end into chr and return start. */
{
char *name;
char *words[3];
int numWords = 0;
char *chrom, *range;
int skip = 0;
int start = 0;
if (!pName || ((name = *pName) == NULL))
    return 0;
numWords = chopByChar(name, ':', words, sizeof(words));
if (numWords == 3)
    skip = 1;
chrom = words[0 + skip];
*pName = chrom;
range = words[1 + skip];
if (numWords > 1)
    {
    chopPrefixAt(range, '-');
    start = sqlUnsigned(range);
    }
return start;
}
