#include "common.h"
#include "hash.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "sig.h"
#include "localmem.h"
#include "linefile.h"
#include "obscure.h"
#include "twoBit.h"
#include <limits.h>

static char const rcsid[] = "$Id: twoBit.c,v 1.23 2008/08/12 07:04:35 mikep Exp $";

static int countBlocksOfN(char *s, int size)
/* Count number of blocks of N's (or n's) in s. */
{
int i;
boolean isN, lastIsN = FALSE;
char c;
int blockCount = 0;

for (i=0; i<size; ++i)
    {
    c = s[i];
    isN = (c == 'n' || c == 'N');
    if (isN && !lastIsN)
	++blockCount;
    lastIsN = isN;
    }
return blockCount;
}

static int countBlocksOfLower(char *s, int size)
/* Count number of blocks of lower case letters. */
{
int i;
boolean isLower, lastIsLower = FALSE;
int blockCount = 0;

for (i=0; i<size; ++i)
    {
    isLower = islower(s[i]);
    if (isLower && !lastIsLower)
	++blockCount;
    lastIsLower = isLower;
    }
return blockCount;
}

static void storeBlocksOfN(char *s, int size, bits32 *starts, bits32 *sizes)
/* Store starts and sizes of blocks of N's. */
{
int i;
boolean isN, lastIsN = FALSE;
int startN = 0;
char c;

for (i=0; i<size; ++i)
    {
    c = s[i];
    isN = (c == 'n' || c == 'N');
    if (isN)
        {
	if (!lastIsN)
	    startN = i;
	}
    else
        {
	if (lastIsN)
	    {
	    *starts++ = startN;
	    *sizes++ = i - startN;
	    }
	}
    lastIsN = isN;
    }
if (lastIsN)
    {
    *starts = startN;
    *sizes = i - startN;
    }
}

static void storeBlocksOfLower(char *s, int size, bits32 *starts, bits32 *sizes)
/* Store starts and sizes of blocks of lower case letters. */
{
int i;
boolean isLower, lastIsLower = FALSE;
int startLower = 0;

for (i=0; i<size; ++i)
    {
    isLower = islower(s[i]);
    if (isLower)
        {
	if (!lastIsLower)
	    startLower = i;
	}
    else
        {
	if (lastIsLower)
	    {
	    *starts++ = startLower;
	    *sizes++ = i - startLower;
	    }
	}
    lastIsLower = isLower;
    }
if (lastIsLower)
    {
    *starts = startLower;
    *sizes = i - startLower;
    }
}

static int packedSize(int unpackedSize)
/* Return size when packed, rounding up. */
{
return ((unpackedSize + 3) >> 2);
}

struct twoBit *twoBitFromDnaSeq(struct dnaSeq *seq, boolean doMask)
/* Convert dnaSeq representation in memory to twoBit representation.
 * If doMask is true interpret lower-case letters as masked. */
{
int ubyteSize = packedSize(seq->size);
UBYTE *pt;
struct twoBit *twoBit;
DNA last4[4];	/* Holds few bases. */
DNA *dna;
int i, end;

/* Allocate structure and fill in name. */
AllocVar(twoBit);
pt = AllocArray(twoBit->data, ubyteSize);
twoBit->name = cloneString(seq->name);
twoBit->size = seq->size;

/* Convert to 4-bases per byte representation. */
dna = seq->dna;
end = seq->size - 4;
for (i=0; i<end; i += 4)
    {
    *pt++ = packDna4(dna+i);
    }

/* Take care of conversion of last few bases. */
last4[0] = last4[1] = last4[2] = last4[3] = 'T';
memcpy(last4, dna+i, seq->size-i);
*pt = packDna4(last4);

/* Deal with blocks of N. */
twoBit->nBlockCount = countBlocksOfN(dna, seq->size);
if (twoBit->nBlockCount > 0)
    {
    AllocArray(twoBit->nStarts, twoBit->nBlockCount);
    AllocArray(twoBit->nSizes, twoBit->nBlockCount);
    storeBlocksOfN(dna, seq->size, twoBit->nStarts, twoBit->nSizes);
    }

/* Deal with masking */
if (doMask)
    {
    twoBit->maskBlockCount = countBlocksOfLower(dna, seq->size);
    if (twoBit->maskBlockCount > 0)
        {
	AllocArray(twoBit->maskStarts, twoBit->maskBlockCount);
	AllocArray(twoBit->maskSizes, twoBit->maskBlockCount);
	storeBlocksOfLower(dna, seq->size, 
		twoBit->maskStarts, twoBit->maskSizes);
	}
    }
return twoBit;
}


