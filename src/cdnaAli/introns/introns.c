/* This program creates a list of introns in GFF format and
 * in a more elaborate text format. It also creates a list
 * of alternatively spliced genes. */

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "dnautil.h"
#include "nt4.h"
#include "wormdna.h"
#include "localmem.h"
#include "cda.h"
#include "cdnaAli.h"
#include "htmshell.h"


struct nt4Seq **chromNt4;

int cloneAliReverseStrand(struct ali *ali)
/* Returns TRUE (1)  if the alignment of the clone EST came from is on minus
 * strand, FALSE (0) otherwise. */
{
struct cdna *cdna = ali->cdna;
char *name = cdna->name;
int nameLen = strlen(name);
boolean isReverse = FALSE;

if (nameLen > 1 && name[nameLen-1] == '3' && name[nameLen-2] == '.')
    isReverse = TRUE;
if (ali->strand == '-')
    isReverse = !isReverse;
return (isReverse ? 1 : 0);
}

int consensusStrand(struct cdnaRef *refList)
/* Return most common strand in refList. */
{
struct cdnaRef *ref;
int counts[2];

counts[0] = counts[1] = 0;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    counts[cloneAliReverseStrand(ref->ali)] += 1;
    }
return (counts[0] >= counts[1] ? 0 : 1);
}

int blockListToCdaBlocks(struct block *blockList, struct cdaBlock *cbArray)
{
struct block *b;
struct cdaBlock *c;
int goodCount = 0;
for (b = blockList,c=cbArray; b!=NULL; b=b->next, c++)
    {
    if (b->nStart < b->nEnd)
        {
        c->nStart = b->nStart;
        c->nEnd = b->nEnd;
        c->hStart = b->hStart;
        c->hEnd = b->hEnd;
        c->startGood = b->startGood;
        c->endGood = b->endGood;
        c->midScore = 255;
        ++goodCount;
        }
    }
return goodCount;
}


struct cdaAli *cdnaAliToCdaAli(struct cdna *cdna, struct ali *ali)
{
struct cdaAli *c;
allocV(c);
c->name = cdna->name;
c->baseCount = cdna->size;
c->milliScore = (int)(ali->score*1000);
c->chromIx = wormChromIx(ali->chrom);
c->strand = ali->strand;
c->blocks = alloc(slCount(ali->blockList) * sizeof(struct cdaBlock));
c->blockCount = blockListToCdaBlocks(ali->blockList, c->blocks);
cdaFixChromStartEnd(c);
return c;
}

struct feature *makeExons(struct cdna *cdnaList)
{
struct cdna *cdna;
struct ali *ali;
struct cdaBlock *block;
struct feature *exon = NULL, *exonList = NULL;
struct cdnaRef *ref;
int exonCount = 0;
struct cdaAli *c;
int blockCount;
int i;

printf("Making exons\n");
for (cdna = cdnaList; cdna != NULL; cdna = cdna->next)
    {
    for (ali = cdna->aliList; ali != NULL; ali = ali->next)
        {
        c = cdnaAliToCdaAli(cdna, ali);
        cdaCoalesceBlocks(c);
        blockCount = c->blockCount;
        block = c->blocks;
        for (i=0, block = c->blocks; i<blockCount; ++i, ++block)
            {
            if (block->nEnd - block->nStart >= 16)
                {
                ref = allocA(struct cdnaRef);
                ref->ali = ali;
                exon = allocA(struct feature);
                exon->chrom = ali->chrom;
                exon->start = block->hStart;
                exon->end = block->hEnd;
                exon->gene = ali->gene;
                exon->cdnaCount = 1;
                exon->cdnaRefs = ref;
                exon->startGood = block->startGood;
                exon->endGood = block->endGood;
                slAddHead(&exonList, exon);
                ++exonCount;
                }
            }
        }
    }
slSort(&exonList, cmpFeatures);
printf("Made %d exons\n", exonCount);
collapseFeatures(exonList);
printf("Done making %d exons after removing dupes\n", slCount(exonList));
return exonList;
}

