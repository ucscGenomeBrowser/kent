#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"

void writeTo4nt(struct dnaSeq *seq, FILE *file)
/* Write out file in format of two bits per nucleotide. */
{
int i;
bits32 word;
int wordLeft = 16;
DNA *dna = seq->dna;
int size = seq->size;
int sig = 0x12345678;
int dVal;
int nCount = 0;

assert(sizeof(bits32) == 4);
assert(sizeof(seq->size) == 4);

writeOne(file, sig);
writeOne(file, seq->size);

printf("Writing %d bases in %d bytes\n", seq->size, ((seq->size+15)/16) * 4);
for (i=0; i<size; ++i)
    {
    if ((dVal = ntVal[dna[i]]) < 0)
        {
        ++nCount;
        dVal = T_BASE_VAL;  /* N's etc. get almost silently converted to Ts */
        }
    word <<= 2;
    word += dVal;
    if (--wordLeft <= 0)
        {
        writeOne(file, word);
        word = 0;
        wordLeft = 16;
        }
    }
if (wordLeft > 0)
    {
    while (--wordLeft >= 0)
        word <<= 2;
    writeOne(file, word);
    }
if (nCount > 0)
    warn("%d N's changed to T's", nCount);
}

int main(int argc, char *argv[])
{
char *inName, *outName;
struct dnaSeq *seq;
FILE *outFile;

if (argc != 3)
    {
    errAbort("fato4nt - a program to convert .fa files to .4nt files\n"
             "Usage: fato4nt in.fa out.4nt");
    }
dnaUtilOpen();
inName = argv[1];
outName = argv[2];
outFile = mustOpen(outName, "wb");
seq = faReadDna(inName);
writeTo4nt(seq, outFile);
carefulClose(&outFile);
return 0;
}