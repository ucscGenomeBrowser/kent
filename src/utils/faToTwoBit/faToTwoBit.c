/* faToTwoBit - Convert DNA from fasta to 2bit format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "fa.h"
#include "sig.h"

static char const rcsid[] = "$Id: faToTwoBit.c,v 1.2 2004/02/23 06:08:33 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToTwoBit - Convert DNA from fasta to 2bit format\n"
  "usage:\n"
  "   faToTwoBit in.fa [in2.fa in3.fa ...] out.2bit\n"
  "options:\n"
  "   -noMask - Ignore lower-case masking in fa file.\n"
  );
}

boolean noMask = FALSE;

static struct optionSpec options[] = {
   {"noMask", OPTION_BOOLEAN},
   {NULL, 0},
};

struct twoBit
/* Two bit representation of DNA. */
    {
    struct twoBit *next;	/* Next sequence in list */
    char *name;			/* Name of sequence. */
    UBYTE *data;		/* DNA at two bits per base. */
    bits32 size;			/* Size of this sequence. */
    bits32 nBlockCount;		/* Count of blocks of Ns. */
    bits32 *nStarts;		/* Starts of blocks of Ns. */
    bits32 *nSizes;		/* Sizes of blocks of Ns. */
    bits32 maskBlockCount;		/* Count of masked blocks. */
    bits32 *maskStarts;		/* Starts of masked regions. */
    bits32 *maskSizes;		/* Sizes of masked regions. */
    bits32 reserved;		/* Reserved for future expansion. */
    };

int countBlocksOfN(char *s, int size)
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

int countBlocksOfLower(char *s, int size)
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

void storeBlocksOfN(char *s, int size, bits32 *starts, bits32 *sizes)
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

void storeBlocksOfLower(char *s, int size, bits32 *starts, bits32 *sizes)
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

int twoBitSizeInFile(struct twoBit *twoBit)
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

static void unknownToN(char *s, int size)
/* Convert non ACGT characters to N. */
{
char c;
int i;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (ntChars[c] == 0)
        {
	if (isupper(c))
	    s[i] = 'N';
	else
	    s[i] = 'n';
	}
    }
}

	    
void faToTwoBit(char *inFiles[], int inFileCount, char *outFile)
/* Convert inFiles in fasta format to outfile in 2 bit 
 * format. */
{
struct twoBit *twoBitList = NULL, *twoBit;
int i;
struct hash *uniqHash = newHash(18);
FILE *f;

for (i=0; i<inFileCount; ++i)
    {
    char *fileName = inFiles[i];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct dnaSeq seq;
    ZeroVar(&seq);
    while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
        {
	if (hashLookup(uniqHash, seq.name))
	    errAbort("Duplicate sequence name %s", seq.name);
	hashAdd(uniqHash, seq.name, NULL);
	if (noMask)
	    faToDna(seq.dna, seq.size);
	else
	    unknownToN(seq.dna, seq.size);
	twoBit = twoBitFromDnaSeq(&seq, !noMask);
	slAddHead(&twoBitList, twoBit);
	}
    }
slReverse(&twoBitList);
f = mustOpen(outFile, "wb");
twoBitWriteHeader(twoBitList, f);
for (twoBit = twoBitList; twoBit != NULL; twoBit = twoBit->next)
    {
    twoBitWriteOne(twoBit, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
noMask = optionExists("noMask");
dnaUtilOpen();
faToTwoBit(argv+1, argc-2, argv[argc-1]);
return 0;
}
