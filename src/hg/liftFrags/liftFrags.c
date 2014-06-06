/* liftFrags - This program lifts annotations on fragments to FPC contig 
 * coordinates. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "hash.h"
#include "obscure.h"
#include "linefile.h"
#include "agpFrag.h"
#include "rmskOut.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftFrags - This program lifts annotations on clone fragments\n"
  "to FPC contig coordinates\n"
  "usage:\n"
  "   liftFrags destFile gold.N geno.lst sourceDir\n"
  "Currently this only handles RepeatMasker .out files\n"
  "or psLayout .psl files\n");
}

struct agpFrag *readGold(char *fileName, struct hash *fragHash, int *retSize)
/* Read in fragments from gold file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[16];
int wordCount;
struct agpFrag *fragList = NULL, *frag;
int end, maxEnd = 0;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (words[4][0] != 'N' && words[4][0] != 'U')
        {
	lineFileExpectWords(lf, 9, wordCount);
	frag = agpFragLoad(words);
	// file is 1-based but agpFragLoad() now assumes 0-based:
	frag->chromStart -= 1;
	frag->fragStart  -= 1;
	slAddHead(&fragList, frag);
	hashAddUnique(fragHash, frag->frag, frag);
	end = frag->chromEnd;
	}
    else
        {
	end = atoi(words[2]);
	}
    if (end > maxEnd) maxEnd = end;
    }
lineFileClose(&lf);
slReverse(&fragList);
*retSize = maxEnd;
return fragList;
}

boolean changeRmskCoordinates(struct rmskOut *rmsk, struct agpFrag *frag, int destSize)
/* Change coordinates of rmsk from frag to contig or chromosome level. */
{
int s, e, start, end, offset;

start = max(rmsk->genoStart, frag->fragStart);
end = min(rmsk->genoEnd, frag->fragEnd);
if (start >= end)
    return FALSE;
if (frag->strand[0] == '+')
    {
    offset = frag->chromStart - frag->fragStart;
    s = start + offset;
    e = end + offset;
    }
else
    {
    offset = frag->chromStart;
    s = offset + frag->fragEnd - end;
    e = offset + frag->fragEnd - start;
    if (rmsk->strand[0] == '+') rmsk->strand[0] = '-';
    else rmsk->strand[0] = '+';
    }
freeMem(rmsk->genoName);
rmsk->genoName = cloneString(frag->chrom);
rmsk->genoStart = s;
rmsk->genoEnd = e;
rmsk->genoLeft = destSize - e;
return TRUE;
}

int rmskOutCmpGenoStart(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct rmskOut *a = *((struct rmskOut **)va);
const struct rmskOut *b = *((struct rmskOut **)vb);
return a->genoStart - b->genoStart;
}

void liftOut(char *destFile, struct hash *fragHash, int destSize, 
	char *genoLst, char *sourceDir)
/* Lift all .out files from genoLst.  Only lift fragments that are in fragHash.  */
{
struct rmskOut *inList, *outList = NULL, *rmsk, *rmskNext;
char *buf, **genoFiles, rmskFile[512];
int genoCount, i;
struct agpFrag *frag;
int totalCount = 0, liftedCount = 0;

readAllWords(genoLst, &genoFiles, &genoCount, &buf);
for (i = 0; i<genoCount; ++i)
    {
    sprintf(rmskFile, "%s.out", genoFiles[i]);
    inList = rmskOutRead(rmskFile);
    for (rmsk = inList; rmsk != NULL; rmsk = rmskNext)
        {
	rmskNext = rmsk->next;
	++totalCount;
	frag = hashFindVal(fragHash, rmsk->genoName);
	if (frag != NULL)
	    {
	    if (changeRmskCoordinates(rmsk, frag, destSize))
		{
		++liftedCount;
		slAddHead(&outList, rmsk);
		}
	    else
	        rmskOutFree(&rmsk);
	    }
	else
	    rmskOutFree(&rmsk);
	}
    }
printf("Writing %d repeats (of %d) to %s\n", liftedCount, totalCount, destFile);
slSort(&outList, rmskOutCmpGenoStart);
rmskOutWriteAllOut(destFile, outList);
}

boolean changePslCoordinates(struct psl *psl, struct agpFrag *frag, int destSize)
/* Change coordinates of psl from frag to contig or chromosome level. */
{
int s, e, start, end, offset;
int blockCount, j;
unsigned *tStarts;

start = psl->tStart;
end = psl->tEnd;
if (start >= end)
    return FALSE;
if (frag->strand[0] == '+')
    {
    offset = frag->chromStart - frag->fragStart;
    s = start + offset;
    e = end + offset;
    }
else
    {
    offset = frag->chromStart;
    s = offset + frag->fragEnd - end;
    e = offset + frag->fragEnd - start;
    if (psl->strand[0] == '+') psl->strand[0] = '-';
    else psl->strand[0] = '+';
    }
freeMem(psl->tName);
psl->tName = cloneString(frag->chrom);
psl->tStart = s;
psl->tEnd = e;
psl->tSize = destSize - e;
blockCount = psl->blockCount;
tStarts = psl->tStarts;
for (j=0; j<blockCount; ++j) 
   tStarts[j] += offset;

return TRUE;
}

void liftPsl(char *destFile, struct hash *fragHash, int destSize, 
	char *genoLst, char *sourceDir)
/* Lift all .psl files from genoLst.  Only lift fragments that are in fragHash.  */
{
struct psl *inList, *outList = NULL, *psl, *pslNext;
char *buf, **genoFiles, pslFile[512];
int genoCount, i;
struct agpFrag *frag;
int totalCount = 0, liftedCount = 0;

readAllWords(genoLst, &genoFiles, &genoCount, &buf);
for (i = 0; i<genoCount; ++i)
    {
    sprintf(pslFile, "%s", genoFiles[i]);
    inList = pslLoadAll(pslFile);
    for (psl = inList; psl != NULL; psl = pslNext)
        {
	pslNext = psl->next;
	++totalCount;
	frag = hashFindVal(fragHash, psl->tName);
	if (frag != NULL)
	    {
	    if (changePslCoordinates(psl, frag, destSize))
		{
		++liftedCount;
		slAddHead(&outList, psl);
		}
	    else
	        pslFree(&psl);
	    }
	else
	    pslFree(&psl);
	}
    }
printf("Writing %d psl records (of %d) to %s\n", liftedCount, totalCount, destFile);
slSort(&outList, pslCmpTarget);
pslWriteAll(outList, destFile, TRUE);
}

void liftFrags(char *destFile, char *goldFile, char *genoLst, char *sourceDir)
/* liftFrags - This program lifts annotations on fragments to FPC contig 
 * coordinates. */
{
struct hash *fragHash = newHash(16);
struct agpFrag *fragList = NULL;
int goldSize;

fragList = readGold(goldFile, fragHash, &goldSize);
printf("%d frags in %d bases in %s\n", slCount(fragList), goldSize, goldFile);
if (endsWith(destFile, ".out"))
    liftOut(destFile, fragHash, goldSize, genoLst, sourceDir);
else if (endsWith(destFile, ".psl"))
    liftPsl(destFile, fragHash, goldSize, genoLst, sourceDir);
else
    errAbort("Unrecognized suffix on %s", destFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5)
    usage();
liftFrags(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
