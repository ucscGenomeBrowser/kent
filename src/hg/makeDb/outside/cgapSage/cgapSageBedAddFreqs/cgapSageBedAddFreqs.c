/* cgapSageBedAddFreqs - Add frequency data to the bed. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "sqlNum.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "verbose.h"
#include "bed.h"
#include "cgapSage/cgapSage.h"
#include "cgapSage/cgapSageLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cgapSageBedAddFreqs - Add frequency data to the bed.\n"
  "usage:\n"
  "   cgapSageBedAddFreqs mappings.bed frequecies.txt libraries.txt output.bed\n"
  "where mappings.bed is in BED8 format and the output is an extra 4 columns.\n"
  "options:\n"
  "   -noEmpty    If a tag doesn't have frequency data for it, don't include it in the final bed.\n"
  "   -verbose    Output status messages to stderr."
  );
}

static struct optionSpec options[] = {
    {"noEmpty", OPTION_BOOLEAN},
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};

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
struct hash *freqHash = newHash(21);
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
return totTagsHash;
}

void hashElSlPairListFree(struct hashEl *el)
/* Free up the list in one of the hashEls. */
{
struct slPair **pList = (struct slPair **)&el->val;
slFreeList(pList);
}

void freeFreqHash(struct hash **pFreqHash)
/* Free up the hash we created. */
{
hashTraverseEls(*pFreqHash, hashElSlPairListFree);
hashFree(pFreqHash);
}

void dumpHashEl(struct hashEl *el)
/* Print out a hashEl for debugging. */
{
struct slPair *list = el->val, *cur;
printf("el->name = %s\n", el->name);
for (cur = list; cur != NULL; cur = cur->next)
    printf("\tlib = %s, freq = %d\n", cur->name, ptToInt(cur->val));
}

struct cgapSage *cloneBedAddStuff(struct hash *freqHash, struct hash *totTagsHash, struct bed *oneBed)
/* Do a shallow copy of the bed into the cgapSage struct.  Later the original */
/* bed list should be freed with slFreeList instead of bedFreeList. */
{
struct cgapSage *newCgap;
struct slPair *list = hashFindVal(freqHash, oneBed->name); 
struct slPair *cur;
int ix = 0;
AllocVar(newCgap);
newCgap->chrom = oneBed->chrom;
newCgap->chromStart = oneBed->chromStart;
newCgap->chromEnd = oneBed->chromEnd;
newCgap->name = oneBed->name;
newCgap->score = oneBed->score;
newCgap->strand[0] = oneBed->strand[0];
newCgap->thickStart = oneBed->thickStart;
newCgap->thickEnd = oneBed->thickEnd;
newCgap->numLibs = slCount(list);
if (newCgap->numLibs > 0)
    {
    AllocArray(newCgap->libIds, newCgap->numLibs);
    AllocArray(newCgap->freqs, newCgap->numLibs);
    AllocArray(newCgap->tagTpms, newCgap->numLibs);
    for (cur = list; cur != NULL; cur = cur->next)
	{
	double totalTags = (double)hashIntVal(totTagsHash, cur->name);
	newCgap->libIds[ix] = sqlUnsigned(cur->name);
	newCgap->freqs[ix] = (unsigned)ptToInt(cur->val);
	newCgap->tagTpms[ix] = (double)newCgap->freqs[ix] * (1000000 / totalTags);
	ix++;
	}
    }
else if (optionExists("noEmpty"))
    {
    cgapSageFree(&newCgap);
    }
return newCgap;
}

struct cgapSage *makeCgapSageList(struct hash *freqHash, struct hash *totTagsHash, struct bed *bedList)
/* Make a new bed list using the old one and the hash.  This is the crux of */
/* the whole program pretty much. */
{
struct cgapSage *newList = NULL;
struct bed *oneBed;
for (oneBed = bedList; oneBed != NULL; oneBed = oneBed->next)
    {
    struct cgapSage *newOne = cloneBedAddStuff(freqHash, totTagsHash, oneBed);
    if (newOne)
	slAddHead(&newList, newOne);
    }
slReverse(&newList);
return newList;
}

void writeCgapSageFile(struct cgapSage *sageList, char *newBedFile)
/* Output the new list. */
{
FILE *f = mustOpen(newBedFile, "w");
struct cgapSage *sage;
for (sage = sageList; sage != NULL; sage = sage->next)
    cgapSageTabOut(sage, f);
carefulClose(&f);
}

void cgapSageBedAddFreqs(char *oldBedFile, char *freqFile, char *libsFile, char *newBedFile)
/* cgapSageBedAddFreqs - Add frequency data to the bed. */
{
struct hash *totTagsHash;
struct bed *mappings;
struct hash *freqHash;
struct cgapSage *sageList;
verbose(1, "Loading libraries...\n");
totTagsHash =  getTotTagsHash(libsFile);
verbose(1, "Loaded libraries.\n");
verbose(1, "Loading mappings...\n");
mappings = bedLoadNAll(oldBedFile, 8);
verbose(1, "Loaded mappings.\n");
verbose(1, "Loading frequencies...\n");
freqHash = getFreqHash(freqFile);
verbose(1, "Loaded frequencies.\n");
verbose(1, "Building new bed list...\n");
sageList = makeCgapSageList(freqHash, totTagsHash, mappings);
verbose(1, "Built new bed list.\n");
verbose(1, "Writing output...\n");
writeCgapSageFile(sageList, newBedFile);
verbose(1, "Wrote output. All done!\n");
freeFreqHash(&freqHash);
hashFree(&totTagsHash);
cgapSageFreeList(&sageList);
slFreeList(&mappings);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
if (optionExists("verbose"))
    verboseSetLevel(1);
cgapSageBedAddFreqs(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
