/* sorterna - This processes the output of editbase.c and displays it in
 * a more useful form. */

#include "common.h"
#include "dnaseq.h"
#include "wormdna.h"
#include "fuzzyFind.h"
#include "htmshell.h"

struct scoredString
    {
    struct scoredString *next;
    double score;
    char *string;
    };

struct ernaHit
    {
    struct ernaHit *next;
    char *chrom;
    int pos;
    int totalCdna;
    int commonErr;
    char from, to;
    double score;
    };

struct ernaClump
    {
    struct ernaClump *next;
    struct ernaHit *hits;
    char *chrom;
    int start, end;
    double score;
    };

struct ernaClump *findClump(struct ernaClump *clumpList, struct ernaHit *hit, int lumpSize)
/* Search clump list for a clump into which can add hit. */
{
struct ernaClump *clump;
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    if (hit->pos >= clump->start - lumpSize && hit->pos <= clump->end + lumpSize
        && sameString(hit->chrom, clump->chrom) )
        {
        return clump;
        }
    }
return NULL;
}
     
struct ernaClump *clumpHits(struct ernaHit *hitList)
/* Clump the hits together. */
{
struct ernaClump *clumpList = NULL, *clump;
int lumpSize = 25;
struct ernaHit *hit, *nextHit;

for (hit = hitList; hit != NULL; hit = nextHit)
    {
    nextHit = hit->next;
    if ((clump = findClump(clumpList, hit, lumpSize)) == NULL)
        {
        AllocVar(clump);
        clump->chrom = hit->chrom;
        clump->start = clump->end = hit->pos;
        slAddHead(&clumpList, clump);
        }
    else
        {
        if (hit->pos < clump->start)
            clump->start = hit->pos;
        else if (hit->pos > clump->end)
            clump->end = hit->pos;
        }
    clump->score += hit->score;
    slAddHead(&clump->hits, hit);
    }
return clumpList;
}

int cmpScore(const void *va, const void *vb)
/* Compare two by score */
{
const struct ernaClump *a = *((struct ernaClump **)va);
const struct ernaClump *b = *((struct ernaClump **)vb);
double dif = b->score - a->score;
if (dif < 0)
    return -1;
else if (dif > 0)
    return +1;
else
    return 0;
}

struct ernaHit *parseHit(char *line)
/* Convert hit from text format to binary. */
{
char *words[32];
int wordCount;
char *parts[3];
int partCount;
char *nucs[2];
int nucCount;
struct ernaHit *hit;

AllocVar(hit);
wordCount = chopLine(line, words);
hit->score = atof(words[0]);
partCount = chopString(words[1], ":", parts, ArraySize(parts));
hit->chrom = wormOfficialChromName(parts[0]);
hit->pos = atoi(parts[1])-1;
nucCount = chopString(words[2], "->", nucs, ArraySize(nucs));
hit->from = nucs[0][0];
hit->to = nucs[1][0];
hit->totalCdna = atoi(words[10]);
hit->commonErr = atoi(words[7]);
return hit;
}

#define lineSize 70

struct lineAli
    {
    struct lineAli *next;
    char *name;
    boolean isEmbryo;
    double score;
    char line[lineSize+1];
    };

int cmpLaScore(const void *va, const void *vb)
/* Compare two by score */
{
const struct lineAli *a = *((struct lineAli **)va);
const struct lineAli *b = *((struct lineAli **)vb);
double dif = b->score - a->score;
if (dif < 0)
    return -1;
else if (dif > 0)
    return +1;
else
    return 0;
}

int spaceCount(char *s)
/* Return true if string is all spaces. */
{
int count = 0;
char c;
while (c = *s++)
    if (c == ' ')
        ++count;
return count;
}

struct lineAli *makeLineAli(char *name, struct ffAli *aliList, DNA *hay, DNA *needle, int displayOffset)
/* Make up structure showing local alignment in detail. */
{
struct lineAli *la = AllocA(struct lineAli);
int dispSize = lineSize;
DNA *dispStart = hay + displayOffset;
DNA *dispEnd = dispStart + dispSize;
int displayEndIx = displayOffset + dispSize;
struct ffAli *ali;
int i;
int startIx, endIx;
int hToN;
DNA *h, *n;
DNA nuc;
int matchCount = 0;
int totalCount = 0;

la->name = name;
memset(la->line, ' ', dispSize);
for (ali = aliList; ali != NULL; ali = ali->right)
    {
    if (ali->hStart >= dispEnd || ali->hEnd <= dispStart)
        continue;
    startIx = ali->hStart - hay;
    if (startIx < displayOffset)
        startIx = displayOffset;
    endIx = ali->hEnd - hay;
    if (endIx > displayEndIx)
        endIx = displayEndIx;
    hToN = ali->nStart - ali->hStart;
    for (i=startIx; i<endIx; ++i)
        {
        h = hay+i;
        n = h + hToN;
        nuc = *n;
        if (*h != nuc)
            nuc = toupper(nuc);
        else
            ++matchCount;
        la->line[i-displayOffset] = nuc;
        ++totalCount;
        }
    }
la->score = lineSize - spaceCount(la->line);
return la;
}

