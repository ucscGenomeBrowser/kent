/* exonAli.c - Allign cDNAs based on assumption that alignments
 * will be clustered into exons. */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "shaRes.h"
#include "dnaseq.h"
#include "nt4.h"
#include "fa.h"
#include "wormdna.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "fuzzyFind.h"
#include "errabort.h"

long clock1000();

void usage()
/* Describe how to use program and abort. */
{
errAbort("This program aligns cDNA with genomic sequence. "
         "Usage:\n"
         "    exonAli named output cdnaName(s)\n"
         "    exonAli in output listFile\n"
         "    exonAli all output faFile ntDir\n"
         "    exonAli starting output faFile ntDir startingIx [count]\n"
         "    exonAli resume output faFile ntDir\n");
}


FILE *reportFile = NULL;
boolean isHtml = FALSE;

void reportAndExtra(char *format, va_list args, char *extra)
/* Print out to stdout and maybe a report file. Possibly
 * include extra string before report. */
{
if (reportFile != NULL)
    {
    if (extra != NULL)
        fprintf(reportFile, "%s", extra);
    vfprintf(reportFile, format, args);
    fflush(reportFile);
    }
if (extra != NULL)
    fprintf(stdout, "%s", extra);
vfprintf(stdout, format, args);
if (isHtml)
    fprintf(stdout, "<BR>\n");
}

void reportWarning(char *format, va_list args)
/* Print a warning message */
{
reportAndExtra(format, args, "###");
}

void vaReportf(char *format, va_list args)
/* Print out to stdout and maybe a report file. */
{
reportAndExtra(format, args, NULL);
}

void reportf(char *format, ...)
/* Print out to stdout and maybe a report file. */
{
va_list args;
va_start(args, format);
vaReportf(format, args);
va_end(args);
}



#define tileSize 16
/* # of bases we fit in 32 bits. */
#define tileSizeShift 4
/* How far to shift something to multiply it by tile size. */

struct crudeHit
/* A tileSize exact match between probe and target. */
    {
    int probeOffset;
    int targetOffset;
    boolean isLumped;
    };


struct crudeExon
/* A bunch of hits that hang together on a diagonal. */
    {
    int probeStart;
    int probeEnd;
    int targetStart;
    int targetEnd;
    int hitCount;
    boolean isLumped;
    };

struct crudeGene
/* A collection of diagonals that seem to hang together
 * like exons. */
    {
    struct nt4Seq *target;
    boolean isRc;
    int probeStart;
    int probeEnd;
    int targetStart;
    int targetEnd;
    int score;
    };

int lumpHitsIntoExons(struct crudeHit *hits, int hitCount, struct crudeExon *exons)
/* Lump together hits that are within 48 bp of each other and within 2 of
 * the same diagonal into "exons".  The exons array must be hitCount elements
 * in order to accomodate the worst case where no lumping occurs.  */
{
int i;
struct crudeExon *exon = exons;
int exonCount = 0;
for (i=0; i<hitCount; ++i)
    hits[i].isLumped = FALSE;

for (;;)
    {
    boolean startedExon = FALSE;
    int diagDiff;
    int tOff, pOff;
    int thisDiff;
    for (i=0; i<hitCount; ++i)
        {
        if (hits[i].isLumped == FALSE)
            {
            pOff = hits[i].probeOffset;
            tOff = hits[i].targetOffset;
            thisDiff = pOff - tOff;
            if (!startedExon)   /* First hit in exon. */
                {
                startedExon = TRUE;
                hits[i].isLumped = TRUE;
                exon->probeStart = pOff;
                exon->probeEnd = pOff + tileSize;
                exon->targetStart = tOff;
                exon->targetEnd = tOff + tileSize;
                exon->hitCount = 1;
                diagDiff = thisDiff;
                }
            else if (diagDiff - 2 <= thisDiff && thisDiff <= diagDiff + 2
                    && exon->probeEnd - 2 <= pOff && pOff <= exon->probeEnd + 48
                    && exon->targetEnd - 2 <= tOff && tOff <= exon->targetEnd + 48)
                {
                hits[i].isLumped = TRUE;
                exon->probeEnd = pOff + tileSize;
                exon->targetEnd = tOff + tileSize;
                exon->hitCount += 1;
                }
            }
        }
    if (!startedExon)   /* Must be all lumped together. */
        break;
    ++exonCount;
    ++exon;
    }

return exonCount;
}