char intronStrand(DNA *start, int size, boolean *retPerfect)
/* Returns '+', '-' or '.' for forward, reverse, and who knows. */
{
DNA a,b,y,z;
int fCount = 0, rCount = 0;

a = start[0];
b = start[1];
y = start[size-2];
z = start[size-1];

if (a == 'g')
    ++fCount;
if (b == 't')
    ++fCount;
if (y == 'a')
    ++fCount;
if (z == 'g')
    ++fCount;

if (a == 'c')
    ++rCount;
if (b == 't')
    ++rCount;
if (y == 'a')
    ++rCount;
if (z == 'c')
    ++rCount;
*retPerfect = (fCount >= 4 || rCount >= 4);
if (fCount >= 3 || rCount >= 3)
    return (fCount > rCount) ? '+' : '-';
else
    return '.';
}

void writeIntron(FILE *f, struct feature *intron, DNA *dna, int dnaSize, char strand, int startExtra, int intronSize, int insideIntronSize)
{
int i;
struct cdnaRef *ref;
fprintf(f, "%d %s %d %d %c %s ",
    intron->cdnaCount, intron->chrom, intron->start, intron->end, strand,
    intron->gene);
for (i=0; i<startExtra; ++i)
    fputc(dna[i], f);
fputc(' ', f);
for (i=startExtra; i<= startExtra+insideIntronSize; i++)
    fputc(dna[i], f);
fputc(' ', f);
for (i=startExtra + intronSize - insideIntronSize; i<startExtra+intronSize; ++i)
    fputc(dna[i], f);
fputc(' ', f);
for (i=startExtra+intronSize; i<dnaSize; ++i)
    fputc(dna[i], f);
for (ref = intron->cdnaRefs; ref != NULL; ref = ref->next)
    fprintf(f, " %s", ref->ali->cdna->name);
fputc('\n', f);
}

void writeDnaText(FILE *f, char *dna, int dnaSize, int lineSize)
/* Write out DNA to a file. */
{
while (dnaSize > 0)
    {
    if (lineSize > dnaSize) lineSize = dnaSize;
    mustWrite(f, dna, lineSize);
    fputc('\n', f);
    dna += lineSize;
    dnaSize -= lineSize;
    }
}

