/* FilterDB - Look for dupes in the DB.  Write out new DB without the dupes.  Catch is
 * some of the dupes are ok - just get rid of dupes from my genBank screwup. */
#include "common.h"
#include "shares.h"
#include "dnaseq.h"
#include "fa.h"
#include "wormdna.h"
#include "hash.h"

struct cdnaInfo
    /* Accumulated info about one cDNA. */
    {
    struct cdnaInfo *next;
    struct cdnaInfo *crcHashNext;
    struct dnaSeq *seq;
    int ix;
    bits32 baseCrc;
    boolean isDupe;
    };

bits32 dnaCrc(DNA *dna, int size)
/* Return a number that is fairly unique for a particular
 * piece of DNA. */
{
bits32 shiftAcc = 0;
bits32 b;
while (--size >= 0)
    {
    b = *dna++;
    if (shiftAcc >= 0x80000000u)
        {
        shiftAcc <<= 1;
        shiftAcc |= 1;
        shiftAcc += b;
        }
    else
        {
        shiftAcc <<= 1;
        shiftAcc += b;
        }
    }
return shiftAcc;
}


void filterDupeCdna(struct cdnaInfo *ci, struct dnaSeq *cdnaSeq)
/* Flag duplicated cDNA that is named differently. */
{
#define crcHashSize (1<<14)
#define crcHashMask (crcHashSize-1)
static struct cdnaInfo *crcHash[crcHashSize];
int hashVal = (ci->baseCrc&crcHashMask);
struct cdnaInfo *hel = crcHash[hashVal];
struct dnaSeq *otherSeq;
char *ciName;

ciName = ci->seq->name;
while (hel != NULL)
    {
    if (hel->seq->size == ci->seq->size && hel->baseCrc == ci->baseCrc)
        {
        otherSeq = hel->seq;
        if (memcmp(cdnaSeq->dna, otherSeq->dna, cdnaSeq->size) == 0)
            {
            /* This is from where I accidentally included some stuff twice
             * in the cDNA database. */
            if ((intAbs(hel->ix-ci->ix) <= 1) || (hel->ix < 5533 && ci->ix > 5533 &&
                ((ciName[0] == 'C' && ciName[1] == 'E')  ||
                (ciName[0] == 'c' && ciName[1] == 'm'))))
                {
                hel->isDupe = TRUE;
                break;
                }
            else
                {
//                warn("%s Duplicate cDNA size %d (%d %s and %d %s)", 
//                    ciName, cdnaSeq->size, ci->ix, ciName, hel->ix, hel->seq->name);
                break;
                }
            }
        }
    hel = hel->crcHashNext;
    }
ci->crcHashNext = crcHash[hashVal];
crcHash[hashVal] = ci;
}

struct cdnaInfo *readAll()
{
struct wormCdnaIterator *si;
struct cdnaInfo *list = NULL, *el;
struct dnaSeq *seq;
int ix = 0;

if (!wormSearchAllCdna(&si))
    errAbort("Couldn't wormSearchAllCdna");
while ((seq = nextWormCdna(si)) != NULL)
    {
    AllocVar(el);
    el->ix = ix;
    el->seq = seq;
    el->baseCrc = dnaCrc(seq->dna, seq->size);
    slAddHead(&list, el);
    filterDupeCdna(el, seq);
    ++ix;
    if (ix%5000 == 0)
        {
        printf("reading #%d\n", ix);
        }
    }
slReverse(&list);
return list;
}

void writeFa(FILE *f, char *name, DNA *dna, int dnaSize)
{
int writeSize;
fprintf(f, ">%s\n", name);
while (dnaSize > 0)
    {
    writeSize = dnaSize;
    if (writeSize > 50) writeSize = 50;
    mustWrite(f, dna, writeSize);
    fputc('\n', f);
    dna += writeSize;
    dnaSize -= writeSize;
    }
}

int writeAll(struct cdnaInfo *ci, char *fileName)
{
FILE *f = mustOpen(fileName, "w");
struct cdnaInfo *c;
int writeCount = 0;
for (c=ci; c != NULL; c = c->next)
    {
    if (!c->isDupe)
        {
        if (++writeCount % 5000 == 0)
            printf("writing %d\n", writeCount);
        writeFa(f, c->seq->name, c->seq->dna, c->seq->size);
        }
    }
fclose(f);
return writeCount;
}

int main(int argc, char *argv[])
{
struct cdnaInfo *ci;
int writeCount;

if (argc != 2)
    {
    errAbort("filterDb - remove accidental duplication event of 7/10/99\n"
             "Usage: filterDb newDb");
    }
dnaUtilOpen();
ci = readAll();
printf("Read %d\n", slCount(ci));
//writeCount = writeAll(ci, argv[1]);
//printf("Wrote %d\n", writeCount);
return 0;
}