/* binGood - make a binary version (good.ali) of the good.txt alignments file. */
#include "common.h"
#include "cdnaAli.h"
#include "sig.h"
#include "wormdna.h"


void saveString(FILE *f, char *s)
{
int len = strlen(s);
UBYTE ulen = (UBYTE)len;
if (len > 255)
    errAbort("String %s too long", s);
writeOne(f, ulen);
mustWrite(f, s, len);
}

void saveBlock(FILE *f, struct block *b)
{
UBYTE uc;

if (b->nStart != b->nEnd)
    {
    writeOne(f, b->nStart);
    writeOne(f, b->nEnd);
    writeOne(f, b->hStart);
    uc = (UBYTE)(b->startGood < 256 ? b->startGood : 255);
    writeOne(f, uc);
    uc = (UBYTE)(b->endGood < 256 ? b->endGood : 255);
    writeOne(f, uc);
    uc = (UBYTE)roundingScale(255, b->midGood, b->hEnd - b->hStart);
    writeOne(f, uc);
    }
}

int countGoodBlocks(struct block *b)
/* Count non-zero length blocks */
{
int count = 0;
for (;b!=NULL; b=b->next)
    {
    if (b->nStart != b->nEnd)
        ++count;
    }
return count;
}

int getChromIx(char *chrom)
{
int i;
for (i=0; i<chromCount; ++i)
	if (sameWord(chromNames[i], chrom))
		return i;
return -1;
}

void saveAli(FILE *f, struct ali *ali)
{
short blockCount = countGoodBlocks(ali->blockList);
UBYTE chromIx = (UBYTE)getChromIx(ali->chrom);
short milliScore = (short)(ali->score*1000);
struct block *b;
UBYTE type = (ali->cdna->isEmbryonic ?  1 : 0);

saveString(f, ali->cdna->name);
writeOne(f, type);
writeOne(f, ali->cdna->size);
writeOne(f, milliScore);
writeOne(f, chromIx);
writeOne(f, ali->strand);
writeOne(f, ali->direction);
writeOne(f, blockCount);
for (b = ali->blockList; b != NULL; b = b->next)
    saveBlock(f, b);
}


void saveCdna(FILE *f, struct cdna *cdna)
{
struct ali *ali;
for (ali = cdna->aliList; ali != NULL; ali = ali->next)
    saveAli(f, ali);
}

int countAlis(struct cdna *cdnaList)
{
int count = 0;
struct cdna *cdna;
for (cdna = cdnaList; cdna != NULL; cdna = cdna->next)
    count += slCount(cdna->aliList);
return count;
}

void saveBin(struct cdna *cdnaList, char *outName)
{
FILE *f = mustOpen(outName, "wb");
bits32 sig = aliSig;
struct cdna *c;

writeOne(f, sig);
for (c=cdnaList; c!=NULL; c=c->next)
    saveCdna(f, c);
fclose(f);
}

int countEmbryonic(struct cdna *cdnaList)
{
struct cdna *c;
int count = 0;

for (c = cdnaList; c != NULL; c = c->next)
    {
    if (c->isEmbryonic)
        ++count;
    }
return count;
}

int main(int argc, char *argv[])
{
struct cdna *cdnaList;
char *inName, *outName;

if (argc != 3)
    {
    errAbort("binGood - convert text format alignment file to binary format\n"
             "usage:\n"
             "    binGood good.txt good.ali");
    }
inName = argv[1];
outName = argv[2];

cdnaAliInit();
cdnaList = loadCdna(inName);
printf("%d embryonic ones, %d total\n", countEmbryonic(cdnaList), slCount(cdnaList));
printf("Saving %s\n", outName);
saveBin(cdnaList, outName);
printf("All done!\n");
return 0;
}