static int twoBitSizeInFile(struct twoBit *twoBit)
/* Figure out size structure will take in file. */
{
return packedSize(twoBit->size) 
	+ sizeof(twoBit->size)
	+ sizeof(twoBit->nBlockCount)
	+ sizeof(twoBit->nStarts[0]) * twoBit->nBlockCount
	+ sizeof(twoBit->nSizes[0]) * twoBit->nBlockCount
	+ sizeof(twoBit->maskBlockCount)
	+ sizeof(twoBit->maskStarts[0]) * twoBit->maskBlockCount
	+ sizeof(twoBit->maskSizes[0]) * twoBit->maskBlockCount
	+ sizeof(twoBit->reserved);
}

void twoBitWriteOne(struct twoBit *twoBit, FILE *f)
/* Write out one twoBit sequence to binary file. 
 * Note this does not include the name, which is
 * stored only in index. */
{
writeOne(f, twoBit->size);
writeOne(f, twoBit->nBlockCount);
if (twoBit->nBlockCount > 0)
    {
    fwrite(twoBit->nStarts, sizeof(twoBit->nStarts[0]), 
    	twoBit->nBlockCount, f);
    fwrite(twoBit->nSizes, sizeof(twoBit->nSizes[0]), 
    	twoBit->nBlockCount, f);
    }
writeOne(f, twoBit->maskBlockCount);
if (twoBit->maskBlockCount > 0)
    {
    fwrite(twoBit->maskStarts, sizeof(twoBit->maskStarts[0]), 
    	twoBit->maskBlockCount, f);
    fwrite(twoBit->maskSizes, sizeof(twoBit->maskSizes[0]), 
    	twoBit->maskBlockCount, f);
    }
writeOne(f, twoBit->reserved);
mustWrite(f, twoBit->data, packedSize(twoBit->size));
}

void twoBitWriteHeader(struct twoBit *twoBitList, FILE *f)
/* Write out header portion of twoBit file, including initial
 * index */
{
bits32 sig = twoBitSig;
bits32 version = 0;
bits32 seqCount = slCount(twoBitList);
bits32 reserved = 0;
bits32 offset = 0;
struct twoBit *twoBit;
long long counter = 0; /* check for 32 bit overflow */

/* Write out fixed parts of header. */
writeOne(f, sig);
writeOne(f, version);
writeOne(f, seqCount);
writeOne(f, reserved);

/* Figure out location of first byte past index.
 * Each index entry contains 4 bytes of offset information
 * and the name of the sequence, which is variable length. */
offset = sizeof(sig) + sizeof(version) + sizeof(seqCount) + sizeof(reserved);
for (twoBit = twoBitList; twoBit != NULL; twoBit = twoBit->next)
    {
    int nameLen = strlen(twoBit->name);
    if (nameLen > 255)
        errAbort("name %s too long", twoBit->name);
    offset += nameLen + 1 + sizeof(bits32);
    }

/* Write out index. */
for (twoBit = twoBitList; twoBit != NULL; twoBit = twoBit->next)
    {
    int size = twoBitSizeInFile(twoBit);
    writeString(f, twoBit->name);
    writeOne(f, offset);
    offset += size;
    counter += (long long)size;
    if (counter > UINT_MAX )
        errAbort("Error in faToTwoBit, index overflow at %s. The 2bit format "
                "does not support indexes larger than %dGb, \n"
                "please split up into smaller files.\n", 
                twoBit->name, UINT_MAX/1000000000);
    }
}