void showClump(struct ernaClump *clump, FILE *f)
/* Show detailed alignment for one clump. */
{
int chromStart = clump->start - 1000;
int chromEnd = clump->end + 1000;
int chromSize;
DNA *chromDna;
struct wormFeature *cdnaNameList, *cdnaName;
struct lineAli *laList = NULL, *la;
struct ffAli *ali;
struct dnaSeq *cdna;
boolean rcCdna;
int clumpSize = clump->end - clump->start + 1;
int displaySize = lineSize;
int displayStart = (clump->start+clump->end)/2 - displaySize/2;
int displayEnd = displayStart + displaySize;
int displayDnaOffset;
DNA *displayDna;
struct ernaHit *hit;

/* Get genomic dna and list of all cDNAs in area around clump. */
wormClipRangeToChrom(clump->chrom, &chromStart, &chromEnd);
chromSize = chromEnd - chromStart;
chromDna = wormChromPart(clump->chrom, chromStart, chromSize);
cdnaNameList = wormCdnasInRange(clump->chrom, chromStart, chromEnd);

/* Figure out 60 bases to display alignment around clump. */
wormClipRangeToChrom(clump->chrom, &displayStart, &displayEnd);
displaySize = displayEnd - displayStart;
displayDnaOffset = displayStart - chromStart;
displayDna = chromDna + displayDnaOffset;

/* Make up detailed alignment on each cDNA */
for (cdnaName = cdnaNameList; cdnaName != NULL; cdnaName = cdnaName->next)
    {
    struct wormCdnaInfo info;
    if (!wormCdnaSeq(cdnaName->name, &cdna, &info))
        {
        warn("Couldn't find %s", cdnaName->name);
        continue;
        }
    if (!ffFindEitherStrandN(cdna->dna, cdna->size, chromDna, chromSize, ffCdna, &ali, &rcCdna))
        {
        warn("Couldn't align %s", cdnaName->name);
        continue;
        }
    if (rcCdna)
        reverseComplement(cdna->dna, cdna->size);
    la = makeLineAli(cdnaName->name, ali, chromDna, cdna->dna, displayDnaOffset);
    la->isEmbryo = info.isEmbryonic;
    slAddHead(&laList, la);    
    freeDnaSeq(&cdna);
    ffFreeAli(&ali);
    }

/* Display genomic with upper case at hot spots*/
displayDna[displaySize] = 0;
for (hit = clump->hits; hit != NULL; hit = hit->next)
    {
    int doff = hit->pos - chromStart;
    chromDna[doff] = toupper(chromDna[doff]);
    }
fprintf(f, "%s Genomic\n", displayDna);

/* Display aligned list by sorted score. */
slSort(&laList, cmpLaScore);
for (la = laList; la != NULL; la = la->next)
    {
    if (spaceCount(la->line) != lineSize)
        fprintf(f, "%s %s %s\n", la->line, la->name, (la->isEmbryo ? "emb" : "   "));
    }
/* Clean up. */
slFreeList(&cdnaNameList);
slFreeList(&laList);
freeMem(chromDna);
}

int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in, *out;
char line[256];
struct ernaHit *list = NULL, *el;
struct ernaClump *clumpList, *clump;
int clumpCount = 0;

if (argc != 3)
    {
    errAbort("sorterna - Process raw editRna file into a nice html.\n"
             "usage: sorterna editrna.raw editrna.html");
    }
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "r");
out = mustOpen(outName, "w");

/* Read in whole file. */
while (fgets(line, sizeof(line), in))
    {
    el = parseHit(line);
    if (el->totalCdna >= el->commonErr + 2)
        slAddHead(&list, el);
    }
fclose(in);

printf("Got %d raw elements\n", slCount(list));
clumpList = clumpHits(list);
printf("Merged into %d clumps\n", slCount(clumpList));

slSort(&clumpList, cmpScore);

htmStart(out, "RNA Editing Candidates");
fprintf(out, "<H2>RNA Editing Candidates</H2>\n");
fprintf(out, "<TT><PRE>");
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    ++clumpCount;
    printf("Analysing clump %d\n", clumpCount);
    fprintf(out, "<P><HR ALIGN=CENTER></P>");
    fprintf(out, "<A HREF=\"../cgi-bin/tracks.exe?where=%s:%d-%d&hilite=%d-%d\">%s:%d-%d</A> ", 
        clump->chrom, clump->start-1000, clump->end+1000,
        clump->start, clump->end+1,
        clump->chrom, clump->start, clump->end+1);
    fprintf(out, " score %f elements %d\n", clump->score, slCount(clump->hits));
    showClump(clump, out);
    }
htmEnd(out);
return 0;
}