void writeIntrons(struct feature *intronList, char *gffName, char *txtName, char *faName)
{
FILE *gff = mustOpen(gffName, "w");
FILE *txt = mustOpen(txtName, "w");
FILE *fa = mustOpen(faName, "w");
struct feature *intron;
int intronSize;
int intronCount = 0;
int idealExtra = 30;
int insideIntronSize = 40;
int startExtra, endExtra;
int dnaStart, dnaEnd, dnaSize;
struct nt4Seq *chrom;
DNA *dna;
char strand;
boolean sureAboutStrand;
int fCount=0, rCount=0, qCount=0;
static char strandIxToC[2] = {'+', '-'};
char cloneStrand;
int consMismatchCount = 0;
int nonconsMismatchCount = 0;

printf("Writing introns gff\n");
fprintf(gff, "##gff-version 1\n");
fprintf(gff, "##source-version cDnaIntrons 1.0\n");

for (intron = intronList; intron != NULL; intron = intron->next)
    {
    /* Get the dna for this intron, and a little extra. */
    chrom = chromNt4[ixInStrings(intron->chrom, chromNames, chromCount)];
    dnaStart = intron->start - idealExtra;
    if (dnaStart < 0)
        dnaStart = 0;
    intronSize = intron->end - intron->start;
    startExtra = intron->start - dnaStart;
    dnaEnd = intron->end + idealExtra;
    if (dnaEnd > chrom->baseCount)
        dnaEnd = chrom->baseCount;
    endExtra = dnaEnd - intron->end;
    dnaSize = dnaEnd - dnaStart;
    dna = nt4Unpack(chrom, dnaStart, dnaSize);
    /* Figure out what strand it's on by match to consensus sequence. */
    strand = intronStrand(dna + startExtra, intronSize, &sureAboutStrand);
    cloneStrand = strandIxToC[consensusStrand(intron->cdnaRefs)];
    if (strand == '.') strand = cloneStrand;
    if (cloneStrand != strand)
        {
        //warn("Mixed signals on %s intron strand pos %s:%d-%d", 
        //    (sureAboutStrand ? "consensus" : "non-consensus"),
        //    intron->chrom, intron->start, intron->end);
        if (sureAboutStrand)
            consMismatchCount += 1;
        else
            nonconsMismatchCount += 1;
        }
    if (strand == '.') ++qCount;
    else if (strand == '+') ++fCount;
    else if (strand == '-') ++rCount;

    /* Write line to gff file. */
    fprintf(gff, "%s\t%s\tintron\t%d\t%d\t%d\t%c\t.\t%s\n",
        intron->chrom, intron->cdnaRefs->ali->cdna->name, intron->start + 1, intron->end, 
        intron->cdnaCount, strand, intron->gene);

    /* Write line to txt file, reverse complementing if necessary. */
    if (strand == '-')
        {
        int temp;
        reverseComplement(dna, dnaSize);
        temp = startExtra;
        startExtra = endExtra;
        endExtra = temp;
        }
    writeIntron(txt, intron, dna, dnaSize, strand, startExtra, intronSize, insideIntronSize);

    /* Write to fa file. */
    fprintf(fa, ">%s_%d_%d%c C. elegans intron near %s\n", intron->chrom, intron->start+1, 
        intron->end, strand, intron->gene);
    writeDnaText(fa, dna + startExtra, intronSize, 50);
     
    freeMem(dna);
    ++intronCount;
    }
fclose(fa);
fclose(gff);
fclose(txt);
printf("Wrote %d introns: %d + %d - %d ?\n", intronCount,
    fCount, rCount, qCount);
printf("%d consensus mismatches, %d nonconsensus mismatches on strand\n",
    consMismatchCount, nonconsMismatchCount);
}

struct hiRange
    {
    struct hiRange *next;
    int start, end;
    };

struct hiRange *newHiRange(int start, int end)
{
struct hiRange *hi;
allocV(hi);
hi->start = start;
hi->end = end;
return hi;
}

boolean overlapFeatures(struct feature *a, struct feature *b)
{
if (a->start >= b->end || b->start >= a->end)
    return FALSE;
else
    return TRUE;
}

struct hiRange *hiFromOverlap(struct feature *a, struct feature *b)
/* Return hiRange from overlapping parts of feature a,b 
 * (Assumes that a and b do overlap!) */
{
int start, end;

assert(overlapFeatures(a,b));
start = (a->start < b->start ? b->start : a->start);
end = (a->end < b->end ? a->end : b->end);
return newHiRange(start, end);
}

void hiEnds(struct hiRange *hiList, int *retStart, int *retEnd)
/* Returns end points of list of hiRanges. */
{
struct hiRange *hi;
int start = 0x7fffffff;
int end = -start;

assert(hiList != NULL);
for (hi = hiList; hi != NULL; hi = hi->next)
    {
    if (hi->start < start)
        start = hi->start;
    if (hi->end > end)
        end = hi->end;
    }
*retStart = start;
*retEnd = end;
}

struct hiRange *consolidateHi(struct hiRange *hiList)
/* Make minimal list of hiRanges that would display the same. */
{
int start, end;
int count;
struct hiRange *hi;
struct hiRange *newList = NULL;
char c, lastC = FALSE;
int s;
int i;
static char *flagBuf;
static int flagBufSize = 0;

hiEnds(hiList, &start, &end);
count = end-start;
if (count > flagBufSize)
    {
    flagBufSize = count*2;
    flagBuf = alloc(flagBufSize);
    }

memset(flagBuf, FALSE, count);
for (hi = hiList; hi != NULL; hi = hi->next)
    memset(flagBuf+hi->start - start, TRUE, hi->end - hi->start);

for (i=0; i<count; ++i)
    {
    c = flagBuf[i];
    if (c != lastC)
        {
        if (c == 0)
            {
            hi = newHiRange(s+start,i+start);
            slAddHead(&newList, hi);
            }
        else
            {
            s = i;
            }
        }
    lastC = c;
    }
if (lastC)
    {
    hi = newHiRange(s+start,count+start);
    slAddHead(&newList, hi);
    }
slReverse(&newList);
return newList;
}

