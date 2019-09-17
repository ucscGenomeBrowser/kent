/* Some stuff shared by gfClient and gfPcr. 
 * Copyright 2001-2004 Jim Kent. All rights reserved. */

#include "common.h"
#include "linefile.h"
#include "dnaseq.h"
#include "genoFind.h"
#include "gfInternal.h"
#include "errAbort.h"
#include "nib.h"
#include "twoBit.h"



static int extendRespect(int oldX, int newX)
/* Return newX modified slightly so as to be in same frame as oldX. */
{
int frame = oldX % 3;
newX = newX - (newX % 3) + frame;
return newX;
}

void gfiExpandRange(struct gfRange *range, int qSize, int tSize, 
	boolean respectFrame, boolean isRc, int expansion)
/* Expand range to cover an additional 500 bases on either side. */
{
int x;

x = range->qStart - expansion;
if (x < 0) x = 0;
range->qStart = x;

x = range->qEnd + expansion;
if (x > qSize) x = qSize;
range->qEnd = x;

x = range->tStart - expansion;
if (x < 0) x = 0;
if (respectFrame && !isRc) 
    {
    x = extendRespect(range->tStart, x);
    }
range->tStart = x;

x = range->tEnd + expansion;
if (x > tSize) x = tSize;
if (respectFrame && isRc)
    {
    x = extendRespect(range->tEnd, x);
    if (x > tSize)
        x -= 3;
    }
range->tEnd = x;
}

struct dnaSeq *gfiExpandAndLoadCached(struct gfRange *range, 
	struct hash *tFileCache, char *tSeqDir, int querySize, 
	int *retTotalSeqSize, boolean respectFrame, boolean isRc, int expansion)
/* Expand range to cover an additional expansion bases on either side.
 * Load up target sequence and return. (Done together because don't
 * know target size before loading.) */
{
struct dnaSeq *target = NULL;
char fileName[PATH_LEN+256];

safef(fileName, sizeof(fileName), "%s/%s", tSeqDir, range->tName);
if (nibIsFile(fileName))
    {
    struct nibInfo *nib = hashFindVal(tFileCache, fileName);
    if (nib == NULL)
        {
	nib = nibInfoNew(fileName);
	hashAdd(tFileCache, fileName, nib);
	}
    if (isRc)
	reverseIntRange(&range->tStart, &range->tEnd, nib->size);
    gfiExpandRange(range, querySize, nib->size, respectFrame, isRc, expansion);
    target = nibLdPart(fileName, nib->f, nib->size, 
    	range->tStart, range->tEnd - range->tStart);
    if (isRc)
	{
	reverseComplement(target->dna, target->size);
	reverseIntRange(&range->tStart, &range->tEnd, nib->size);
	}
    *retTotalSeqSize = nib->size;
    }
else
    {
    struct twoBitFile *tbf = NULL;
    // split a filename+chrom like "/gbdb/hg19/hg19.2bit:chr1"
    // (can also be URL like "http://hgdownload.soe.ucsc.edu/gbdb/hg19/hg19.2bit:chr1")
    // into filename and tSeqName
    char *tSeqName = strrchr(fileName, ':');
    int tSeqSize = 0;
    if (tSeqName == NULL)
        errAbort("No colon in .2bit response from gfServer");
    *tSeqName++ = 0;
    tbf = hashFindVal(tFileCache, fileName);
    if (tbf == NULL)
        {
	tbf = twoBitOpen(fileName);
	hashAdd(tFileCache, fileName, tbf);
	}
    tSeqSize = twoBitSeqSize(tbf, tSeqName);
    if (isRc)
	reverseIntRange(&range->tStart, &range->tEnd, tSeqSize);
    gfiExpandRange(range, querySize, tSeqSize, respectFrame, isRc, expansion);
    target = twoBitReadSeqFragLower(tbf, tSeqName, range->tStart, range->tEnd);
    if (isRc)
	{
	reverseComplement(target->dna, target->size);
	reverseIntRange(&range->tStart, &range->tEnd, tSeqSize);
	}
    *retTotalSeqSize = tSeqSize;
    }
return target;
}

void gfiGetSeqName(char *spec, char *name, char *file)
/* Extract sequence name and optionally file name from spec, 
 * which includes nib and 2bit files.  (The file may be NULL
 * if you don't care.) */
{
if (nibIsFile(spec))
    {
    splitPath(spec, NULL, name, NULL);
    if (file != NULL)
        strcpy(file, spec);
    }
else
    {
    char *s = strchr(spec, ':');
    if (s == NULL)
	errAbort("Expecting colon in %s", spec);
    strcpy(name, s+1);
    if (file != NULL)
        {
	int fileNameSize = s - spec;
	memcpy(file, spec, fileNameSize);
	file[fileNameSize] = 0;
	}
    }
}

