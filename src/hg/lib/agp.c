/* Process AGP file */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "agp.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "linefile.h"
#include "hash.h"


void agpAddAll(char *agpFile, struct hash *agpHash)
/* add AGP entries from a file into a hash of AGP lists */
{
int i;
struct hashEl *hel;
struct hash *hash = agpLoadAll(agpFile);

if (hash == NULL || agpHash == NULL)
    return;

for (i = 0; i < hash->size; i++)
    {
    for (hel = hash->table[i]; hel != NULL; hel = hel->next)
	hashAdd(agpHash, hel->name, hel->val);
    }
}

struct agp *agpLoad(char **row, int ct)
/* Load an AGP entry from array of strings.  Dispose with agpFree */
{
struct agp *agp;
struct agpFrag *agpFrag;
struct agpGap *agpGap;

if (ct < 8)
    errAbort("Expecting >= 8 words in AGP file, got %d\n", ct);

AllocVar(agp);
if (row[4][0] != 'N' && row[4][0] != 'U')
    {
    /* not a gap */
    if (ct != 9)
        errAbort("Expecting 9 words in AGP fragment line, got %d\n", ct);
    agpFrag = agpFragLoad(row);
    agp->entry = agpFrag;
    agp->isFrag = TRUE;
    }
else
    {
    /* gap */
    agpGap = agpGapLoad(row);
    agp->entry = agpGap;
    agp->isFrag = FALSE;
    }
return agp;
}

void agpFree(struct agp **pAgp)
/* Free a dynamically allocated AGP entry */
{
struct agp *agp;

if ((agp = *pAgp) == NULL) return;
if (agp->isFrag)
    freeMem((struct agpFrag *)agp->entry);
else
    freeMem((struct agpGap *)agp->entry);
freez(pAgp);
}

void agpFreeList(struct agp **pList)
/* Free a list of dynamically allocated AGP entries. */
{
struct agp *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    agpFree(&el);
    }
*pList = NULL;
}

struct hash *agpLoadAll(char *agpFile)
/* load AGP entries into a hash of AGP lists, one per chromosome */
{
struct hash *agpHash = newHash(0);
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *words[9];
int lastPos = 0;
int wordCount;
struct agpFrag *agpFrag;
struct agpGap *agpGap;
char *chrom;
struct agp *agp;
struct hashEl *hel;

while ((wordCount = lineFileChopNext(lf, words, ArraySize(words))) != 0)
    {
    lineFileExpectAtLeast(lf, 8, wordCount);
    chrom = words[0];
    if (!hashFindVal(agpHash, chrom))
        lastPos = 1;
    AllocVar(agp);
    if (words[4][0] != 'N' && words[4][0] != 'U')
        {
        /* not a gap */
        lineFileExpectWords(lf, 9, wordCount);
        agpFrag = agpFragLoad(words);
        if (agpFrag->chromStart != lastPos)
            errAbort(
               "Frag start (%d, %d) doesn't match previous end line %d of %s\n",
                     agpFrag->chromStart, lastPos, lf->lineIx, lf->fileName);
        if (agpFrag->chromEnd - agpFrag->chromStart != 
                        agpFrag->fragEnd - agpFrag->fragStart)
            errAbort("Sizes don't match in %s and %s line %d of %s\n",
                    agpFrag->chrom, agpFrag->frag, lf->lineIx, lf->fileName);
        lastPos = agpFrag->chromEnd + 1;
        agp->entry = agpFrag;
        agp->isFrag = TRUE;
        }
    else
        {
        /* gap */
        lineFileExpectWords(lf, 8, wordCount);
        agpGap = agpGapLoad(words);
        if (agpGap->chromStart != lastPos)
            errAbort("Gap start (%d, %d) doesn't match previous end line %d of %s\n",
                     agpGap->chromStart, lastPos, lf->lineIx, lf->fileName);
        lastPos = agpGap->chromEnd + 1;
        agp->entry = agpGap;
        agp->isFrag = FALSE;
        }
    if ((hel = hashLookup(agpHash, chrom)) == NULL)
        hashAdd(agpHash, chrom, agp);
    else
        slAddHead(&(hel->val), agp);
    }
/* reverse AGP lists */
struct hashCookie cookie;
cookie = hashFirst(agpHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    slReverse(&hel->val);
    }
return agpHash;
}

void freeAgpHashEl(struct hashEl *agpHashEl)
/* Free this list up. */
{
struct agp *list = agpHashEl->val;
agpFreeList(&list);
}

void agpHashFree(struct hash **pAgpHash)
/* Free up the hash created with agpLoadAll. */
{
struct hash *agpHash = *pAgpHash;
hashTraverseEls(agpHash, freeAgpHashEl);
freeHash(&agpHash);
*pAgpHash = NULL;
}