void twoBitClose(struct twoBitFile **pTbf)
/* Free up resources associated with twoBitFile. */
{
struct twoBitFile *tbf = *pTbf;
if (tbf != NULL)
    {
    freez(&tbf->fileName);
    carefulClose(&tbf->f);
    hashFree(&tbf->hash);
    /* The indexList is allocated out of the hash's memory pool. */
    freez(pTbf);
    }
}

struct twoBitFile *twoBitOpen(char *fileName)
/* Open file, read in header and index.  
 * Squawk and die if there is a problem. */
{
bits32 sig;
struct twoBitFile *tbf;
struct twoBitIndex *index;
boolean isSwapped = FALSE;
int i;
struct hash *hash;
FILE *f = mustOpen(fileName, "rb");

/* Allocate header verify signature, and read in
 * the constant-length bits. */
AllocVar(tbf);
mustReadOne(f, sig);
if (sig == twoBitSwapSig)
    isSwapped = tbf->isSwapped = TRUE;
else if (sig != twoBitSig)
    errAbort("%s doesn't have a valid twoBitSig", fileName);
tbf->fileName = cloneString(fileName);
tbf->f = f;
tbf->version = readBits32(f, isSwapped);
if (tbf->version != 0)
    {
    errAbort("Can only handle version 0 of this file. This is version %d",
    	(int)tbf->version);
    }
tbf->seqCount = readBits32(f, isSwapped);
tbf->reserved = readBits32(f, isSwapped);

/* Read in index. */
hash = tbf->hash = hashNew(digitsBaseTwo(tbf->seqCount));
for (i=0; i<tbf->seqCount; ++i)
    {
    char name[256];
    if (!fastReadString(f, name))
        errAbort("%s is truncated", fileName);
    lmAllocVar(hash->lm, index);
    index->offset = readBits32(f, isSwapped);
    hashAddSaveName(hash, name, index, &index->name);
    slAddHead(&tbf->indexList, index);
    }
slReverse(&tbf->indexList);
return tbf;
}


static int findGreatestLowerBound(int blockCount, bits32 *pos, 
	int val)
/* Find index of greatest element in posArray that is less 
 * than or equal to val using a binary search. */
{
int startIx=0, endIx=blockCount-1, midIx;
int posVal;

for (;;)
    {
    if (startIx == endIx)
        {
	posVal = pos[startIx];
	if (posVal <= val || startIx == 0)
	    return startIx;
	else
	    return startIx-1;
	}
    midIx = ((startIx + endIx)>>1);
    posVal = pos[midIx];
    if (posVal < val)
        startIx = midIx+1;
    else
        endIx = midIx;
    }
}

static void twoBitSeekTo(struct twoBitFile *tbf, char *name)
/* Seek to start of named record.  Abort if can't find it. */
{
struct twoBitIndex *index = hashFindVal(tbf->hash, name);
if (index == NULL)
     errAbort("%s is not in %s", name, tbf->fileName);
fseek(tbf->f, index->offset, SEEK_SET);
}

static void readBlockCoords(FILE *f, boolean isSwapped, bits32 *retBlockCount,
			    bits32 **retBlockStarts, bits32 **retBlockSizes)
/* Read in blockCount, starts and sizes from file. (Same structure used for
 * both blocks of N's and masked blocks.) */
{
bits32 blkCount = readBits32(f, isSwapped);
*retBlockCount = blkCount;
if (blkCount == 0)
    {
    *retBlockStarts = NULL;
    *retBlockSizes = NULL;
    }
else
    {
    bits32 *nStarts, *nSizes;
    AllocArray(nStarts, blkCount);
    AllocArray(nSizes, blkCount);
    fread(nStarts, sizeof(nStarts[0]), blkCount, f);
    fread(nSizes, sizeof(nSizes[0]), blkCount, f);
    if (isSwapped)
	{
	int i;
	for (i=0; i<blkCount; ++i)
	    {
	    nStarts[i] = byteSwap32(nStarts[i]);
	    nSizes[i] = byteSwap32(nSizes[i]);
	    }
	}
    *retBlockStarts = nStarts;
    *retBlockSizes = nSizes;
    }
}