int lumpExonsIntoGenes(struct crudeExon *exons, int exonCount, 
    struct crudeGene *genes, struct nt4Seq *target, boolean isRc)
/* Lump together crude exons into crude genes, and score them. */
{
int i;
struct crudeGene *gene = genes;
int geneCount = 0;

for (i=0; i<exonCount; ++i)
    exons[i].isLumped = FALSE;

for (;;)
    {
    boolean startedGene = FALSE;
    int lastDiff;
    int tOff, pOff;
    int thisDiff;
    int exonsInThis;

    for (i=0; i<exonCount; ++i)
        {
        if (!exons[i].isLumped)
            {
            tOff = exons[i].targetStart;
            pOff = exons[i].probeStart;
            thisDiff = tOff - pOff;
            if (!startedGene)
                {
                startedGene = TRUE;
                exons[i].isLumped = TRUE;
                lastDiff = thisDiff;
                exonsInThis = 1;
                gene->target = target;
                gene->isRc = isRc;
                gene->probeStart = exons[i].probeStart;
                gene->probeEnd = exons[i].probeEnd;
                gene->targetStart = exons[i].targetStart;
                gene->targetEnd = exons[i].targetEnd;
                gene->score = exons[i].hitCount * exons[i].hitCount;
                }
            else if (gene->targetEnd - 10 <= tOff && tOff <= gene->targetEnd + 25000
                     && gene->probeEnd - 10 <= pOff && pOff <= gene->probeEnd + 64
                     && lastDiff - 2 <= thisDiff)
                {
                exons[i].isLumped = TRUE;
                gene->targetEnd = exons[i].targetEnd;
                gene->probeEnd = exons[i].probeEnd;
                gene->score += exons[i].hitCount * exons[i].hitCount;
                exonsInThis += 1;
                lastDiff = thisDiff;
                }
            }
        }
    if (!startedGene)
        break;
    gene->score += exonsInThis;
    ++geneCount;
    ++gene;
    }
return geneCount;
}


boolean makeGoodTile(DNA *dna, bits32 *retTile)
/* Turn next 16 base pairs into a tile.  Return FALSE
 * if it wouldn't be a good tile. */
{
int i;
DNA repeater[2*tileSize-1];
bits32 tile;

/* Make sure that tile isn't part of a pattern. */ 
memcpy(repeater, dna+1, tileSize-1);
memcpy(repeater+tileSize-1, dna, tileSize);
if (memMatch(dna, tileSize, repeater, 2*tileSize-1) != NULL)
    return FALSE;

/* Make sure no N's in tile */
for (i=0; i<tileSize; ++i)
    {
    if (ntVal[dna[i]] < 0)
        return FALSE;
    }

/* Pack dna into tile, and make sure it's not on our list of
 * bad tiles. */
tile = packDna16(dna);
*retTile = tile;
return TRUE;
}

struct probeTile
/* A tile from the probe and the offset where it came from. */
    {
    struct probeTile *next;
    int offset;
    bits32 tile;
    };

#define tileHashBits 16
#define tileHashSize (1<<tileHashBits)
#define tileHashMask (tileHashSize-1)
#define tileHashFunc(tile) ((tile)&tileHashMask)

struct probeTile **makeTileHash()
{
struct probeTile **table;
int tableSize = tileHashSize * sizeof(table[0]);

table = needLargeMem(tableSize);
memset(table, 0, tableSize);
return table;
}


struct fastProber
/* Structure that has hash list of probeSeq tiles for fast searching */
    {
    struct probeTile **hash;
    struct probeTile *probes;
    };

struct fastProber *makeFastProber(struct dnaSeq *probeSeq)
{
DNA *probeDna = probeSeq->dna;
int maxProbeCount = probeSeq->size - tileSize + 1;
int probeCount = 0;
struct probeTile *probes;
struct probeTile *probe;
struct probeTile **hash;
int i;
struct fastProber *fp;

if (maxProbeCount <= 0)
    return NULL;

AllocVar(fp);
fp->hash = hash = makeTileHash();
fp->probes = probes = needMem(maxProbeCount * sizeof(probes[0]) );
for (i=0; i<maxProbeCount; ++i)
    {
    probe = &probes[probeCount];
    if (makeGoodTile(probeDna+i, &probe->tile))
        {
        int hashIx = tileHashFunc(probe->tile);
        probe->offset = i;
        probe->next = hash[hashIx];
        hash[hashIx] = probe;
        ++probeCount;
        }
    }
return fp;
}

