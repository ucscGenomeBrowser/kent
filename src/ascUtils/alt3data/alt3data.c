/* alt3data - extracts alt3 splicing data from intron database. */
#include "common.h"
#include "wormdna.h"

struct feature
    {
    struct feature *next;
    char *chrom;
    int start, end;
    char strand;
    char *orfName;
    DNA *dna;
    int alt3size;
    int cdnaCount;
    int shortCdnaCount;
    };

int cmpFeatures(const void *va, const void *vb)
/* Compare two introns. */
{
struct feature **pA = (struct feature **)va;
struct feature **pB = (struct feature **)vb;
struct feature *a = *pA, *b = *pB;
int diff;

if ((diff = strcmp(a->chrom, b->chrom)) != 0)
    return diff;
if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

int cmpAlts(const void *va, const void *vb)
/* Compare two introns. */
{
struct feature **pA = (struct feature **)va;
struct feature **pB = (struct feature **)vb;
struct feature *a = *pA, *b = *pB;

return a->alt3size - b->alt3size;
}


boolean sameFeatures(struct feature *a, struct feature *b)
/* Returns true if two features cover the same area. */
{
if (a == b)
    return TRUE;
if (a == NULL || b == NULL)
    return FALSE;
return cmpFeatures(&a, &b) == 0;
}

struct feature *skipIrrelevant(struct feature *listSeg, struct feature *feat,
    int extraSpace)
/* Skip parts of listSeg that couldn't matter to feat. Assumes
 * listSeg is sorted on start. Returns first possibly relevant
 * item of listSeg. */
{
struct feature *seg = listSeg;
char *chrom = feat->chrom;
int start = feat->start - extraSpace;
int skipCount = 0;

/* skip past wrong chromosome. */
while (seg != NULL && strcmp(seg->chrom, chrom) < 0)
    {  
    seg = seg->next;
    ++skipCount;
    }
/* Skip past stuff that we've passed on this chromosome. */
while (seg != NULL && seg->chrom == chrom && seg->end < start)
    {
    seg = seg->next;
    ++skipCount;
    }
return seg;
}

struct feature *sortedSearchOverlap(struct feature *listSeg, struct feature *feature, int extra)
/* Assuming that listSeg is sorted on start, search through list
 * until find something that overlaps with feature, or have gone
 * past where feature could be. Returns overlapping feature or NULL. */
{
struct feature *seg = listSeg;
char *chrom = feature->chrom;
int start = feature->start;
int end = feature->end;

while (seg != NULL && seg->chrom == chrom && seg->start <= end + extra)
    {
    if (!(seg->start - extra >= end || seg->end + extra <= start))
        return seg;
    seg = seg->next;
    }
return NULL;
}

struct feature *readGff(char *gffName)
/* Read a GFF file with ORF-names on features into memory. */
{
char line[1024];
int lineCount;
char *words[128];
int wordCount;
struct feature *list = NULL, *el;
FILE *f = mustOpen(gffName, "r");

while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    if (line[0] == '#')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        break;
    if (wordCount < 9)
        errAbort("Need at least 9 fields in line %d of %s", lineCount, gffName);
    AllocVar(el);
    el->chrom = wormOfficialChromName(words[0]);
    el->start = atoi(words[3]);
    el->end = atoi(words[4]);
    el->cdnaCount = atoi(words[5]);
    el->strand = words[6][0];
    el->orfName = cloneString(words[8]);
    slAddHead(&list, el);
    }
fclose(f);
slReverse(&list);
return list;
}

void fetchDna(struct feature *el)
/* Fill in the DNA from list - reverse complementing it if on minus strand. */
{
int size;

if (el->dna == NULL)
    {
    size = el->end - el->start;
    el->dna = wormChromPart(el->chrom, el->start, size);
    if (el->strand == '-')
         reverseComplement(el->dna, size);
    }
}

boolean verifyAg(struct feature *feat)
/* Return true if feature's DNA ends in AG. */
{
int size = feat->end - feat->start;
DNA *dna;

fetchDna(feat);
dna = feat->dna;
return (dna[size-2] == 'a' && dna[size-1] == 'g');
}