struct twoBit *twoBitFromFile(char *fileName)
/* Get twoBit list of all sequences in twoBit file. */
{
struct twoBitFile *tbf = twoBitOpen(fileName);
struct twoBitIndex *index;
struct twoBit *twoBitList = NULL, *twoBit = NULL;
boolean isSwapped = tbf->isSwapped;
FILE *f = tbf->f;

for (index = tbf->indexList; index != NULL; index = index->next)
    {
    char *name = index->name;
    bits32 packByteCount;

    AllocVar(twoBit);
    twoBit->name = cloneString(name);

    /* Find offset in index and seek to it */
    twoBitSeekTo(tbf, name);

    /* Read in seqSize. */
    twoBit->size = readBits32(f, isSwapped);

    /* Read in blocks of N. */
    readBlockCoords(f, isSwapped, &(twoBit->nBlockCount),
		    &(twoBit->nStarts), &(twoBit->nSizes));

    /* Read in masked blocks. */
    readBlockCoords(f, isSwapped, &(twoBit->maskBlockCount),
		    &(twoBit->maskStarts), &(twoBit->maskSizes));

    /* Reserved word. */
    twoBit->reserved = readBits32(f, isSwapped);

    /* Read in data. */
    packByteCount = packedSize(twoBit->size);
    twoBit->data = needLargeMem(packByteCount);
    mustRead(f, twoBit->data, packByteCount);

    slAddHead(&twoBitList, twoBit);
    }

twoBitClose(&tbf);
slReverse(&twoBitList);
return twoBitList;
}

struct dnaSeq *twoBitReadSeqFragExt(struct twoBitFile *tbf, char *name,
	int fragStart, int fragEnd, boolean doMask, int *retFullSize)
