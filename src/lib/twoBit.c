#include "common.h"
#include "hash.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "sig.h"
#include "localmem.h"
#include "obscure.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: twoBit.c,v 1.3 2004/02/24 22:04:14 kent Exp $";

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
char b;
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
    writeString(f, twoBit->name);
    writeOne(f, offset);
    offset += twoBitSizeInFile(twoBit);
    }
}

void twoBitHeaderFree(struct twoBitHeader **pTbh)
/* Free up resources associated with twoBitHeader. */
{
struct twoBitHeader *tbh = *pTbh;
if (tbh != NULL)
    {
    freez(&tbh->fileName);
    carefulClose(&tbh->f);
    hashFree(&tbh->hash);
    /* The indexList is allocated out of the hash's memory pool. */
    freez(pTbh);
    }
}

static bits32 readBits32(FILE *f, boolean isSwapped)
/* Read and optionally byte-swap 32 bit entity. */
{
bits32 val;
mustReadOne(f, val);
if (isSwapped) 
    val = byteSwap32(val);
return val;
}

struct twoBitHeader *twoBitHeaderRead(char *fileName, FILE *f)
/* Read in header and index from already opened file.  
 * Squawk and die if there is a problem. */
{
bits32 sig;
struct twoBitHeader *tbh;
struct twoBitIndex *index;
boolean isSwapped = FALSE;
int i;
struct hash *hash;

/* Allocate header verify signature, and read in
 * the constant-length bits. */
AllocVar(tbh);
mustReadOne(f, sig);
if (sig == twoBitSwapSig)
    isSwapped = tbh->isSwapped = TRUE;
else if (sig != twoBitSig)
    errAbort("%s doesn't have a valid twoBitSig", fileName);
tbh->fileName = cloneString(fileName);
tbh->f = f;
tbh->version = readBits32(f, isSwapped);
if (tbh->version != 0)
    {
    errAbort("Can only handle version 0 of this file. This is version %d",
    	(int)tbh->version);
    }
tbh->seqCount = readBits32(f, isSwapped);
tbh->reserved = readBits32(f, isSwapped);

/* Read in index. */
hash = tbh->hash = hashNew(digitsBaseTwo(tbh->seqCount));
for (i=0; i<tbh->seqCount; ++i)
    {
    char name[256];
    if (!fastReadString(f, name))
        errAbort("%s is truncated", fileName);
    lmAllocVar(hash->lm, index);
    index->offset = readBits32(f, isSwapped);
    hashAddSaveName(hash, name, index, &index->name);
    slAddHead(&tbh->indexList, index);
    }
slReverse(&tbh->indexList);
return tbh;
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


struct dnaSeq *twoBitReadSeqFrag(struct twoBitHeader *tbh, char *name,
	int fragStart, int fragEnd)
/* Read part of sequence from .2bit file.  To read full
 * sequence call with start=end=0.  Note that sequence will
 * be mixed case, with repeats in lower case and rest in
 * upper case. */
{
struct dnaSeq *seq;
bits32 seqSize;
bits32 nBlockCount, maskBlockCount;
bits32 *nStarts = NULL, *nSizes = NULL;
bits32 *maskStarts = NULL, *maskSizes = NULL;
boolean isSwapped = tbh->isSwapped;
FILE *f = tbh->f;
struct twoBitIndex *index;
int i;
int packByteCount, packedStart, packedEnd, remainder, midStart, midEnd;
int outSize;
UBYTE *packed, *packedAlloc;
DNA *dna;

/* Find offset in index and seek to it */
dnaUtilOpen();
index = hashFindVal(tbh->hash, name);
if (index == NULL)
     errAbort("%s is not in %s", name, tbh->fileName);
fseek(f, index->offset, SEEK_SET);

/* Read in seqSize. */
seqSize = readBits32(f, isSwapped);
if (fragEnd == 0)
    fragEnd = seqSize;
if (fragEnd > seqSize)
    errAbort("twoBitReadSeqFrag end (%d) >= seqSize (%d)", fragEnd, seqSize);
outSize = fragEnd - fragStart;
if (outSize < 1)
    errAbort("twoBitReadSeqFrag start (%d) >= end (%d)", fragStart, fragEnd);

/* Read in blocks of N. */
nBlockCount = readBits32(f, isSwapped);
if (nBlockCount > 0)
    {
    AllocArray(nStarts, nBlockCount);
    AllocArray(nSizes, nBlockCount);
    fread(nStarts, sizeof(nStarts[0]), nBlockCount, f);
    fread(nSizes, sizeof(nSizes[0]), nBlockCount, f);
    if (isSwapped)
        {
	for (i=0; i<nBlockCount; ++i)
	    {
	    nStarts[i] = byteSwap32(nStarts[i]);
	    nSizes[i] = byteSwap32(nSizes[i]);
	    }
	}
    }

/* Read in masked blocks. */
maskBlockCount = readBits32(f, isSwapped);
if (maskBlockCount > 0)
    {
    AllocArray(maskStarts, maskBlockCount);
    AllocArray(maskSizes, maskBlockCount);
    fread(maskStarts, sizeof(maskStarts[0]), maskBlockCount, f);
    fread(maskSizes, sizeof(maskSizes[0]), maskBlockCount, f);
    if (isSwapped)
        {
	for (i=0; i<maskBlockCount; ++i)
	    {
	    maskStarts[i] = byteSwap32(maskStarts[i]);
	    maskSizes[i] = byteSwap32(maskSizes[i]);
	    }
	}
    }

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

toUpperN(seq->dna, seq->size);
if (maskBlockCount > 0)
    {
    int startIx = findGreatestLowerBound(maskBlockCount, maskStarts, fragStart);
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

return seq;
}

struct dnaSeq *twoBitLoadAll(char *spec)
/* Return list of all sequences matching spec.  If
 * spec is a simple file name then this will be
 * all sequence in file. Otherwise it will be
 * the sequence in the file specified by spec,
 * which is in format
 *    file/path/name:seqName:start-end
 * or
 *    file/path/name:seqName */
{
struct dnaSeq *list = NULL, *seq;
struct twoBitHeader *tbh = NULL;
FILE *f = NULL;
if (twoBitIsRange(spec))
    {
    char *dupe = cloneString(spec);
    char *file, *seqName;
    int start, end;
    twoBitParseRange(dupe, &file, &seqName, &start, &end);
    f = mustOpen(file, "rb");
    tbh = twoBitHeaderRead(file, f);
    list = twoBitReadSeqFrag(tbh, seqName, start, end);
    freez(&dupe);
    }
else
    {
    struct twoBitIndex *index;
    f = mustOpen(spec, "rb");
    tbh = twoBitHeaderRead(spec, f);
    for (index = tbh->indexList; index != NULL; index = index->next)
	{
	seq = twoBitReadSeqFrag(tbh, index->name, 0, 0);
	slAddHead(&list, seq);
	}
    slReverse(&list);
    }
twoBitHeaderFree(&tbh);
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
 * This will destroy the input 'rangeSpec' in the process.
 * Returns FALSE if it doesn't fit this format. 
 * If it is the shorter form then start and end will both
 * be returned as zero, which is ok by twoBitReadSeqFrag. */
{
char *s, *e;

/* Save file name. */
*retFile = s = rangeSpec;

/* Grab seqName, zero terminate fileName. */
s = strchr(s, ':');
if (s == NULL)
    return FALSE;
*s++ = 0;
*retSeq = s;

/* Grab start, zero terminate seqName. */
s = strchr(s, ':');
if (s == NULL)
    {
    *retStart = *retEnd = 0;
    return TRUE;
    }
*s++ = 0;
if (!isdigit(s[0]))
    return FALSE;
*retStart = atoi(s);

/* Grab end. */
s = strchr(s, '-');
if (s == NULL)
    return FALSE;
s += 1;
if (!isdigit(s[0]))
    return FALSE;
*retEnd = atoi(s);
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