struct feature *scanForAlt3()
/* Scan intron database for introns that are identical on 5' side but
 * differ on 3' side. */
{
char intronGffName[512];
struct feature *intronList, *intron;
struct feature *seg, *segList;
struct feature *altList = NULL;
char intronStrand;

sprintf(intronGffName, "%s%s", wormCdnaDir(), "introns.gff");
intronList = readGff(intronGffName);
segList = intronList;
for (intron = intronList; intron != NULL; intron = intron->next)
    {
    intronStrand = intron->strand;
    seg = segList = skipIrrelevant(segList, intron, 0);
    while ((seg = sortedSearchOverlap(seg, intron, 0)) != NULL)
        {
        if (intronStrand != '.' && seg->strand == intronStrand)
            {
            struct feature *bigger, *smaller;
            boolean gotAlt = FALSE;
            if (intron->strand == '+')
                {
                if (intron->start == seg->start && intron->end != seg->end)
                    {
                    gotAlt = TRUE;
                    if (intron->end > seg->end)
                        {
                        bigger = intron;
                        smaller = seg;
                        }
                    else
                        {
                        bigger = seg;
                        smaller = intron;
                        }                    
                    }
                }
            else
                {
                if (intron->end == seg->end && intron->start != seg->start)
                    {
                    gotAlt = TRUE;
                    if (intron->start < seg->start)
                        {
                        bigger = intron;
                        smaller = seg;
                        }
                    else
                        {
                        bigger = seg;
                        smaller = intron;
                        }
                    }
                }
            if (gotAlt)
                {
                struct feature *alt;
                int alt3size = (bigger->end - bigger->start) - (smaller->end - smaller->start);

                if (alt3size <= 18)
                    {
                    if (!sameFeatures(bigger, altList))
                        {
                        if (verifyAg(bigger) && verifyAg(smaller))
                            {                          
                            AllocVar(alt);
                            *alt = *bigger;
                            alt->alt3size = alt3size;
                            alt->shortCdnaCount = smaller->cdnaCount;
                            slAddHead(&altList, alt);
                            }
                        }
                    }
                }
            }
        seg = seg->next;
        }
    }
slReverse(&altList);
return altList;
}

void writeDna(struct feature *list, char *fileName)
/* Write out an FA file from list. */
{
int shortestSize = 0x7fffffff;
int dnaSize;
int dnaOff;
struct feature *el;
FILE *f;
int goodCount = 0;
DNA *dna;

for (el = list; el != NULL; el = el->next)
    {
    dnaSize = el->end - el->start;
    if (shortestSize > dnaSize)
        shortestSize = dnaSize;
    }
uglyf("Shortest size is %d\n", shortestSize);
//shortestSize -= 4;  /* Avoid including intron start. */

f = mustOpen(fileName, "w");
for (el = list; el != NULL; el = el->next)
    {
    dnaSize = el->end - el->start;
    dna = el->dna;
    ++goodCount;
    dnaOff = dnaSize - shortestSize;
    fprintf(f, ">%s %s:%d-%d %c alt 3' by %d\n%s\n", el->orfName, el->chrom, el->start, el->end, 
        el->strand, el->alt3size, el->dna + dnaOff);    
    }
uglyf("Wrote %d good looking alt-spliced sites\n", goodCount);
fclose(f);
}

int main(int argc, char *argv[])
/* Check command line arguments, make up list of 3' alt spliced things,
 * use it to make an FA file. */
{
char *outName;
struct feature *list = NULL;

if (argc != 2)
    {
    errAbort("alt3data - writes an fa file full of sequence data from alt-3 spliced genes\n"
             "usage:\n"
             "    alt3data outfile.fa");
    }
outName = argv[1];
list = scanForAlt3();
slSort(&list, cmpAlts);
uglyf("Found %d alt 3' splice sites\n", slCount(list));
writeDna(list, outName);
return 0;
}