/* sampleToWig - Convert sample format to denser wig(gle) format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "rle.h"
#include "../../hg/inc/sample.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sampleToWig - Convert sample format to denser wig(gle) format\n"
  "usage:\n"
  "   sampleToWig in.sample out.wig\n"
  "options:\n"
  "   -invert (converts wig to sample)\n"
  );
}

char wigSignature[4] = {'W', 'I', 'G', 2 };

void wigWriteHead(FILE *f)
/* Write out signature at start of wig file. */
{
mustWrite(f, wigSignature, sizeof(wigSignature));
}

boolean wigReadHead(FILE *f)
/* Read wiggle header return FALSE if it's wrong. */
{
char sig[4];
if ( fread(sig, sizeof(sig), 1, f) != 1)
    return FALSE;
if (memcmp(sig, wigSignature, 4) == 0)
    return TRUE;
else
    return FALSE;
}

FILE *wigOpenVerify(char *fileName)
/* Open file, read and verify header. */
{
FILE *f = mustOpen(fileName, "rb");
if (!wigReadHead(f))
    errAbort("%s is not a wig format file", fileName);
return f;
}

void setBytes(UBYTE *pt, UBYTE val, int count)
/* Set a bunch of bytes to val. */
{
while (--count>=0)
    *pt++ = val;
}

/* I'm duplicating this code here rather than moving this
 * whole business to live under hg.  */

struct sample *sampleLoad(char **row)
/* Load a sample from row fetched with select * from sample
 * from database.  Dispose of this with sampleFree(). */
{
struct sample *ret;
int sizeOne,i;
char *s;

AllocVar(ret);
ret->sampleCount = sqlUnsigned(row[6]);
ret->chrom = cloneString(row[0]);
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd = sqlUnsigned(row[2]);
ret->name = cloneString(row[3]);
ret->score = sqlUnsigned(row[4]);
strcpy(ret->strand, row[5]);
sqlUnsignedDynamicArray(row[7], &ret->samplePosition, &sizeOne);
assert(sizeOne == ret->sampleCount);
sqlSignedDynamicArray(row[8], &ret->sampleHeight, &sizeOne);
assert(sizeOne == ret->sampleCount);
return ret;
}

void sampleFree(struct sample **pEl)
/* Free a single dynamically allocated sample such as created
 * with sampleLoad(). */
{
struct sample *el;

if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->name);
freeMem(el->samplePosition);
freeMem(el->sampleHeight);
freez(pEl);
}


void sampleToWig(char *inName, char *outName)
/* sampleToWig - Convert sample format to denser wig(gle) format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *row[9];
struct sample *sample;
int i, size, cmpSize;

wigWriteHead(f);
while (lineFileRow(lf, row))
    {
    UBYTE *semiPacked;
    signed char *cmpBuf;
    sample = sampleLoad(row);

    /* Write out first fields as tab-separated. */
    fprintf(f, "%s\t", sample->chrom);
    fprintf(f, "%u\t", sample->chromStart);
    fprintf(f, "%u\t", sample->chromEnd);
    fprintf(f, "%s\t", sample->name);
    fprintf(f, "%u\t", sample->score);
    fprintf(f, "%s\t", sample->strand);

    /* Make an array that has 255 (maximum values)
     * for missing data, and real data (clamped to
     * 255 or less) elsewhere. */
    size = sample->chromEnd - sample->chromStart;
    AllocArray(semiPacked, size);
    setBytes(semiPacked,255,size);
    for (i=0; i<sample->sampleCount; ++i)
        {
	int val = sample->sampleHeight[i];
	if (val > 255) val = 255;
	semiPacked[sample->samplePosition[i]] = val;
	}

    /* Run length compress this array. */
    AllocArray(cmpBuf, 2*size);
    cmpSize = rleCompress(semiPacked, size, cmpBuf);
    fprintf(f, "%u\n", cmpSize);
    mustWrite(f, cmpBuf, cmpSize);

    freez(&cmpBuf);
    freez(&semiPacked);
    sampleFree(&sample);
    }
carefulClose(&f);
}

void wigToSample(char *inFile, char *outFile)
/* Convert from wiggle back to sample. */
{
FILE *f = wigOpenVerify(inFile);
FILE *out = mustOpen(outFile, "w");
char lineBuf[256], *s, *row[8];
int i, wordCount, outCount, start, end, size, cmpSize;
UBYTE *semiPacked;
signed char *cmpBuf;

while ((s = fgets(lineBuf, sizeof(lineBuf), f)) != NULL)
    {
    wordCount = chopLine(s, row);
    if (wordCount != 7)
        errAbort("%s does not seem to be a wig file\n", inFile);
    for (i=0; i<6; ++i)
        fprintf(out, "%s\t", row[i]);
    start = atoi(row[1]);
    end = atoi(row[2]);
    size = end - start;
    if (size <= 0)
        errAbort("%s has odd size\n", inFile);
    cmpSize = atoi(row[6]);

    AllocArray(cmpBuf, cmpSize);
    mustRead(f, cmpBuf, cmpSize);
    AllocArray(semiPacked, size);
    rleUncompress(cmpBuf, cmpSize, semiPacked, size);

    outCount = 0;
    for (i=0; i<size; ++i)
        {
	if (semiPacked[i] != 255)
	    ++outCount;
	}
    fprintf(out, "%d\t", outCount);

    for (i=0; i<size; ++i)
        {
	if (semiPacked[i] != 255)
	    fprintf(out, "%d,", i);
	}
    fprintf(out, "\t");
    for (i=0; i<size; ++i)
        {
	if (semiPacked[i] != 255)
	    fprintf(out, "%d,", semiPacked[i]);
	}
    fprintf(out, "\n");
    }
carefulClose(&out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
if (optionExists("invert"))
    wigToSample(argv[1], argv[2]);
else
    sampleToWig(argv[1], argv[2]);
return 0;
}