/* Read part of sequence from .2bit file.  To read full
 * sequence call with start=end=0.  Sequence will be lower
 * case if doMask is false, mixed case (repeats in lower)
 * if doMask is true. */
{
struct dnaSeq *seq;
bits32 seqSize;
bits32 nBlockCount, maskBlockCount;
bits32 *nStarts = NULL, *nSizes = NULL;
bits32 *maskStarts = NULL, *maskSizes = NULL;
boolean isSwapped = tbf->isSwapped;
FILE *f = tbf->f;
int i;
int packByteCount, packedStart, packedEnd, remainder, midStart, midEnd;
int outSize;
UBYTE *packed, *packedAlloc;
DNA *dna;

/* Find offset in index and seek to it */
dnaUtilOpen();
twoBitSeekTo(tbf, name);

/* Read in seqSize. */
seqSize = readBits32(f, isSwapped);
if (fragEnd == 0)
    fragEnd = seqSize;
if (fragEnd > seqSize)
    errAbort("twoBitReadSeqFrag in %s end (%d) >= seqSize (%d)", name, fragEnd, seqSize);
outSize = fragEnd - fragStart;
if (outSize < 1)
    errAbort("twoBitReadSeqFrag in %s start (%d) >= end (%d)", name, fragStart, fragEnd);

/* Read in blocks of N. */
readBlockCoords(f, isSwapped, &nBlockCount, &nStarts, &nSizes);

/* Read in masked blocks. */
readBlockCoords(f, isSwapped, &maskBlockCount, &maskStarts, &maskSizes);

/* Skip over reserved word. */
readBits32(f, isSwapped);

/* Allocate dnaSeq, and fill in zero tag at end of sequence. */
AllocVar(seq);
if (outSize == seqSize)
    seq->name = cloneString(name);
else
    {
    char buf[256*2];
    safef(buf, sizeof(buf), "%s:%d-%d", name, fragStart, fragEnd);
    seq->name = cloneString(buf);
    }
seq->size = outSize;
dna = seq->dna = needLargeMem(outSize+1);
seq->dna[outSize] = 0;


/* Skip to bits we need and read them in. */
packedStart = (fragStart>>2);
packedEnd = ((fragEnd+3)>>2);
packByteCount = packedEnd - packedStart;
packed = packedAlloc = needLargeMem(packByteCount);
fseek(f, packedStart, SEEK_CUR);
mustRead(f, packed, packByteCount);

/* Handle case where everything is in one packed byte */
if (packByteCount == 1)
    {
    int pOff = (packedStart<<2);
    int pStart = fragStart - pOff;
    int pEnd = fragEnd - pOff;
    UBYTE partial = *packed;
    assert(pEnd <= 4);
    assert(pStart >= 0);
    for (i=pStart; i<pEnd; ++i)
	*dna++ = valToNt[(partial >> (6-i-i)) & 3];
    }
else
    {
    /* Handle partial first packed byte. */
    midStart = fragStart;
    remainder = (fragStart&3);
    if (remainder > 0)
	{
	UBYTE partial = *packed++;
	int partCount = 4 - remainder;
	for (i=partCount-1; i>=0; --i)
	    {
	    dna[i] = valToNt[partial&3];
	    partial >>= 2;
	    }
	midStart += partCount;
	dna += partCount;
	}

    /* Handle middle bytes. */
    remainder = fragEnd&3;
    midEnd = fragEnd - remainder;
    for (i=midStart; i<midEnd; i += 4)
        {
	UBYTE b = *packed++;
	dna[3] = valToNt[b&3];
	b >>= 2;
	dna[2] = valToNt[b&3];
	b >>= 2;
	dna[1] = valToNt[b&3];
	b >>= 2;
	dna[0] = valToNt[b&3];
	dna += 4;
	}

    if (remainder >0)
	{
	UBYTE part = *packed;
	part >>= (8-remainder-remainder);
	for (i=remainder-1; i>=0; --i)
	    {
	    dna[i] = valToNt[part&3];
	    part >>= 2;
	    }
	}
    }
freez(&packedAlloc);

if (nBlockCount > 0)
    {
    int startIx = findGreatestLowerBound(nBlockCount, nStarts, fragStart);
    for (i=startIx; i<nBlockCount; ++i)
        {
	int s = nStarts[i];
	int e = s + nSizes[i];
	if (s >= fragEnd)
	    break;
	if (s < fragStart)
	   s = fragStart;
	if (e > fragEnd)
	   e = fragEnd;
	if (s < e)
	    memset(seq->dna + s - fragStart, 'n', e - s);
	}
    }

if (doMask)
    {
    toUpperN(seq->dna, seq->size);
    if (maskBlockCount > 0)
	{
	int startIx = findGreatestLowerBound(maskBlockCount, maskStarts, 
		fragStart);
	for (i=startIx; i<maskBlockCount; ++i)
	    {
	    int s = maskStarts[i];
	    int e = s + maskSizes[i];
	    if (s >= fragEnd)
		break;
	    if (s < fragStart)
		s = fragStart;
	    if (e > fragEnd)
		e = fragEnd;
	    if (s < e)
		toLowerN(seq->dna + s - fragStart, e - s);
	    }
	}
    }
freez(&nStarts);
freez(&nSizes);
freez(&maskStarts);
freez(&maskSizes);
if (retFullSize != NULL)
    *retFullSize = seqSize;
return seq;
}

struct dnaSeq *twoBitReadSeqFrag(struct twoBitFile *tbf, char *name,
	int fragStart, int fragEnd)
