/* indexgl - Create an index for a gl file. */
#include "common.h"
#include "sig.h"
#include "snofmake.h"


static boolean glNextRecord(FILE *f, void *data, char **rName, int *rNameLen)
/* Reads next record of gl file and returns the name from it. */
{
static char geneNameBuf[256];
UBYTE geneNameSize;
UBYTE chromIx, strand;
short pointCount;
int point;
int i;

if (!readOne(f, geneNameSize))
    return FALSE;
if (!(geneNameSize > 0 && geneNameSize < 21))
    errAbort("geneNameSize out of range [1:20] at: %d", geneNameSize);
mustRead(f, geneNameBuf, geneNameSize);
geneNameBuf[geneNameSize] = 0;
mustReadOne(f, chromIx);
mustReadOne(f, strand);
mustReadOne(f, pointCount);
for (i=0; i<pointCount; ++i)
    mustReadOne(f, point);
*rName = geneNameBuf;
*rNameLen = geneNameSize;
return TRUE;
}

int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in;
bits32 sig;

if (argc != 3)
    {
    errAbort("This program makes an index file for a .gl file\n"
             "usage: %s file.gl indexFile\n", argv[0]);
    }
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "rb");
mustReadOne(in, sig);
if (sig !=  glSig)
    {
    fprintf(stderr, "%s isn't a good gl file\n", inName);
    exit(-3);
    }
if (!snofMakeIndex(in, outName, glNextRecord, NULL))
    {
    exit(-4);
    }
return 0;
}