void freeFastProber(struct fastProber **pFp)
{
struct fastProber *fp = *pFp;
if (fp != NULL)
    {
    freeMem(fp->probes);
    freeMem(fp->hash);
    freez(pFp);
    }
}

int makeIndividualHits(struct probeTile **hash, bits32 *bases, int baseOffset, int baseWordCount,
    struct crudeHit *hits, int maxHitCount, int hitCount)
/* Scan a short segment for individual hits. */
{
struct probeTile *probe;
int i;
for (i=0; i<baseWordCount; ++i)
    {
    if ((probe = hash[tileHashFunc(bases[i])]) != NULL)
        {
        do
            {
            if (bases[i] == probe->tile)
                {
                if (hitCount >= maxHitCount)
                    {
                    warn("Too many hits, only taking first %d", maxHitCount);
                    goto END_HIT_LOOP;
                    }
                hits[hitCount].probeOffset = probe->offset;
                hits[hitCount].targetOffset = ((i+baseOffset)<<tileSizeShift);
                ++hitCount;
                break;
                }
            probe = probe->next;
            }
        while (probe != NULL);
        }
    }
END_HIT_LOOP:
return hitCount;
}

int makeHits(struct fastProber *fp, struct nt4Seq *target, struct crudeHit *hits,
    int maxHitCount)
/* Scan entire target for hits to probe. */
{
bits32 *bases = target->bases;
struct probeTile **hash = fp->hash;
bits32 acc;
int hitCount = 0;
int i;
int baseWordCount = (target->baseCount>>tileSizeShift);
bits32 *endBases = bases + baseWordCount;
int chunkSize = 8;
int chunkCount = baseWordCount/chunkSize;
int baseOffset = 0;

for (i=0; i<chunkCount; ++i)
    {
    acc = ((bits32)(hash[tileHashFunc(bases[0])])
        | (bits32)(hash[tileHashFunc(bases[1])])
        | (bits32)(hash[tileHashFunc(bases[2])])
        | (bits32)(hash[tileHashFunc(bases[3])])
        | (bits32)(hash[tileHashFunc(bases[4])])
        | (bits32)(hash[tileHashFunc(bases[5])])
        | (bits32)(hash[tileHashFunc(bases[6])])
        | (bits32)(hash[tileHashFunc(bases[7])]));
    if (acc)
        {
        hitCount = makeIndividualHits(hash, bases, baseOffset, chunkSize, hits, maxHitCount, hitCount);
        if (hitCount >= maxHitCount)
            break;
        }
    bases += chunkSize;
    baseOffset += chunkSize;
    }
if (bases != endBases)
    hitCount = makeIndividualHits(hash, bases, baseOffset, endBases-bases, hits, maxHitCount, hitCount);
return hitCount;
}

