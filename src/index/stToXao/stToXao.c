/* stToXao - Make an index file into st file for each chromosome .  */
#include "common.h"
#include "sig.h"
#include "wormdna.h"

struct xaoPos
/* Saves position in xao file and chromosome position associated with it. */
    {
    struct xaoPos *next;
    int start, end;
    long fpos;
    };

int cmpXaoPos(const void *va, const void *vb)
/* Compare two by start then end. */
{
struct xaoPos **pA = (struct xaoPos **)va;
struct xaoPos **pB = (struct xaoPos **)vb;
struct xaoPos *a = *pA, *b = *pB;
int diff;

if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

void writeXao(char *dir, char *chrom, struct xaoPos *xaoList)
/* Write out a .xao file for chromosome. */
{
char fileName[512];
FILE *f;
bits32 sig = xaoSig;
struct xaoPos *xao;

sprintf(fileName, "%s%s.xao", dir, chrom);
printf("Writing %s\n", fileName);
f = mustOpen(fileName, "wb");
writeOne(f, sig);
for (xao = xaoList; xao != NULL; xao = xao->next)
    {
    writeOne(f, xao->start);
    writeOne(f, xao->end);
    writeOne(f, xao->fpos);
    }
fclose(f);
}

static void skipThroughLf(FILE *f)
/* Just skip from current file position to next line feed. */
{
int c;

while ((c = fgetc(f)) != EOF)
    if (c == '\n')
        break;
}

int main(int argc, char *argv[])
{
char *inName, *outDir;
FILE *in;
static struct xaoPos *chromXao[64], *xao;
int i;
char line[512];
char *words[64];
int wordCount;
char *chrom;
int chromIx;
int chromCount;
char **chromNames;

if (argc != 3)
    {
    errAbort("stToXao - make indices into st file, one for each chromosome.\n"
             "usage:\n"
             "    stToXao infile.st outDir/");
    }
inName = argv[1];
outDir = argv[2];
wormChromNames(&chromNames, &chromCount);
assert(chromCount <= ArraySize(chromXao) );

in = mustOpen(inName, "rb");
printf("Scanning %s\n", inName);
for (;;)
    {
    AllocVar(xao);
    xao->fpos = ftell(in);
    if (fgets(line, sizeof(line), in) == NULL)
        break;
    wordCount = chopLine(line, words);
    if (wordCount < 9)
        errAbort("Short line in %s\n", inName);
    if (!wormParseChromRange(words[8], &chrom, &xao->start, &xao->end))
        errAbort("Bad chromosome range in %s, %s\n", inName, words[8]);
    chromIx = wormChromIx(chrom);
    slAddHead(&chromXao[chromIx], xao);

    /* Skip the query, target, and symbol long lines. */
    skipThroughLf(in);
    skipThroughLf(in);
    skipThroughLf(in);
    }

for (i=0; i<chromCount; ++i)
    {
    slSort(&chromXao[i], cmpXaoPos);
    writeXao(outDir, chromNames[i], chromXao[i]);
    }
return 0;
}
