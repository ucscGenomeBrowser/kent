/* twoBitToFa - Convert all or part of twoBit file to fasta. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "sig.h"
#include "dnaseq.h"
#include "fa.h"
#include "obscure.h"

static char const rcsid[] = "$Id: twoBitToFa.c,v 1.1 2004/02/23 02:35:36 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twoBitToFa - Convert all or part of .2bit file to fasta\n"
  "usage:\n"
  "   twoBitToFa input.2bit output.fa\n"
  "options:\n"
  "   -seq=name - restrict this to just one sequence\n"
  "   -start=X  - start at given position in sequence (zero-based)\n"
  "   -end=X - end at given position in sequence (non-inclusive)\n"
  );
}

char *clSeq = NULL;	/* Command line sequence. */
int clStart = 0;	/* Start from command line. */
int clEnd = 0;		/* End from command line. */

static struct optionSpec options[] = {
   {"seq", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {NULL, 0},
};

struct twoBitIndex
/* An entry in twoBit index. */
    {
    struct twoBitIndex *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash */
    bits32 offset;		/* Offset in file. */
    };

struct twoBitHeader
/* Holds header and index info from .2bit file. */
    {
    struct twoBitHeader *next;
    char *fileName;	/* Name of this file, for error reporting. */
    FILE *f;		/* Open file. */
    boolean isSwapped;	/* Is byte-swapping needed. */
    bits32 version;	/* Version of .2bit file */
    bits32 seqCount;	/* Number of sequences. */
    bits32 reserved;	/* Reserved, always zero for now. */
    struct twoBitIndex *indexList;	/* List of sequence. */
    struct hash *hash;	/* Hash of sequences. */
    };

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


struct dnaSeq *twoBitReadSeqFrag(struct twoBitHeader *tbh, char *name,
	int fragStart, int fragEnd)
/* Read part of sequence from .2bit file.  To read full
 * sequence call with start=end=0. */
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
uglyf("outSize = %d\n", outSize);

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
seq->name = cloneString(name);
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

#ifdef SOON
/* Set relevant parts to N. */
for (i=0; i<nBlockCount; ++i)
    {
    memset(seq->dna + nStarts[i], 'n', nSizes[i]);
    }

/* Take care of case. */
toUpperN(seq->dna, seq->size);
for (i=0; i<maskBlockCount; ++i)
    {
    toLowerN(seq->dna + maskStarts[i], maskSizes[i]);
    }
#endif /* SOON */
return seq;
}


void outputOne(struct twoBitHeader *tbh, char *seqName, FILE *f, 
	int start, int end)
/* Output sequence. */
{
struct dnaSeq *seq = twoBitReadSeqFrag(tbh, seqName, start, end);
faWriteNext(f, seq->name, seq->dna, seq->size);
dnaSeqFree(&seq);
}

void twoBitToFa(char *inName, char *outName)
/* twoBitToFa - Convert all or part of twoBit file to fasta. */
{
FILE *inFile = mustOpen(inName, "rb");
FILE *outFile = mustOpen(outName, "w");
struct twoBitHeader *tbh = twoBitHeaderRead(inName, inFile);
struct twoBitIndex *index;

if (clSeq == NULL)
    {
    for (index = tbh->indexList; index != NULL; index = index->next)
        {
	outputOne(tbh, index->name, outFile, clStart, clEnd);
	}
    }
else
    {
    outputOne(tbh, clSeq, outFile, clStart, clEnd);
    }
twoBitHeaderFree(&tbh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clSeq = optionVal("seq", clSeq);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
dnaUtilOpen();
twoBitToFa(argv[1], argv[2]);
return 0;
}
