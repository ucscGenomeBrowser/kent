/* nt4ToFas - convert one NT4 file to multiple FA files. */
#include "common.h"
#include "dnautil.h"
#include "nt4.h"
#include "fa.h"

void writeIn50s(FILE *f, DNA *dna, int dnaSize)
/* Write out FA file or die trying. */
{
int dnaLeft = dnaSize;
int lineSize;

while (dnaLeft > 0)
    {
    lineSize = dnaLeft;
    if (lineSize > 50)
        lineSize = 50;
    mustWrite(f, dna, lineSize);
    fputc('\n', f);
    dna += lineSize;
    dnaLeft -= lineSize;
    }
}

int main(int argc, char *argv[])
{
char outName[512];
int i,j;
DNA *dna;
int size = 9900;
int skip = 10000;
int ix = 8000000;
FILE *out;

for (i=0; i<12; ++i)
    {
    sprintf(outName, "/temp/iii_%d.fa", i+1);
    printf("Saving %s\n", outName);
    out = mustOpen(outName, "w");
    for (j=0; j<10+(i&3)*3; ++j)
        {
        if (i&1)
            {
            size = 1000 * (i+j+1);
            skip = size+100;
            }
        else
            {
            size = 1000 * (20+i-j);
            skip = size+100;
            }
        fprintf(out, ">%s %d-%d\n", outName, ix, ix+size);
        dna = nt4LoadPart("C:/biodata/celegans/chrom/x.nt4", ix, size);
        writeIn50s(out, dna, size);
        ix += skip;
        freez(&dna);
        }
    fclose(out);
    }
return 0;
}
