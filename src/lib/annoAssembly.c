/* annoAssembly -- basic metadata about an assembly for the annoGrator framework. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoAssembly.h"
#include "dnaseq.h"
#include "obscure.h"
#include "twoBit.h"

struct annoAssembly *annoAssemblyNew(char *name, char *twoBitPath)
/* Return an annoAssembly with open twoBitFile. */
{
struct annoAssembly *aa;
AllocVar(aa);
aa->name = cloneString(name);
aa->twoBitPath = cloneString(twoBitPath);
aa->tbf = twoBitOpen(aa->twoBitPath);
aa->curSeq = NULL;
return aa;
}

struct slName *annoAssemblySeqNames(struct annoAssembly *aa)
/* Return a list of sequence names in this assembly. */
{
struct slName *seqNames = twoBitSeqNames(aa->twoBitPath);
slSort(&seqNames, slNameCmp);
return seqNames;
}

uint annoAssemblySeqSize(struct annoAssembly *aa, char *seqName)
/* Return the number of bases in seq which must be in aa's twoBitFile. */
{
if (aa->seqSizes == NULL)
    aa->seqSizes = hashNew(digitsBaseTwo(aa->tbf->seqCount));
struct hashEl *hel = hashLookup(aa->seqSizes, seqName);
uint seqSize;
if (hel != NULL)
    seqSize = (uint)(hel->val - NULL);
else
    {
    seqSize = (uint)twoBitSeqSize(aa->tbf, seqName);
    char *pt = NULL;
    hashAdd(aa->seqSizes, seqName, pt + seqSize);
    }
return seqSize;
}

void annoAssemblyGetSeq(struct annoAssembly *aa, char *seqName, uint start, uint end,
			char *buf, size_t bufSize)
/* Copy sequence to buf; bufSize must be at least end-start+1 chars in length. */
{
if (aa->curSeq == NULL || differentString(aa->curSeq->name, seqName))
    {
    dnaSeqFree(&aa->curSeq);
    aa->curSeq = twoBitReadSeqFragLower(aa->tbf, seqName, 0, 0);
    }
uint chromSize = aa->curSeq->size;
if (end > chromSize || start > chromSize || start > end)
    errAbort("annoAssemblyGetSeq: bad coords [%u,%u) (sequence %s size %u)",
	     start, end, seqName, chromSize);
safencpy(buf, bufSize, aa->curSeq->dna+start, end-start);
}

void annoAssemblyClose(struct annoAssembly **pAa)
/* Close aa's twoBitFile and free mem. */
{
if (*pAa == NULL)
    return;
struct annoAssembly *aa = *pAa;
freeMem(aa->name);
freeMem(aa->twoBitPath);
twoBitClose(&(aa->tbf));
hashFree(&(aa->seqSizes));
freez(pAa);
}