/* Read part of sequence from .2bit file.  To read full
 * sequence call with start=end=0.  Note that sequence will
 * be mixed case, with repeats in lower case and rest in
 * upper case. */
{
return twoBitReadSeqFragExt(tbf, name, fragStart, fragEnd, TRUE, NULL);
}

struct dnaSeq *twoBitReadSeqFragLower(struct twoBitFile *tbf, char *name,
	int fragStart, int fragEnd)
/* Same as twoBitReadSeqFrag, but sequence is returned in lower case. */
{
return twoBitReadSeqFragExt(tbf, name, fragStart, fragEnd, FALSE, NULL);
}

int twoBitSeqSize(struct twoBitFile *tbf, char *name)
/* Return size of sequence in two bit file in bases. */
{
twoBitSeekTo(tbf, name);
return readBits32(tbf->f, tbf->isSwapped);
}

long long twoBitTotalSize(struct twoBitFile *tbf)
/* Return total size of all sequences in two bit file. */
{
struct twoBitIndex *index;
long long totalSize = 0;
for (index = tbf->indexList; index != NULL; index = index->next)
    {
    fseek(tbf->f, index->offset, SEEK_SET);
    totalSize += readBits32(tbf->f, tbf->isSwapped);
    }
return totalSize;
}

struct dnaSeq *twoBitLoadAll(char *spec)
/* Return list of all sequences matching spec, which is in
 * the form:
 *
 *    file/path/input.2bit[:seqSpec1][,seqSpec2,...]
 *
 * where seqSpec is either
 *     seqName
 *  or
 *     seqName:start-end */
{
struct twoBitSpec *tbs = twoBitSpecNew(spec);
struct twoBitFile *tbf = twoBitOpen(tbs->fileName);
struct dnaSeq *list = NULL;
if (tbs->seqs != NULL)
    {
    struct twoBitSeqSpec *tbss;
    for (tbss = tbs->seqs; tbss != NULL; tbss = tbss->next)
        slSafeAddHead(&list, twoBitReadSeqFrag(tbf, tbss->name,
                                               tbss->start, tbss->end));
    }
else
    {
    struct twoBitIndex *index;
    for (index = tbf->indexList; index != NULL; index = index->next)
	slSafeAddHead(&list, twoBitReadSeqFrag(tbf, index->name, 0, 0));
    }
slReverse(&list);
twoBitClose(&tbf);
twoBitSpecFree(&tbs);
return list;
}

struct slName *twoBitSeqNames(char *fileName)
/* Get list of all sequences in twoBit file. */
{
struct twoBitFile *tbf = twoBitOpen(fileName);
struct twoBitIndex *index;
struct slName *name, *list = NULL;
for (index = tbf->indexList; index != NULL; index = index->next)
    {
    name = slNameNew(index->name);
    slAddHead(&list, name);
    }
twoBitClose(&tbf);
slReverse(&list);
return list;
}

boolean twoBitIsFile(char *fileName)
/* Return TRUE if file is in .2bit format. */
{
return endsWith(fileName, ".2bit");
}

boolean twoBitParseRange(char *rangeSpec, char **retFile, 
	char **retSeq, int *retStart, int *retEnd)
