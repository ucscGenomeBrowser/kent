/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "align.h"
#include "axt.h"
#include "bed.h"


struct align* alignNew(struct axt* axtList)
/* Construct a new align object. */
{
struct align* aln;
struct axt* tail;
AllocVar(aln);

aln->head = axtList;
slSort(&aln->head, axtCmpTarget);

aln->tName = cloneString(aln->head->tName);
aln->tStart = aln->head->tStart;
tail = aln->head;
while (tail->next != NULL)
    tail = tail->next;
aln->tEnd = tail->tEnd;
return aln;
}

void alignFree(struct align** aln)
/* Free a align object. */
{
axtFreeList(&(*aln)->head);
freeMem((*aln)->tName);
freeMem((*aln)->selectMap);
freeMem(*aln);
aln = NULL;
}

struct align* alignLoadAxtFile(char* fname)
/* Construct an align object from an AXT file */
{
struct axt *axtList = NULL;
struct axt *rec;
struct align* aln = NULL;
struct lineFile *lf = lineFileOpen(fname, TRUE);

/* Read records into a list, make sure targets are the same */
while ((rec = axtRead(lf)) != NULL)
    {
    if ((axtList != NULL)
        && differentString(axtList->tName, rec->tName))
        errAbort("all target names must be the same, expected %s, "
                 "got %s at %s:%d",
                 axtList->tName, rec->tName,
                 lf->fileName, lf->lineIx);
    slAddHead(&axtList, rec);
    }
lineFileClose(&lf);
if (axtList == NULL)
    errAbort("no alignments found in %s", fname);
aln = alignNew(axtList);
    
return aln;
}

static void setSelect(struct align* aln,
                      char* name,
                      unsigned start,
                      unsigned end)
/* set the specified ranges in the select map that overlap the alignment */
{
if (start < aln->tStart)
    start = aln->tStart;
if (end > aln->tEnd)
    end = aln->tEnd;
if (sameString(name, aln->tName) && (end > start))
    bitSetRange(aln->selectMap, (start - aln->tStart),
                (end - start));
}

void alignSelectWithBedFile(struct align* aln,
                            char *fname)
/* select positions to count based on a BED file */
{
struct lineFile *lf = lineFileOpen(fname, TRUE);
struct bed rec;
int numWords;
char* row[4];
ZeroVar(&rec);

if (aln->selectMap == NULL)
    aln->selectMap = bitAlloc(aln->tEnd - aln->tStart);
    
while ((numWords = lineFileChopNext(lf, row, 4)) > 0)
    {
    if (numWords < 4)
        errAbort("short BED line at got %s:%d",
                 lf->fileName, lf->lineIx);
    bedStaticLoad(row, &rec);
    setSelect(aln, rec.chrom, rec.chromStart, rec.chromEnd);
    }

lineFileClose(&lf);
}
