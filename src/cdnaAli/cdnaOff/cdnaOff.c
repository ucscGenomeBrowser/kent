/* cdnaOff - create files that give cdna offsets.  One such file is
 * made for each chromosome. Format of file is:
 *         signature - 
 *         # of entries
 *         size of name part of entry
 *         entry list
 *             start
 *             end
 *             name
 * The entry list is sorted by start first, then end.
 */
#include "common.h"
#include "cdnaAli.h"
#include "sig.h"

struct cdnaAli
    {
    struct cdnaAli *next;
    struct cdna *cdna;
    struct ali *ali;
    int start, end;
    };

int cmpCdnaAli(const void *va, const void *vb)
/* Compare two introns. */
{
struct cdnaAli **pA = (struct cdnaAli **)va;
struct cdnaAli **pB = (struct cdnaAli **)vb;
struct cdnaAli *a = *pA, *b = *pB;
int diff;

if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}


struct cdnaAli *sortedChromList(struct cdna *cdnaList, char *chrom)
/* Make sorted list of cdna associated with chromosome. */
{
struct cdna *cdna;
struct ali *ali;
struct cdnaAli *list = NULL;
struct cdnaAli *el;

for (cdna = cdnaList; cdna != NULL; cdna = cdna->next)
    {
    for (ali = cdna->aliList; ali != NULL; ali = ali->next)
        {
        if (ali->chrom == chrom)
            {
            allocV(el);
            el->cdna = cdna;
            el->ali = ali;
            blockEnds(ali->blockList, &el->start, &el->end);
            slAddHead(&list, el);
            }
        }
    }
slSort(&list, cmpCdnaAli);
return list;
}

int longestCdnaName(struct cdnaAli *ca)
{
int longest = 0;
int len;
for (;ca != NULL; ca = ca->next)
    {
    if ((len = strlen(ca->cdna->name)) > longest)
        longest = len;
    }
return longest;
}

void writeCdo(char *fileName, struct cdnaAli *ca)
{
bits32 sig = cdoSig;
FILE *f = mustOpen(fileName, "wb");
bits32 listSize = slCount(ca);
bits32 nameSize = longestCdnaName(ca);
char *nameBuf = alloc(nameSize+1);
char isEmbryo;

writeOne(f, sig);
writeOne(f, listSize);
writeOne(f, nameSize);
for (;ca != NULL; ca = ca->next)
    {
    writeOne(f, ca->start);
    writeOne(f, ca->end);
    isEmbryo = (UBYTE)(ca->cdna->isEmbryonic);
    writeOne(f, isEmbryo);
    zeroBytes(nameBuf, nameSize);
    strcpy(nameBuf, ca->cdna->name);
    mustWrite(f, nameBuf, nameSize);
    }
fclose(f);
printf("Wrote %s\n", fileName);
}

int main(int argc, char *argv[])
{
char *inName, *outDir, *suffix;
char outName[512];
struct cdna *cdna;
int i;
struct cdnaAli *ca;
char *chrom;

if (argc != 3)
    {
    errAbort("cdnaOff - creates sorted offset files that position cDNAs in chromosome.\n"
             "usage:\n"
             "    cdnaOff good.txt outputDir\\ ");
    }
inName = argv[1];
outDir = argv[2];
suffix = ".cdo";
cdnaAliInit();

cdna = loadCdna(inName);

for (i=0; i<chromCount; ++i)
    {
    chrom = chromNames[i];
    ca = sortedChromList(cdna, chrom);
    printf("%d alignments on chromosome %s\n", slCount(ca), chrom);
    sprintf(outName, "%s%s%s", outDir, chrom, suffix);
    writeCdo(outName, ca);
    }
return 0;
}