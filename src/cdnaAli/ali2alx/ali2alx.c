#include "common.h"
#include "sig.h"
#include "wormdna.h"
#include "cda.h"



struct cdaInfo
    {
    struct cdaInfo *next;
    char *name;
    int hStart, hEnd;
    UBYTE chromIx;
    UBYTE isEmbryonic;
    long fpos;
    };

struct cdaInfo *nextInfo(FILE *f)
/* Get next piece of indexing info out of an ali file. */
{
struct cdaBlock block;
struct cdaAli ali;
struct cdaInfo *info;
int hStart = 0x7fffffff;
int hEnd = -hStart;
int i;
UBYTE chromIx;

AllocVar(info);
info->fpos = ftell(f);

if ((ali.name = cdaLoadString(f)) == NULL)
    return NULL;
mustReadOne(f, info->isEmbryonic);
mustReadOne(f, ali.baseCount);
mustReadOne(f, ali.milliScore);
mustReadOne(f, chromIx);
ali.chromIx = chromIx;
mustReadOne(f, ali.strand);
mustReadOne(f, ali.direction);
mustReadOne(f, ali.blockCount);

for (i=0; i<ali.blockCount; ++i)
    {
    int bStart, bEnd;
    cdaReadBlock(f, &block);
    bStart = block.hStart;
    bEnd = bStart + block.nEnd - block.nStart;
    if (hStart > bStart)
        hStart = bStart;
    if (hEnd < bEnd)
        hEnd = bEnd;
    }

info->name = ali.name;
info->hStart = hStart;
info->hEnd = hEnd;
info->chromIx = ali.chromIx;
return info;
}

struct cdaInfo *readInfo(char *fileName)
/* Read indexing info out of an ali file. */
{
FILE *f = mustOpen(fileName, "rb");
bits32 sig;
struct cdaInfo *list = NULL, *el;
int i = 0;

mustReadOne(f, sig);
if (sig != aliSig)
    errAbort("Bad signature on %s", fileName);

while ((el = nextInfo(f)) != NULL)
    {
    slAddHead(&list, el);
    }
fclose(f);
return list;
}

int cmpInfo(const void *va, const void *vb)
/* Compare two by start then end. */
{
struct cdaInfo **pA = (struct cdaInfo **)va;
struct cdaInfo **pB = (struct cdaInfo **)vb;
struct cdaInfo *a = *pA, *b = *pB;
int diff;

if ((diff = a->hStart - b->hStart) != 0)
    return diff;
return a->hEnd - b->hEnd;
}


void splitInfo(struct cdaInfo *list, struct cdaInfo *buckets[], int bucketCount)
/* Split list into buckets by chromosome and sort them. */
{
struct cdaInfo *el, *next;
int i;
for (i=0; i<bucketCount; ++i)
    buckets[i] = NULL;
next = list;
while ((el = next) != NULL)
    {
    next = el->next;
    slAddHead(&buckets[el->chromIx], el);
    }
for (i=0; i<bucketCount; ++i)
    slSort(&buckets[i], cmpInfo);
}


void writeAlx(struct cdaInfo *infoList, char *fileName)
/* Write out one alx file. */
{
FILE *f = mustOpen(fileName, "wb");
struct cdaInfo *info;
bits32 sig = alxSig;
// bits32 count = slCount(infoList);

writeOne(f, sig);
for (info = infoList; info != NULL; info = info->next)
    {
    writeOne(f, info->hStart);
    writeOne(f, info->hEnd);
    writeOne(f, info->fpos);
    }
fclose(f);
}

int countEmbryonic(struct cdaInfo *list)
{
struct cdaInfo *el;
int count = 0;
for (el = list; el != NULL; el = el->next)
    if (el->isEmbryonic)
        ++count;
return count;
}

int main(int argc, char *argv[])
{
char *inName, *outDir;
struct cdaInfo *infoList;
struct cdaInfo *chromInfo[10];
char **chromNames;
int chromCount;
int i;

if (argc != 3)
    {
    errAbort("ali2alx - produces an index file for each chromosome into an ali file.\n"
             "Usage:\n"
             "     ali2alx in.ali alxDir");
    }
inName = argv[1];
outDir = argv[2];
printf("Reading %s\n", inName);
infoList = readInfo(inName);
printf("%d embryonic of %d total\n", countEmbryonic(infoList), slCount(infoList));

wormChromNames(&chromNames, &chromCount);
printf("Splitting\n");
splitInfo(infoList, chromInfo, chromCount);


for (i=0; i<chromCount; ++i)
    {
    char fileName[512];
    sprintf(fileName, "%s%s.alx", outDir, chromNames[i]);
    printf("Writing %s\n", fileName);
    writeAlx(chromInfo[i], fileName);
    }
return 0;
}