/* Parse out something in format
 *    file/path/name:seqName:start-end
 * or
 *    file/path/name:seqName
 * or
 *    file/path/name:seqName1,seqName2,seqName3,...
 * This will destroy the input 'rangeSpec' in the process.  Returns FALSE if
 * it doesn't fit this format, setting retFile to rangeSpec, and retSet to
 * null.  If it is the shorter form then start and end will both be returned
 * as zero, which is ok by twoBitReadSeqFrag.  Any of the return arguments
 * maybe NULL.
 */
{
char *s, *e;
int n;

/* default returns */
if (retFile != NULL)
    *retFile = rangeSpec;
if (retSeq != NULL)
    *retSeq = NULL;
if (retStart != NULL)
    *retStart = 0;
if (retEnd != NULL)
    *retEnd = 0;

/* start with final name  */
s = strrchr(rangeSpec, '/');
if (s == NULL)
    s = rangeSpec;
else
    s++;

/* Grab seqName, zero terminate fileName. */
s = strchr(s, ':');
if (s == NULL)
    return FALSE;
*s++ = 0;
if (retSeq != NULL)
    *retSeq = s;

/* Grab start, zero terminate seqName. */
s = strchr(s, ':');
if (s == NULL)
    return TRUE;  /* no range spec */
*s++ = 0;
n = strtol(s, &e, 0);
if (*e != '-')
    return FALSE; /* not a valid range */
if (retStart != NULL)
    *retStart = n;
s = e+1;

/* Grab end. */
n = strtol(s, &e, 0);
if (*e != '\0')
    return FALSE; /* not a valid range */
if (retEnd != NULL)
    *retEnd = n;
return TRUE;
}

boolean twoBitIsRange(char *rangeSpec)
/* Return TRUE if it looks like a two bit range specifier. */
{
char *dupe = cloneString(rangeSpec);
char *file, *seq;
int start, end;
boolean isRange = twoBitParseRange(dupe, &file, &seq, &start, &end);
if (isRange)
    isRange = twoBitIsFile(file);
freeMem(dupe);
return isRange;
}

boolean twoBitIsFileOrRange(char *spec)
/* Return TRUE if it is a two bit file or subrange. */
{
return twoBitIsFile(spec) || twoBitIsRange(spec);
}

static struct twoBitSeqSpec *parseSeqSpec(char *seqSpecStr)
/* parse one sequence spec */
{
boolean isOk = TRUE;
char *s, *e;
struct twoBitSeqSpec *seq;
AllocVar(seq);
seq->name = cloneString(seqSpecStr);

/* Grab start */
s = strchr(seq->name, ':');
if (s == NULL)
    return seq;  /* no range spec */
*s++ = 0;
seq->start = strtol(s, &e, 0);
if (*e != '-')
    isOk = FALSE;
else
    {
    /* Grab end */
    s = e+1;
    seq->end = strtol(s, &e, 0);
    if (*e != '\0')
        isOk = FALSE;
    }
if (!isOk || (seq->end < seq->start))
    errAbort("invalid twoBit sequence specification: \"%s\"", seqSpecStr);
return seq;
}

boolean twoBitIsSpec(char *spec)
/* Return TRUE spec is a valid 2bit spec (see twoBitSpecNew) */
{
struct twoBitSpec *tbs = twoBitSpecNew(spec);
boolean isSpec = (tbs != NULL);
twoBitSpecFree(&tbs);
return isSpec;
}

struct twoBitSpec *twoBitSpecNew(char *specStr)
/* Parse a .2bit file and sequence spec into an object.
 * The spec is a string in the form:
 *
 *    file/path/input.2bit[:seqSpec1][,seqSpec2,...]
 *
 * where seqSpec is either
 *     seqName
 *  or
 *     seqName:start-end
 *
 * free result with twoBitSpecFree().
 */
{
char *s, *e;
int i, numSeqs;
char **seqSpecs;
struct twoBitSpec *spec;
AllocVar(spec);
spec->fileName = cloneString(specStr);

/* start with final file name  */
s = strrchr(spec->fileName, '/');
if (s == NULL)
    s = spec->fileName;
else
    s++;

/* find end of file name and zero-terminate */
e = strchr(s, ':');
if (e == NULL)
    s = NULL; /* just file name */
else
    {
    *e++ = '\0';
    s = e;
    }
if (!endsWith(spec->fileName, ".2bit"))
    {
    twoBitSpecFree(&spec);
    return NULL; /* not a 2bit file */
    }

if (s != NULL)
    {
    /* chop seqs at commas and parse */
    numSeqs = chopString(s, ",", NULL, 0);
    AllocArray(seqSpecs, numSeqs);
    chopString(s, ",", seqSpecs, numSeqs);
    for (i = 0; i< numSeqs; i++)
        slSafeAddHead(&spec->seqs, parseSeqSpec(seqSpecs[i]));
    slReverse(&spec->seqs);
    }
return spec;
}

