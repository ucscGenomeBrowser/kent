/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Nib - nibble (4 bit) representation of nucleotide sequences. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "sig.h"

void nibOpenVerify(char *fileName, FILE **retFile, int *retSize)
/* Open file and verify it's in good nibble format. */
{
bits32 size;
bits32 sig;
FILE *f = mustOpen(fileName, "rb");

dnaUtilOpen();
mustReadOne(f, sig);
mustReadOne(f, size);
if (sig != nibSig)
    {
    sig = byteSwap32(sig);
    size = byteSwap32(size);
    if (sig != nibSig)
	errAbort("%s is not a good .nib file.",  fileName);
    }
*retSize = size;
*retFile = f;
}

static struct dnaSeq *nibLd(char *fileName, FILE *f, int seqSize, int start, int size,
    char *faName)
/* Load part of an open .nib file. */
{
int end;
DNA *d;
int bVal;
struct dnaSeq *seq;
int bytePos, byteSize;

assert(start >= 0);
assert(size >= 0);
end = start+size;
if (end > seqSize)
    errAbort("nibLoadPart past end of file (%d %d)", end, seqSize);

AllocVar(seq);
seq->size = size;
seq->name = cloneString(faName);
seq->dna = d = needLargeMem(size+1);
bytePos = (start>>1);
fseek(f, bytePos + 2*sizeof(bits32), SEEK_SET);
if (start & 1)
    {
    bVal = getc(f);
    if (bVal < 0)
	{
	errAbort("Read error 1 in %s", fileName);
	}
    *d++ = valToNt[(bVal&0xf)];
    // *d++ = valToNt[(bVal>>4)];
    size -= 1;
    }
byteSize = (size>>1);
while (--byteSize >= 0)
    {
    bVal = getc(f);
    if (bVal < 0)
	errAbort("Read error 2 in %s", fileName);
    d[0] = valToNt[(bVal>>4)];
    d[1] = valToNt[(bVal&0xf)];
    d += 2;
    }
if (size&1)
    {
    bVal = getc(f);
    if (bVal < 0)
	errAbort("Read error 3 in %s", fileName);
    *d++ = valToNt[(bVal>>4)];
    }
*d = 0;
return seq;
}


struct dnaSeq *nibLdPart(char *fileName, FILE *f, int seqSize, int start, int size)
/* Load part of an open .nib file. */
{
char nameBuf[512];
sprintf(nameBuf, "%s:%d-%d", fileName, start, start+size);
return nibLd(fileName, f, seqSize, start, size, nameBuf);
}

struct dnaSeq *nibLoadPart(char *fileName, int start, int size)
/* Load part of an .nib file. */
{
struct dnaSeq *seq;
FILE *f;
int seqSize;
nibOpenVerify(fileName, &f, &seqSize);
seq = nibLdPart(fileName, f, seqSize, start, size);
fclose(f);
return seq;
}

struct dnaSeq *nibLoadAll(char *fileName)
/* Load part of an .nib file. */
{
struct dnaSeq *seq;
FILE *f;
int seqSize;
nibOpenVerify(fileName, &f, &seqSize);
seq = nibLd(fileName, f, seqSize, 0, seqSize, fileName);
fclose(f);
return seq;
}

void nibWrite(struct dnaSeq *seq, char *fileName)
/* Write out file in format of four bits per nucleotide. */
{
int i;
UBYTE byte;
DNA *dna = seq->dna;
int dVal;
bits32 size = seq->size;
int byteCount = (size>>1);
bits32 sig = nibSig;
int nCount = 0;
FILE *f = mustOpen(fileName, "w");

assert(sizeof(bits32) == 4);

writeOne(f, sig);
writeOne(f, seq->size);

printf("Writing %d bases in %d bytes\n", seq->size, ((seq->size+1)/2) + 8);
while (--byteCount >= 0)
    {
    dVal = ntVal5[dna[0]];
    byte = (dVal<<4);
    dVal = ntVal5[dna[1]];
    byte |= dVal;
    if (putc(byte, f) < 0)
	{
	perror("");
	errAbort("Couldn't write all of %s", fileName);
	}
    dna += 2;
    }
if (size & 1)
    {
    dVal = ntVal5[dna[0]];
    byte = (dVal<<4);
    putc(byte, f);
    }
}

boolean isNib(char *fileName)
/* Return TRUE if file is a nib file. */
{
return endsWith(fileName, ".nib") || endsWith(fileName, ".NIB");
}