struct gene
    {
    struct gene *next;
    char *name;
    boolean skippedExon;
    boolean skippedIntron;
    boolean notGenomicContamination;
    struct hiRange *hi;
    int altIntronStart;
    int altIntronEnd;
    int intronExonOverlap;
    int intronIntronOverlap;
    struct cdnaRef *cdnaRefs;
    };

int scoreGene(struct gene *g)
{
int score = g->intronIntronOverlap + g->intronExonOverlap;
if (g->notGenomicContamination)
    score += 20;
return score;
}

int cmpScores(const void *va, const void *vb)
/* Compare two introns. */
{
struct gene **pA = (struct gene **)va;
struct gene **pB = (struct gene **)vb;
struct gene *a = *pA, *b = *pB;

return scoreGene(b) - scoreGene(a);
}


int cmpNames(const void *va, const void *vb)
/* Compare two introns. */
{
struct gene **pA = (struct gene **)va;
struct gene **pB = (struct gene **)vb;
struct gene *a = *pA, *b = *pB;

return strcmp(a->name, b->name);
}

char *boos(boolean boo, char *trues, char *falses)
/* Return string reflecting boolean value. */
{
return boo ? trues : falses;
}

void findRefRange(struct cdnaRef *refList, char **retChrom, int *retStart, int *retEnd)
/* Find the range covered by references. */
{
struct cdnaRef *ref = refList;
char *chrom = ref->ali->chrom;
int start, end;

blockEnds(ref->ali->blockList, &start, &end);
for (ref = ref->next; ref != NULL; ref = ref->next)
    {
    int s, e;
    blockEnds(ref->ali->blockList, &s, &e);
    if (start > s)
        start = s;
    if (end < e)
        end = e;
    }
*retChrom = chrom;
*retStart = start;
*retEnd = end;
}

int printGeneList(FILE *f, struct gene *geneList)
{
struct gene *gene;
int geneCount = 0;
int cse=0, csi=0, c3=0, c5=0;
struct cdnaRef *ref;
char *chrom;
int start, end;
struct hiRange *hiList, *hi;

for (gene = geneList; gene != NULL; gene = gene->next)
    {
    ++geneCount;
    findRefRange(gene->cdnaRefs, &chrom, &start, &end);
    fprintf(f, "<A HREF=\"../cgi-bin/tracks.exe?where=%s:%d-%d", chrom, start, end);
    if (gene->hi != NULL)
        {
        fputs("&hilite=", f);
        hiList = consolidateHi(gene->hi);
        for (hi = hiList; hi != NULL; hi = hi->next)
            {
            fprintf(f, "%d-%d", hi->start, hi->end);
            if (hi->next != NULL)
                fputc(',', f);
            }
        }
    fprintf(f, "&title=cDNA+Near+%s\">", gene->name);
    fprintf(f, "%-14s</A>  %s   %s   %s   %5d %5d %5d %5d", 
        gene->name, 
        boos(gene->skippedExon, "EX", "  "), 
        boos(gene->skippedIntron, "IN", "  "), 
        boos(gene->notGenomicContamination, "NG", "  "),
        gene->altIntronStart, gene->altIntronEnd, gene->intronExonOverlap,
        gene->intronIntronOverlap);
    for (ref = gene->cdnaRefs; ref != NULL; ref = ref->next)
        fprintf(f, " %s", ref->ali->cdna->name);
    fprintf(f, "\n");
    if (gene->skippedExon) ++cse;
    if (gene->skippedIntron) ++csi;
    if (gene->altIntronStart) ++c5;
    if (gene->altIntronEnd) ++c3;    
    }
printf("%d Alt spliced genes: %d with skipped introns, %d with skipped exons,\n", geneCount, cse, csi);
printf("%d with alt 5' ends %d with alt 3' ends\n", c5, c3);
return geneCount;
}