int cmpHitsTargetFirst(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending probe offset. */
{
struct crudeHit *a = (struct crudeHit *)va;
struct crudeHit *b = (struct crudeHit *)vb;
long diff;
if ((diff = a->targetOffset - b->targetOffset) != 0)
    return diff;
return a->probeOffset - b->probeOffset;
}

int cmpExonsTargetFirst(const void *va, const void *vb)
/* Compare function to sort exons by ascending target
 * offset followed by ascending probe offset. */
{
struct crudeExon *a = (struct crudeExon *)va;
struct crudeExon *b = (struct crudeExon *)vb;
long diff;
if ((diff = a->targetStart - b->targetStart) != 0)
    return diff;
return a->probeStart - b->probeStart;
}


#define maxHitsAtOnce 20000
/* Maximum number of hits to allow on a single scan.
 * If we get more than this need to toss out a bad tile
 * or something... */


int findCrudeGenes(struct fastProber *fp, struct nt4Seq *target,
    struct crudeGene *genes, int maxGeneCount, boolean isRc)
/* Scan target with probe, and arrange hits into crude genes. */
{
struct crudeHit *hits;
struct crudeExon *exons;
int hitCount, exonCount, geneCount = 0;
int targetSize = target->baseCount;
int winMinHitIx = 0;

if (fp == NULL)
    return 0;
assert(maxGeneCount >= maxHitsAtOnce);
hits = needMem(maxHitsAtOnce*sizeof(*hits));
exons = needMem(maxHitsAtOnce*sizeof(*exons));
hitCount = makeHits(fp, target, hits, maxHitsAtOnce);
//hitCount = makeIndividualHits(fp->hash, target->bases, target->baseCount>>tileSizeShift,
//    hits, maxHitsAtOnce, 0);

if (hitCount > 0)
    {
    qsort(hits, hitCount, sizeof(hits[0]), cmpHitsTargetFirst);
    exonCount = lumpHitsIntoExons(hits, hitCount, exons);
    qsort(exons, exonCount, sizeof(exons[0]), cmpExonsTargetFirst);
    geneCount = lumpExonsIntoGenes(exons, exonCount, genes, target, isRc);
    }
freeMem(hits);
freeMem(exons);
return geneCount;
}

int cmpGenesByScore(const void *va, const void *vb)
/* cmp function to sort leaving highest scores first. */
{
struct crudeGene *a = (struct crudeGene *)va;
struct crudeGene *b = (struct crudeGene *)vb;
return b->score - a->score;
}

int countGenesBetter(struct crudeGene *genes, int geneCount, int cutoff)
/* Count number of genes (in sorted list) with scores better than cutoff */
{
int i;
for (i=0; i<geneCount; ++i)
    {
    if (genes[i].score <= cutoff)
        break;
    }
return i;
}

int filterPoorGenes(struct crudeGene *genes, int geneCount)
/* Sort gene list by score and remove bottom scorers. 
 * This is gauranteed to get rid of half of genes on list or 
 * more. */
{
int bestScore, worstScore;
int newGeneCount;
int halfGeneCount = geneCount/2;

qsort(genes, geneCount, sizeof(genes[0]), cmpGenesByScore);
bestScore = genes[0].score;
worstScore = genes[geneCount-1].score;

/* If scores are clustered with half or more at the top,
 * we have to be crude and just lop off bottom half of scores. */
if (bestScore == genes[halfGeneCount].score)
    {
    warn("Bunches of genes, all scoring the same. Program is "
         "throwing out half out of necessity.\n");
    return geneCount/2;
    }

/* Drop bottom score until have gotten rid of at least half. */
newGeneCount = geneCount;
while (newGeneCount > halfGeneCount)
    {
    worstScore = genes[newGeneCount-1].score;
    newGeneCount = countGenesBetter(genes, newGeneCount, worstScore);
    }
return newGeneCount;
}

void findAliEnds(struct ffAli *ali, DNA *needle, DNA *hay,
    long *retNeedleStart, long *retNeedleEnd,
    long *retHayStart, long *retHayEnd)
{
DNA *hStart, *hEnd, *nStart, *nEnd;
boolean first = TRUE;

while (ali->left) ali = ali->left;
for (;ali != NULL; ali = ali->right)
    {
    if (first)
        {
        hStart = ali->hStart;
        hEnd = ali->hEnd;
        nStart = ali->nStart;
        nEnd = ali->nEnd;
        first = FALSE;
        }
    else
        {
        if (ali->hStart < hStart)
            hStart = ali->hStart;
        if (ali->hEnd > hEnd)
            hEnd = ali->hEnd;
        if (ali->nStart < nStart)
            nStart = ali->nStart;
        if (ali->nEnd > nEnd)
            nEnd = ali->nEnd;
        }
    }
*retHayStart = hStart - hay;
*retNeedleStart = nStart - needle;
*retHayEnd = hEnd - hay;
*retNeedleEnd = nEnd - needle;
}



boolean findFineAlignment(struct dnaSeq *probe, struct crudeGene *crudeGene, 
        struct ffAli **retAli, int *retAliScore, 
        DNA **retUnpacked, int *retUnpackedSize,
        long *retNeedleStart, long *retNeedleEnd,
        long *retHayStart, long *retHayEnd)
/* Call fuzzy finder on the crude gene alignment to make a
 * complete alignment. */
{
struct nt4Seq *target;
int tarStart, tarEnd;
int unpackedSize;
DNA *unpacked;
struct ffAli *ali = NULL;
static int outside = 250;
int aliScore;
long hayStart, hayEnd, needleStart, needleEnd;

target = crudeGene->target;

tarStart = crudeGene->targetStart - outside;
if (tarStart < 0) tarStart = 0;
tarEnd = crudeGene->targetEnd + outside;
if (tarEnd > target->baseCount) tarEnd = target->baseCount;

unpackedSize = tarEnd - tarStart;
unpacked = nt4Unpack(target, tarStart, unpackedSize);

if (crudeGene->isRc)
    reverseComplement(probe->dna, probe->size);
ali = ffFind(probe->dna, probe->dna + probe->size, 
    unpacked, unpacked + unpackedSize, ffTight);
aliScore = ffScoreCdna(ali);
if (crudeGene->isRc)
    reverseComplement(probe->dna, probe->size);
if (ali != NULL)
    {
    findAliEnds(ali, probe->dna, unpacked, 
        &needleStart, &needleEnd, &hayStart, &hayEnd);    
    hayStart += tarStart;
    hayEnd += tarStart;
    }
//#define SHOW_ALI
#ifdef SHOW_ALI
if (hayStart == 4801255 && hayEnd == 4801707)
    {
    char hayName[50];
    sprintf(hayName, "%s:%d-%d %c", target->name, tarStart, tarEnd);
    ffShowAli(ali, "probe", probe->dna, 0, 
        hayName, unpacked, (tarStart),
        crudeGene->isRc ? '-' : '+');
    }
#endif /* SHOW_ALI */

*retAli = ali;
*retAliScore = aliScore;
*retUnpacked = unpacked;
*retUnpackedSize = unpackedSize;
*retNeedleStart = needleStart;
*retNeedleEnd = needleEnd;
*retHayStart = hayStart;
*retHayEnd = hayEnd;
return (ali != NULL);
}

void myBlast(struct dnaSeq *probe, char *probeName, struct nt4Seq *chrome[], int chromeCount)
{
int oneGeneCount;
int i;
int decentAlignments = 0;
int chromeIx;
int geneCount = 0;
int maxGeneCount = 2*maxHitsAtOnce;
long startTime, endTime;
int isRc;
struct nt4Seq *target;
struct crudeGene *crudeGenes = needMem(maxGeneCount*sizeof(*crudeGenes));
struct fastProber *fps[2];

/* Time how long it takes to allign things. */
startTime = clock1000();

fps[0] = makeFastProber(probe);
reverseComplement(probe->dna, probe->size);
fps[1] = makeFastProber(probe);
reverseComplement(probe->dna, probe->size);
for (chromeIx = 0; chromeIx < chromeCount; ++chromeIx)
    {
    target = chrome[chromeIx];
    for (isRc = 0; isRc <= 1; ++isRc)
        {
        /* Get crude idea of genes on one strand. */
        oneGeneCount = findCrudeGenes(fps[isRc], target, 
            crudeGenes+geneCount, maxGeneCount-geneCount, isRc);
            
        /* Increment gene count and if necessary compact list. */
        geneCount += oneGeneCount;
        if (maxGeneCount - geneCount < maxHitsAtOnce)
            {
            geneCount = filterPoorGenes(crudeGenes, geneCount);
            if (maxGeneCount - geneCount < maxHitsAtOnce)
                {
                warn("Too many genes. Moving on to the next.\n");
                break;
                }
            }
        }
    if (maxGeneCount - geneCount < maxHitsAtOnce)
        break;
    }

/* Sort genes by initial promise and get rid of some trash. */
    {
    int bestScore, worstScore;
    qsort(crudeGenes, geneCount, sizeof(crudeGenes[0]), cmpGenesByScore);
    bestScore = crudeGenes[0].score;
    worstScore = crudeGenes[geneCount-1].score;
    if (bestScore > worstScore)
        {
        int cutOff = bestScore/10;
        geneCount = countGenesBetter(crudeGenes, geneCount, cutOff);
        }
    }

/* Do fine alignments. */
for (i=0; i<geneCount; ++i)
    {
    struct ffAli *ali = NULL;
    int aliScore;
    DNA *unpackedDna = NULL;
    int unpackedSize;
    struct crudeGene *cg = crudeGenes+i;
    long hayStart, hayEnd;
    long needleStart, needleEnd;

    if (findFineAlignment(probe, cg, 
        &ali, &aliScore, &unpackedDna, &unpackedSize,
        &needleStart, &needleEnd, &hayStart, &hayEnd))
        {
        reportf("%s %d-%d hits %s %d-%d cs %d score %d strand %c\n", 
            probeName, needleStart, needleEnd,
            cg->target->name, hayStart, hayEnd,
            cg->score, aliScore, cg->isRc ? '-' : '+');
        ++decentAlignments;
        ffFreeAli(&ali);
        freez(&unpackedDna);
        }
    }
freeFastProber(&fps[0]);
freeFastProber(&fps[1]);
endTime = clock1000();
reportf("%d alignments of %s in %f seconds\n\n", decentAlignments, probeName, 
    (endTime-startTime)*0.001);
freeMem(crudeGenes);
}


char *lastCdnaReported(char *reportFileName)
/* Open report file and scan for last cDNA blasted. */
{
FILE *f = mustOpen(reportFileName, "r");
char linebuf[512];
char *words[16];
int wordCount;
char *lastName = NULL;
static char lastNameBuf[256];

for (;;)
    {
    if (fgets(linebuf, sizeof(linebuf), f) == NULL)
        break;
    wordCount = chopString(linebuf, whiteSpaceChopper, words, ArraySize(words));
    if (wordCount > 4 && (strcmp("alignments", words[1]) == 0))
        {
        strncpy(lastNameBuf, words[3], sizeof(lastNameBuf));
        lastName = lastNameBuf;
        }
    }
return lastName;
}

void reportBlasting(int ix, struct dnaSeq *cdna)
{
char *cdnaName = cdna->name;
reportf("%d Blasting %s %d bases\n", ix, cdnaName, cdna->size);
}

void  doInFile(char *inFileName, struct nt4Seq *nt4s[], int nt4Count)
{
FILE *inFile = mustOpen(inFileName, "r");
char lineBuf[256];
char *words[10];
char *cdnaName;
int wordCount;
struct dnaSeq *cdna;
int ix = 0;

while (fgets(lineBuf, sizeof(lineBuf), inFile) != NULL)
    {
    wordCount = chopString(lineBuf, whiteSpaceChopper, words, ArraySize(words));
    if (wordCount == 0) /* Skip blank lines. */
        continue;
    ++ix;
    cdnaName = words[0];
    if (!wormCdnaSeq(cdnaName, &cdna, NULL))
        errAbort("Couldn't find cdna %s\n", cdnaName);
    else
        {
        reportBlasting(ix, cdna);
        myBlast(cdna, cdnaName, nt4s, nt4Count);
        }
    freeDnaSeq(&cdna);
    }
}

FILE *estFile;

int loadNt4s(char *dir, struct nt4Seq ***retNt4s)
/* Load up genome stored 2 bits to a nucleotide. */
{
struct slName *nameList, *name;
struct nt4Seq **nt4s;
int count;
int i;
char fileName[512];
char chromName[256];
char fullChromName[256];
long start, end;

nameList = listDir(dir, "*.nt4");
count = slCount(nameList);
if (count < 1)
    errAbort("Couldn't find any .nt4 files in %s", dir);
nt4s = needMem(count*sizeof(nt4s[0]) );
start = clock1000();
for (name = nameList, i=0; name != NULL; name = name->next, i+=1)
    {
    char *end;

    sprintf(fileName, "%s/%s", dir, name->name);
    
    /* Cut off .nt4 suffix. */
    strcpy(chromName, name->name);
    end = strrchr(chromName, '.');
    assert(end != NULL);
    *end = 0;
    
    sprintf(fullChromName, "Chromosome %s", chromName);
    nt4s[i] = loadNt4(fileName, fullChromName);
    }
end = clock1000();
printf("%4.2f seconds loading %d .nt4 files\n", (end-start)*0.001, count);
*retNt4s = nt4s;
return count;
}

void startEsts(char *estFileName)
/* Start searching ESTs */
{
estFile = mustOpen(estFileName, "rb");
}

struct dnaSeq *nextEst()
/* Get next EST */
{
return faReadOneDnaSeq(estFile, NULL, TRUE);
}

int main(int argc, char *argv[])
/* Set things up to batch process all cdnas in data base.
 * Print results to console and to a report file.
 * Allow user to resume interrupted batch process. */
{
char *command;
struct dnaSeq *cdna = NULL;
struct nt4Seq **nt4s;
int nt4Count;
long baseCount = 0;
int i;
char *reportFileName;

if (argc < 3)
    usage();

dnaUtilOpen();

pushWarnHandler(reportWarning);



command = argv[1];
reportFileName = argv[2];

/* Simple "named" command doesn't get appended to report file. */
if (!differentWord(command, "named"))
    {
    int i;
    nt4Count = loadNt4s(wormChromDir(), &nt4s);
    for (i=3; i<argc; ++i)
        {
        char *cdnaName = argv[i];
        if (!wormCdnaSeq(cdnaName, &cdna, NULL))
            warn("Couldn't find cdna %s\n", cdnaName);
        else
            {
            reportBlasting(i-1, cdna);
            myBlast(cdna, cdnaName, nt4s, nt4Count);
            }
        freeDnaSeq(&cdna);
        }
    }
else if (!differentWord(command, "in"))
    {
    nt4Count = loadNt4s(wormChromDir(), &nt4s);
    reportFile = mustOpen(reportFileName, "a");
    doInFile(argv[3], nt4s, nt4Count);
    }
/* All others do.  Most use an iterator. */
else
    {
    struct dnaSeq *cdna;
    char *cdnaName;
    char *seekUntilName = NULL;
    char *faFileName;
    int seekUntilIx = -1;
    int endIx = 0x7fffffff;
    int dbIx = 0;
    char *ntDir;

    if (argc < 5)
        usage();
    faFileName = argv[3];
    ntDir = argv[4];

    reportFile = mustOpen(reportFileName, "a");

    if (!differentWord(command, "starting"))
        {
        char *numStr;
        if (argc != 6 && argc != 7)
            usage();
        numStr = argv[5];
        if (!isdigit(numStr[0]))
            usage();
        seekUntilIx = atoi(numStr)-1;
        if (argc == 7)
            {
            numStr = argv[6];
            if (!isdigit(numStr[0]))
                usage();
            endIx = seekUntilIx + atoi(numStr);
            }
        warn("Starting at %d\n\n", seekUntilIx+1);
        }
    else if (!differentWord(command, "resume"))
        {
        seekUntilName = lastCdnaReported(reportFileName);
        warn("Resuming after %s\n\n", seekUntilName);
        }
    else if (!differentWord(command, "all"))
        {
        }
    else
        {
        usage();
        }
    
    nt4Count = loadNt4s(ntDir, &nt4s);
    startEsts(faFileName);

    /* Possibly skip over some. */
    if (seekUntilName != NULL)
        {
        while ((cdna = nextEst()) != NULL)
            {
            cdnaName = cdna->name;
            ++dbIx;
            if (!differentWord(cdnaName, seekUntilName))
                break;
            freeDnaSeq(&cdna);
            }
        if (cdna == NULL)
            errAbort("Couldn't find %s in cDNA database", seekUntilName);
        freeDnaSeq(&cdna);
        }
    if (seekUntilIx > 0)
        {
        for (dbIx = 0; dbIx < seekUntilIx; ++dbIx)
            {
            if ((cdna = nextEst()) == NULL)
                errAbort("Can't seek to %d, there's only %d", seekUntilIx+1, dbIx+1);
            freeDnaSeq(&cdna);
            }
        }

    /* Main loop */
    while ((cdna = nextEst()) != NULL)
        {
        if (dbIx >= endIx)
            break;
        ++dbIx;
        cdnaName = cdna->name;
        reportBlasting(dbIx, cdna);
        myBlast(cdna, cdnaName, nt4s, nt4Count);
        freeDnaSeq(&cdna);
        }
    }
for (i=0; i<nt4Count; ++i)
    freeNt4(&nt4s[i]);
freez(&nt4s);
carefulClose(&reportFile);
return 0;
}