struct twoBitSpec *twoBitSpecNewFile(char *twoBitFile, char *specFile)
/* parse a file containing a list of specifications for sequences in the
 * specified twoBit file. Specifications are one per line in forms:
 *     seqName
 *  or
 *     seqName:start-end
 */
{
struct lineFile *lf = lineFileOpen(specFile, TRUE);
char *line;
struct twoBitSpec *spec;
AllocVar(spec);
spec->fileName = cloneString(twoBitFile);
while (lineFileNextReal(lf, &line))
    slSafeAddHead(&spec->seqs, parseSeqSpec(trimSpaces(line)));
slReverse(&spec->seqs);
lineFileClose(&lf);
return spec;
}

void twoBitSpecFree(struct twoBitSpec **specPtr)
/* free a twoBitSpec object */
{
struct twoBitSpec *spec = *specPtr;
if (spec != NULL)
    {
    struct twoBitSeqSpec *seq;
    while ((seq = slPopHead(&spec->seqs)) != NULL)
        {
        freeMem(seq->name);
        freeMem(seq);
        }
    freeMem(spec->fileName);
    freeMem(spec);
    *specPtr = NULL;
    }
}

void twoBitOutNBeds(struct twoBitFile *tbf, char *seqName, FILE *outF)
/* output a series of bed3's that enumerate the number of N's in a sequence*/
{
int nBlockCount;

twoBitSeekTo(tbf, seqName);

readBits32(tbf->f, tbf->isSwapped);

/* Read in blocks of N. */
nBlockCount = readBits32(tbf->f, tbf->isSwapped);

if (nBlockCount > 0)
    {
    bits32 *nStarts = NULL, *nSizes = NULL;
    int i;

    AllocArray(nStarts, nBlockCount);
    AllocArray(nSizes, nBlockCount);
    fread(nStarts, sizeof(nStarts[0]), nBlockCount, tbf->f);
    fread(nSizes, sizeof(nSizes[0]), nBlockCount, tbf->f);
    if (tbf->isSwapped)
	{
	for (i=0; i<nBlockCount; ++i)
	    {
	    nStarts[i] = byteSwap32(nStarts[i]);
	    nSizes[i] = byteSwap32(nSizes[i]);
	    }
	}

    for (i=0; i<nBlockCount; ++i)
	{
	fprintf(outF, "%s\t%d\t%d\n", seqName, nStarts[i], nStarts[i] + nSizes[i]);
	}

    freez(&nStarts);
    freez(&nSizes);
    }
}

int twoBitSeqSizeNoNs(struct twoBitFile *tbf, char *seqName)
/* return the size of the sequence, not counting N's*/
{
int nBlockCount;
int size;

twoBitSeekTo(tbf, seqName);

size = readBits32(tbf->f, tbf->isSwapped);

/* Read in blocks of N. */
nBlockCount = readBits32(tbf->f, tbf->isSwapped);

if (nBlockCount > 0)
    {
    bits32 *nStarts = NULL, *nSizes = NULL;
    
    int i;

    AllocArray(nStarts, nBlockCount);
    AllocArray(nSizes, nBlockCount);
    fread(nStarts, sizeof(nStarts[0]), nBlockCount, tbf->f);
    fread(nSizes, sizeof(nSizes[0]), nBlockCount, tbf->f);
    if (tbf->isSwapped)
	{
	for (i=0; i<nBlockCount; ++i)
	    {
	    nStarts[i] = byteSwap32(nStarts[i]);
	    nSizes[i] = byteSwap32(nSizes[i]);
	    }
	}

    for (i=0; i<nBlockCount; ++i)
	{
	size -= nSizes[i];
	}

    freez(&nStarts);
    freez(&nSizes);
    }

return(size);
}