boolean hasIntrons(struct ali *ali)
{
struct block *block, *next;
int hGap, nGap;

for (block = ali->blockList;block != NULL; block = block->next)
    {
    if ((next = block->next) == NULL)
        return FALSE;
    nGap = next->nStart - block->nEnd;
    hGap = next->hStart - block->hEnd;
    if (hGap >= 16 && nGap >= -1 && nGap <= 1)
        return TRUE;
    }   
}


void writeAlts(struct feature *intronList, struct feature *exonList,
    char *altFileName, char *justGenesName)
/* Find alt-spliced genes by looking for overlapping introns. */
/* Needs work ~~~ */
{
struct feature *intron, *nextIntron;
int overlap;
int noiseFudge = 2;
int thisSize, nextSize;
FILE *out = mustOpen(altFileName, "w");
FILE *justGenes = mustOpen(justGenesName, "w");
struct hash *altHash = newHash(12);
int altGeneCount = 0;
struct gene *geneList = NULL, *gene;
int farEnd = -1;
struct feature *segList, *seg;
struct hashEl *hel;
struct hiRange *hi;


printf("Looking for alt genes with overlapping introns\n");
nextIntron = intronList;
segList = intronList;
for (intron = intronList; intron != NULL; intron = intron->next)
    {
    seg = segList = skipIrrelevant(segList, intron, -noiseFudge);
    while ((seg = sortedSearchOverlap(seg, intron, -noiseFudge)) != NULL)
        {
        /* Filter out introns not on same strand */
        if (consensusStrand(seg->cdnaRefs) == consensusStrand(intron->cdnaRefs))
            {
            /* Filter out introns that are almost identical. */
            overlap = intron->end - seg->start;
            thisSize = intron->end - intron->start;
            nextSize = seg->end - seg->start;
            if (
                !(overlap - 2*noiseFudge <= nextSize &&
                nextSize <= overlap + 2*noiseFudge)
                            ||
                (!(thisSize - noiseFudge <= nextSize &&
                nextSize <= thisSize + noiseFudge)))
                {
                fprintf(out, "%s intron %s (%d %d) overlaps intron %s (%d %d)\n",
                    intron->gene, 
                    intron->cdnaRefs->ali->cdna->name, intron->start, intron->end,
                    seg->cdnaRefs->ali->cdna->name, seg->start, seg->end);
                if ((hel = hashLookup(altHash, intron->gene)) == NULL)
                    {
                    gene = allocA(struct gene);
                    gene->name = intron->gene;
                    if (wormIsChromRange(gene->name))
                        uglyf("Really weird %s!\n", gene->name);
                    slAddHead(&geneList, gene);
                    hashAdd(altHash, intron->gene, gene);
                    }
                else
                    {
                    gene = hel->val;
                    }
                hi = hiFromOverlap(intron, seg);
                slAddHead(&gene->hi, hi);
                if (intron->start == seg->start)
                    {
                    int dif = intAbs(intron->end - seg->end);
                    if (dif > gene->altIntronEnd)
                        gene->altIntronEnd = dif;
                    }
                if (intron->end == seg->end)
                    {
                    int dif = intAbs(intron->start - seg->start);
                    if (dif > gene->altIntronStart)
                        gene->altIntronStart = dif;
                    }
                overlap = hi->end - hi->start;
                if (overlap > gene->intronIntronOverlap)
                    {
                    gene->intronIntronOverlap = overlap;
                    }
                gene->cdnaRefs = addUniqRefs(gene->cdnaRefs, seg->cdnaRefs);
                gene->cdnaRefs = addUniqRefs(gene->cdnaRefs, intron->cdnaRefs);
                gene->notGenomicContamination = TRUE;
                }
            }
        seg = seg->next;
        }
    }

htmStart(justGenes, "Alt Spliced Genes");
fputs("<TT><PRE>", justGenes);
printf("%d genes with overlapping introns\n", slCount(geneList));
printf("Looking for alt genes with overlapping exon/introns\n");
for (intron = intronList; intron != NULL; intron = intron->next)
    {
    struct feature *exon;
    exon = exonList = skipIrrelevant(exonList, intron, -noiseFudge);
    while ((exon = sortedSearchOverlap(exon, intron, -noiseFudge)) != NULL)
        {
        int overlap;
        int oStart, oEnd;
        boolean exonHasIntrons;
        int endGood;

        /* Filter out introns/exons  not on same strand */
        if (consensusStrand(exon->cdnaRefs) == consensusStrand(intron->cdnaRefs))
            {
            oStart = (exon->start > intron->start ? exon->start : intron->start);
            oEnd = (exon->end < intron->end ? exon->end : intron->end);
            endGood = (exon->start > intron->start ? exon->startGood : exon->endGood);
            overlap = oEnd - oStart;
        
            exonHasIntrons = hasIntrons(exon->cdnaRefs->ali);
            if ((overlap >= 10) || (endGood >= 5 && (overlap >= 5 || exonHasIntrons)))
                {
                fprintf(out, "%s exon %s (%d %d) overlaps intron %s (%d %d)\n",
                    intron->gene, 
                    exon->cdnaRefs->ali->cdna->name, exon->start, exon->end,
                    intron->cdnaRefs->ali->cdna->name, intron->start, intron->end);
                if ((hel = hashLookup(altHash, intron->gene)) == NULL)
                    {
                    gene = allocA(struct gene);
                    gene->name = intron->gene;
                    slAddHead(&geneList, gene);
                    hashAdd(altHash, intron->gene, gene);
                    }
                else
                    {
                    gene = hel->val;
                    }
                hi = hiFromOverlap(intron, exon);
                slAddHead(&gene->hi, hi);
                if (exon->start < intron->start && exon->end > intron->end)
                    {
                    gene->skippedIntron = TRUE;
                    }
                if (intron->start < exon->start && intron->end > exon->end) 
                    {
                    gene->skippedExon = TRUE;
                    }
                if (overlap > gene->intronExonOverlap)
                    gene->intronExonOverlap = overlap;
                if (exonHasIntrons)
                    gene->notGenomicContamination = TRUE;
                gene->cdnaRefs = addUniqRefs(gene->cdnaRefs, exon->cdnaRefs);
                gene->cdnaRefs = addUniqRefs(gene->cdnaRefs, intron->cdnaRefs);
                }
            }
        exon = exon->next;
        }
    }
slSort(&geneList, cmpNames);
altGeneCount = printGeneList(justGenes, geneList);
htmEnd(justGenes);
printf("Wrote %d alt genes\n", altGeneCount);
fclose(justGenes);
fclose(out);
}

int main(int argc, char *argv[])
{
char *inName, *intronTxtName, *intronGffName, *altIntronsName, *altGenesName,
    *intronFaName;
struct cdna *cdnaList;
long startTime, endTime;
struct feature *intronList;
struct feature *exonList;

if (argc != 7)
    {
    errAbort("Introns - finds the introns in a file and writes them to gff.\n"
             "usage:\n"
             "    introns good.txt introns.gff introns.txt altintrons.txt altgenes.txt introns.fa\n");
    }

inName = argv[1];
intronGffName = argv[2];
intronTxtName = argv[3];
altIntronsName = argv[4];
altGenesName = argv[5];
intronFaName = argv[6];

cdnaAliInit();

startTime = clock1000();
cdnaList = loadCdna(inName);
endTime = clock1000();
printf("Loaded %s in %f seconds\n", inName, (endTime - startTime)*0.001);

startTime = clock1000();
wormLoadNt4Genome(&chromNt4, &chromCount);
endTime = clock1000();
printf("Loaded genome in %f seconds\n", (endTime - startTime)*0.001);

intronList = makeIntrons(cdnaList);
writeIntrons(intronList, intronGffName, intronTxtName, intronFaName);

exonList = makeExons(cdnaList);
writeAlts(intronList, exonList, altIntronsName, altGenesName);

return 0;
}

