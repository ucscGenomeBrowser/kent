/* newIntron.c - look for introns that are present in C. elegans but not 
 * C. briggsae and vice versa. */
#include "common.h"
#include "hash.h"
#include "dnautil.h"
#include "portable.h"
#include "gdf.h"
#include "wormdna.h"
#include "xa.h"

FILE *out;  /* Output file. */

void report(FILE *f, char *format, ...)
/* Write printf formatted report to file and standard out. */
{
va_list args;
va_start(args, format);
vfprintf(f, format, args);
vfprintf(stdout, format, args);
va_end(args);
}

#define uglyr report

FILE *openXa(char *organism)
/* Return file handle for xeno-alignments for given organism. */
{
char path[512];
sprintf(path, "%s%s/all.st", wormXenoDir(), organism);
return xaOpenVerify(path);
}

enum gapType
/* What type of gap this is - put in typeByte in features list. */
    {
    cIntronGap, cCodingGap, cIgGap, cIeMixedGap, cIgMixedGap, 
    };

char *gapTypeStrings[] =
    {
    "intronGap", "codingGap", "igGap", "ieMixdGap", "igMixdGap",
    };

boolean addInEx(struct gdfGene *gdfList, int start, int end, int *pInCount, int *pExCount, struct gdfGene **retGene)
/* Find out how much of segment between start and end intersects introns
 * and exons in gdfList, and add these to *pInCount and *pExCount. 
 * Returns FALSE if no intersection.  Returns gene intersected in *retGene */
{
int inCount, exCount, bothCount;
struct gdfGene *gdf;

for (gdf = gdfList; gdf != NULL; gdf = gdf->next)
    {
    struct gdfDataPoint *dp = gdf->dataPoints;
    int dpCount = gdf->dataCount;
    int i;
    inCount = exCount = 0;
    
    for (i=0; i<dpCount; i += 2)
        {
        int s, e, size;
        s = dp[i].start;
        if (start > s)
            s = start;
        e = dp[i+1].start;
        if (end < e)
            e = end;
        if ((size = e - s) > 0)        
            exCount += size;
        }
    for (i=1; i<dpCount-1; i += 2)
        {
        int s, e, size;
        s = dp[i].start;
        if (start > s)
            s = start;
        e = dp[i+1].start;
        if (end < e)
            e = end;
        if ((size = e - s) > 0)        
            inCount += size;
        }
    bothCount = inCount + exCount;
    if (bothCount > 0)
        {
        *retGene = gdf;
        *pInCount += inCount;
        *pExCount += exCount;
        return TRUE;
        }
    }
return FALSE;
}

void classifyGapT(struct gdfGene *gdfList, char *chrom, int start, int end,
    enum gapType *retType, struct gdfGene **retGene)
/* Classify based on where gap in target occurred. 
 * Assumes that gdfList is sorted. */
{
struct gdfGene *gdf;
int inCount = 0, exCount = 0, bothCount;

assert(start == end);
if (addInEx(gdfList, start-6, end+6, &inCount, &exCount, &gdf) )
    {
    bothCount = inCount + exCount;
    bothCount = inCount + exCount;
    if (bothCount >= 8)
        {
        *retGene = gdf;
        if (inCount >= 8)
            *retType = cIntronGap;
        else if (exCount >= 8)
            *retType = cCodingGap;
        else if (bothCount >= 12)
            *retType = cIeMixedGap;
        else 
            *retType = cIgMixedGap;
        return;
        }
    }
*retGene = NULL;
*retType = cIgGap;
return;
}

void classifyGapQ(struct gdfGene *gdfList, char *chrom, int start, int end,
    enum gapType *retType, struct gdfGene **retGene)
/* Classify based on where gap in query occurred. 
 * Assumes that gdfList is sorted. */
{
struct gdfGene *gdf;
int inCount = 0, exCount = 0, bothCount;

if (addInEx(gdfList, start-6, start, &inCount, &exCount, &gdf) 
    && addInEx(gdfList, end, end+6, &inCount, &exCount, &gdf) )
    {
    bothCount = inCount + exCount;
    if (bothCount >= 8)
        {
        *retGene = gdf;
        if (inCount >= 8)
            *retType = cIntronGap;
        else if (exCount >= 8)
            *retType = cCodingGap;
        else if (bothCount >= 12)
            *retType = cIeMixedGap;
        else 
            *retType = cIgMixedGap;
        return;
        }
    }
*retGene = NULL;
*retType = cIgGap;
return;
}

void classifyGap(struct gdfGene *gdfList, char *chrom, int start, int end, char hSym,
    enum gapType *retType, struct gdfGene **retGene)
/* Classify based on where gap occurred. Assumes that gdfList is sorted. */
{
if (hSym == 'T')
    classifyGapT(gdfList, chrom, start, end, retType, retGene);
else
    classifyGapQ(gdfList, chrom, start, end, retType, retGene);
}

boolean isConserved(char h)
/* Return TRUE is hidden symbol indicates conservation */
{
return (h == '1' || h == '2' || h == '3' || h == 'H');
}

boolean hasIntronEnds(char *intSym, int start, int end, char iStart[2], char iEnd[2])
/* Return TRUE if ends of intron at this position match consensus. */
{
boolean hasEnds = (intSym[start] == iStart[0] && intSym[start+1] == iStart[1]
    && intSym[end-2] == iEnd[0] && intSym[end-1] == iEnd[1]);
return hasEnds;
}

boolean isSlidableToIntron(char *intSym, char *gapSym, int start, int end, int symCount,
    char iStart[2], char iEnd[2], int *retSlide)
/* Return TRUE if could slide gap into an intron. */
{
int i;

if (hasIntronEnds(intSym, start, end, iStart, iEnd) )
    {
    *retSlide = 0;
    return TRUE;
    }
for (i=1; i<=5; ++i)
    {
    int s = start+i;
    int e = end+i;
    char sym;
    if (e > symCount)
        break;
    /* See if can move match around gap. */
    sym = intSym[e-1];
    if (sym == gapSym[e-1])
        if (sym != intSym[s-1])
            break;
    if (hasIntronEnds(intSym, s, e, iStart, iEnd) )
        {
        *retSlide = i; 
        return TRUE;
        }
    }
for (i=1; i <= 5; ++i)
    {
    int s = start-i;
    int e = end-i;
    char sym;
    if (s < 0)
        break;
    /* See if can move match around gap. */
    sym = intSym[s];
    if (sym == gapSym[s])
        if (sym != intSym[e])
            break;
    if (hasIntronEnds(intSym, s, e, iStart, iEnd) )
        {
        *retSlide = -i;
        return TRUE;
        }
    }
return FALSE;
}

boolean isIntron(struct xaAli *xa, int start, int end, char hSym, char strand, int *retSlide, boolean *retIsRc)
/* Return TRUE if gap looks like an intron. */
{
/* Intron boundaries are GT ... AG for forward strand,
 *                       CT ... AC for reverse strand. */
char *intSym, *gapSym;
int symCount = xa->symCount;
if (hSym == 'T')
    {
    gapSym = xa->tSym;
    intSym = xa->qSym;
    }
else
    {
    gapSym = xa->qSym;
    intSym = xa->tSym;
    }
if (strand == '+')
    {
    *retIsRc = FALSE;
    return isSlidableToIntron(intSym, gapSym, start, end, symCount, "gt", "ag", retSlide);
    }
else if (strand == '-')
    {
    *retIsRc = TRUE;
    return isSlidableToIntron(intSym, gapSym, start, end, symCount, "ct", "ac", retSlide);
    }
else
    {
    if (isSlidableToIntron(intSym, gapSym, start, end, symCount, "gt", "ag", retSlide))
        {
        *retIsRc = FALSE;
        return TRUE;
        }
    if (isSlidableToIntron(intSym, gapSym, start, end, symCount, "ct", "ac", retSlide))
        {
        *retIsRc = TRUE;
        return TRUE;
        }
    }
return FALSE;
}

struct gapInfo
/* Information on one gap. */
    {
    struct gapInfo *next;
    char *query;
    int qStart, qEnd;
    char *target;
    int tStart, tEnd;
    char *name;
    int size;
    char hSym;
    enum gapType type;
    boolean hasIntronEnds;
    boolean hasStrongHomology;
    boolean isRc;               /* Need to reverse complement to get GT..AG ends? */
    int slideCount;             /* Amount to slide to get GT..AG ends. */
    };

#define exSize 15
#define inSize exSize
#define halfSize exSize
#define totSize (exSize+inSize)

int hist5[totSize][5];
int hist3[totSize][5];
int histCount;

int sum4(int *a)
{
return a[0]+a[1]+a[2]+a[3];
}

void printLineOfHis(FILE *f, int his[totSize][5], DNA base, int start, int size)
/* Print a single line of histogram. */
{
int baseVal = ntVal[base];
int i;
long millis;

fprintf(f, "%c ", base);
for (i=start; i<start+size; ++i)
    {
    millis = his[i][baseVal] * 1000 / sum4(his[i]);
    if (millis >= 10)
	fprintf(f, "%3d%% ", (millis+5)/10);
    else
	fprintf(f, "%d.%d%% ", millis/10, millis%10);
    }
fprintf(f, "\n");
}

void printHis(FILE *f, int his[totSize][5], int start, int size)
/* Print one frequency histogram. */
{
printLineOfHis(f, his, 'a', start, size);
printLineOfHis(f, his, 'c', start, size);
printLineOfHis(f, his, 'g', start, size);
printLineOfHis(f, his, 't', start, size);
fprintf(f, "\n");
}

void printAllHistograms(FILE *f)
/* Print all frequency histograms. */
{
fprintf(f, "Frequency Counts of 5' Ends:\n");
printHis(f, hist5, exSize-7, 14);
fprintf(f, "Frequency Counts of 3' Ends:\n");
printHis(f, hist3, inSize-7, 14);

#ifdef USUAL
fprintf(f, "Frequency Counts of 5' Ends of Exons:\n");
printHis(f, hist5, 0, exSize);
fprintf(f, "Frequency Counts of Intron Starts:\n");
printHis(f, hist5, exSize, inSize);
fprintf(f, "Frequency Counts of Intron Ends:\n");
printHis(f, hist3, 0, inSize);
fprintf(f, "Frequency Counts of 3' Ends of Exons:\n");
printHis(f, hist3, inSize, exSize);
#endif
}


int ceOnlyCount;
int ceOnlyHomoCount[2*halfSize];
int cbOnlyCount;
int cbOnlyHomoCount[2*halfSize];
int bothCount;
int bothHomoCount[2*halfSize];
int ceCount;
int ceHomoCount[2*halfSize];

void printHomoEnds(FILE *f, char *title, int count, int homoCount[2*halfSize])
/* Print totals of one type of homology. */
{
int i;
fprintf(f, "%s (%d total)\n", title, count);
if (count > 0)
    {
    for (i=0; i<2*halfSize; ++i)
        {
        if (i == halfSize)
            fprintf(f, "^ ");
        fprintf(f, "%2d ", round(100.0 * homoCount[i] / count)); 
        }
    }
fprintf(f, "\n");
}

void printHomologousEndStats(FILE *f)
/* Print statistics about how 3' and 5' splice sites are to each other. */
{
fprintf(f, "Percent of match between 3' and 5' splice sites.\n");
printHomoEnds(f, "C. elegans only", ceOnlyCount, ceOnlyHomoCount);
printHomoEnds(f, "C. briggsae only", cbOnlyCount, cbOnlyHomoCount);
printHomoEnds(f, "Either unique", bothCount, bothHomoCount);
printHomoEnds(f, "C. elegans total", ceCount, ceHomoCount);
}

void calcCeHomoCount()
/* Calculate total homologies between 3' and 5' ends for
 * all C. elegans introns. */
{
char *intronFileName = "C:/biodata/celegans/cDNA/introns.txt";
FILE *f = mustOpen(intronFileName, "r");
int lineCount = 0;
static char line[2048];
int wordCount;
char *words[16];
char *exonBefore, *exonAfter, *intronStart, *intronEnd;
boolean firstTime = TRUE;
int ebSize, ieSize;
int ebOff, ieOff;
int i;

while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount < 10)
        errAbort("Short line %d of %s\n", lineCount, intronFileName);
    exonBefore = words[6];
    intronStart = words[7];
    intronEnd = words[8];
    exonAfter = words[9];
    if (firstTime)
        {
        ebSize = strlen(exonBefore);
        ebOff = ebSize - halfSize;
        ieSize = strlen(intronEnd);
        ieOff = ieSize - halfSize;
        firstTime = FALSE;
        }
    for (i=0; i<halfSize; ++i)
        {
        if (exonBefore[i+ebOff] == intronEnd[i+ieOff])
            ceHomoCount[i] += 1;
        if (intronStart[i] == exonAfter[i])
            ceHomoCount[i+halfSize] += 1;
        }        
    ++ceCount;
    }
fclose(f);
}

int histIx(char c)
/* Return histogram index 0-3 for a base and 4 for a gap. */
{
int val;
if ((val = ntVal[c]) < 0)
    val = 4;
return val;
}

boolean noInserts(char *s, int size)
/* Return TRUE if no insert characters in s. */
{
int i;
for (i=0; i<size; ++i)
    if (s[i] == '-')
        return FALSE;
return TRUE;
}

#define intronEndsSize 12
struct intronList 
    {
    struct intronList *next;
    char ends[intronEndsSize*2+1];
    int count;
    boolean isQgap;
    boolean onBoth;
    };

struct intronList *intronList = NULL;
struct hash *intronHash = NULL;

void printSameIntronStats(FILE *f)
/* Print information about how many introns are unique. */
{
int totCount = 0;
int listCount = 0;
int maxCount = 0;
struct intronList *il;
int dupeCount = 0;
int dupeTotal = 0;
int dupeQ = 0, dupeT = 0;
int bothCount = 0;


for (il = intronList; il != NULL; il = il->next)
    {
    totCount += il->count;
    listCount += 1;
    if (il->count > maxCount)
        maxCount = il->count;
    if (il->count > 1)
        {
        ++dupeCount;
        dupeTotal += il->count;
        if (il->isQgap)
            ++dupeQ;
        else
            ++dupeT;
        if (il->onBoth)
            ++bothCount;
        }
    }
fprintf(out, "Stats on how many introns unique to species share share %d bases on either end\n", intronEndsSize);
fprintf(out, "%d total unique introns\n", totCount);
fprintf(out, "%d distinct unique introns\n", listCount);
fprintf(out, "%d maximum occurrences of a unique intron\n", maxCount);
fprintf(out, "Average unique intron appears %4.2f times\n", (double)totCount/listCount);
fprintf(out, "%d dupes total, %d in briggs, %d in elegans\n",
    dupeCount, dupeT, dupeQ);
fprintf(out, "%d introns in dupe sets\n", dupeTotal);
fprintf(out, "%d dupes on both\n\n", bothCount);
}


void writeGap(struct gapInfo *gap, struct xaAli *xa, 
    int symStart, int symEnd, char geneStrand, FILE *f)
/* Write out info on one gap to file. */
{
char qStart[totSize+1], qEnd[totSize+1];
char tStart[totSize+1], tEnd[totSize+1];
char hStart[totSize+1], hEnd[totSize+1];
int s, e, size;
int midSize;
int i;
char *threePrime, *fivePrime;
boolean isQgap;

fprintf(f, "%s %s %s hom  %s:%d-%d %c %s:%d-%d %c slide %d\n",
    gapTypeStrings[gap->type], 
    (gap->hasIntronEnds ? " intron" : "!intron"),
    (gap->hasStrongHomology ? "heavy" : "light"),
    gap->query, gap->qStart, gap->qEnd, xa->qStrand,
    gap->target, gap->tStart, gap->tEnd, geneStrand, gap->slideCount);
s = symStart-exSize;
e = symStart + inSize;
if (s < 0)
    s = 0;
size = e-s;

uglyf("s %d size %d e %d totSize %d\n", s, size, e, totSize);

strncpy(qStart, xa->qSym+s, size);
strncpy(tStart, xa->tSym+s, size);
strncpy(hStart, xa->hSym+s, size);
qStart[size] = tStart[size] = hStart[size] = 0;

// uglyf - crashes by here

s = symEnd-inSize;
midSize = s - e;
e = symEnd+exSize;
if (e > xa->symCount)
    e = xa->symCount;
size = e-s;
strncpy(qEnd, xa->qSym+s, size);
strncpy(tEnd, xa->tSym+s, size);
strncpy(hEnd, xa->hSym+s, size);
qEnd[size] = tEnd[size] = hEnd[size] = 0;

if (gap->isRc)
    {
    swapBytes(qStart, qEnd, totSize);
    swapBytes(tStart, tEnd, totSize);
    swapBytes(hStart, hEnd, totSize);
    reverseComplement(qStart, totSize);
    reverseComplement(qEnd, totSize);
    reverseComplement(tStart, totSize);
    reverseComplement(tEnd, totSize);
    reverseBytes(hStart, totSize);
    reverseBytes(hEnd, totSize);
    }

/* Write out ends of gap to file. */
fprintf(f, "%s  ...%d... %s\n", qStart, midSize, qEnd);
fprintf(f, "%s  ...%d... %s\n", tStart, midSize, tEnd);
fprintf(f, "%s  ...%d... %s\n\n", hStart, midSize, hEnd);

/* Add intron ends to consensus sequence histogram. */
if (gap->hasIntronEnds && gap->type == cCodingGap)
    {
    isQgap = (qStart[exSize] == '-');
    if (isQgap)
        {
        fivePrime = tStart;
        threePrime = tEnd;
        }
    else
        {
        fivePrime = qStart;
        threePrime = qEnd;
        }
    if (noInserts(threePrime, totSize) && noInserts(fivePrime, totSize) )
        {
        int *homoCount;
        for (i=0; i<totSize; ++i)
            {
            hist5[i][histIx(fivePrime[i])] += 1;
            hist3[i][histIx(threePrime[i])] += 1;
            }
        ++histCount;
        if (isQgap)
            {
            ++ceOnlyCount;
            homoCount = ceOnlyHomoCount;
            }
        else
            {
            ++cbOnlyCount;
            homoCount = cbOnlyHomoCount;
            }
        ++bothCount;
        for (i=0; i<totSize; ++i)
            {
            if (fivePrime[i] == threePrime[i])
                {
                homoCount[i] += 1;
                bothHomoCount[i] += 1;
                }
            }
        /* Add introns to list. */
            {
            char idBuf[2*intronEndsSize+1];
            struct intronList *il;
            struct hashEl *hel;
            memcpy(idBuf, fivePrime+exSize, intronEndsSize);
            memcpy(idBuf+intronEndsSize, threePrime, intronEndsSize);
            idBuf[ sizeof(idBuf)-1 ] = 0;
            if ((hel = hashLookup(intronHash, idBuf)) != NULL)
                {
                il = hel->val;
                il->count += 1;
                fprintf(f, ">>>%d of set<<<\n", il->count);
                if (il->isQgap != isQgap)
                    {
                    il->onBoth = TRUE;
                    }
                }
            else
                {
                AllocVar(il);
                strcpy(il->ends, idBuf);
                il->count = 1;
                il->isQgap = isQgap;
                slAddHead(&intronList, il);
                hashAdd(intronHash, idBuf, il);
                }    
            }
        }
    else
        {
        static insertCount = 0;
        warn("Skipping intron with flanking inserts %d", ++insertCount);
        }
    }
}

void slideGap(struct gapInfo *gap, struct xaAli *xa, char gapType,
    int symStart, int symEnd)
/* Rearrange gap position.  Make things looking like:
     cagaggctctcttccaggtaagcttattcaa  ...18... atcgatatttttacagccatccttcttgggt  Q
     cagaggctatgttcca---------------  ...18... ---------------gccatccttcttgggt  T
   look like
     cagaggctctcttccaggtaagcttattcaa  ...18... atcgatatttttacagccatccttcttgggt
     cagaggctatgttccag--------------  ...18... ----------------ccatccttcttgggt
   instead. */
{
int slideCount = gap->slideCount;
if (slideCount != 0)
    {
    char oc;
    char *sym = (gapType == 'Q' ? xa->qSym : xa->tSym);
    char *hSym = xa->hSym;
    int i;
    if (slideCount > 0)
        {
        for (i=0; i< slideCount; ++i)
            {
            int s = symStart + i;
            int e = symEnd + i;
            oc = hSym[s];
            hSym[s] = hSym[e];
            hSym[e] = oc;
            oc = sym[s];
            sym[s] = sym[e];
            sym[e] = oc;
            }
        }
    else
        {
        for (i=0; i > slideCount; --i)
            {
            int s = symStart + i - 1;
            int e = symEnd + i - 1;
            oc = hSym[s];
            hSym[s] = hSym[e];
            hSym[e] = oc;
            oc = sym[s];
            sym[s] = sym[e];
            sym[e] = oc;
            }
        }       
    }
}

boolean uniqueGap(struct gapInfo *oldList, struct gapInfo *gap)
/* Return TRUE if gap looks to be unique in where it occurs
 * in query. */
{
struct gapInfo *old;
int tolerance = 8;

for (old = oldList; old != NULL; old = old->next)
    {
    if (!sameString(old->query, gap->query))
        break;
    if (intAbs(old->qStart-gap->qStart) <= tolerance && intAbs(old->qEnd-gap->qEnd) <= tolerance)
        return FALSE;
    if (sameString(old->target, gap->target) &&
        intAbs(old->tStart-gap->tStart) <= tolerance && intAbs(old->tEnd-gap->tEnd) <= tolerance)        
        return FALSE;
    }
return TRUE;
}


struct gapInfo *findLargeGaps(struct xaAli *xa, struct gapInfo *oldList)
/* Find large gaps in alignment and classify them. */
{
struct gdfGene *gdfList;
struct gapInfo *gapList = NULL, *gap;
int ceIx=0, cbIx=0, symIx=0;
int ceStart=0, cbStart=0, symStart=0;
int runSize = 0;
char sym, lastSym = 0;
int symCount = xa->symCount;

/* Fetch C. elegans region. */
gdfList = wormGdfGenesInRange(xa->target, xa->tStart, xa->tEnd, &wormSangerGdfCache);

/* Run a little state machine that does something at the end of each solid run 
 * of a symbol. */
for (symIx = 0; symIx <= symCount; ++symIx)
    {
    sym = xa->hSym[symIx];
    if (sym != lastSym)
        {
        if (runSize > 32)       /* Introns need to be at least this long. */
            {
            /* We're at end of a solid run. */
            if (lastSym == 'Q' || lastSym == 'T')
                {
                int ceGapStart = xa->tStart + ceStart;
                int ceGapEnd = xa->tStart + ceIx;
                struct gdfGene *gdf;
                char hBefore = xa->hSym[symStart-1];
                char hAfter = sym;
                char strand = '.';

                AllocVar(gap);
                gap->query = cloneString(xa->query);
                gap->qStart = xa->qStart + cbStart;
                gap->qEnd = xa->qStart + cbIx;
                gap->target = cloneString(xa->target);
                gap->tStart = ceGapStart;
                gap->tEnd = ceGapEnd;
                gap->name = cloneString(xa->name);
                gap->size = runSize;
                gap->hSym = lastSym;
                if (uniqueGap(oldList, gap))
                    {
                    slAddHead(&gapList, gap);

                    classifyGap(gdfList, xa->target, ceGapStart, ceGapEnd, lastSym, &gap->type, &gdf);
                    if (gdf != NULL)
                        strand = gdf->strand;
                    gap->hasIntronEnds = isIntron(xa, symStart, symIx, lastSym, strand, &gap->slideCount, &gap->isRc);
                    if (gap->hasIntronEnds)
                        slideGap(gap, xa, lastSym, symStart, symIx);
                    if (isConserved(hBefore) && isConserved(hAfter))
                        gap->hasStrongHomology = TRUE;
                    if (gap->hasStrongHomology)
                        {
                        if (lastSym == 'T')
                            writeGap(gap, xa, symStart+gap->slideCount, symIx+gap->slideCount, strand, out);
                        }
                    }
                }
            }
        runSize = 0;
        ceStart = ceIx;
        cbStart = cbIx;
        symStart = symIx;
        lastSym = sym;
        }
    ++runSize;
    if (xa->qSym[symIx] != '-')
        ++cbIx;
    if (xa->tSym[symIx] != '-')
        ++ceIx;
    }

gdfFreeGeneList(&gdfList);
slReverse(&gapList);
return gapList;
}

int countGapType(struct gapInfo *gapList, int type)
/* Return number of gaps of given type. */
{
struct gapInfo *gap;
int count = 0;
for (gap = gapList; gap != NULL; gap = gap->next)
    if (type == gap->type)
        ++count;
return count;
}

void countGapHomology(struct gapInfo *gapList, int *retWeak, int *retStrong)
/* Count up gap homologies. */
{
struct gapInfo *gap;
int weakCount=0, strongCount=0;
for (gap = gapList; gap != NULL; gap = gap->next)
    {
    if (gap->hasStrongHomology)
        ++strongCount;
    else
        ++weakCount;
    }
*retWeak = weakCount;
*retStrong = strongCount;
}

int countIntronEnds(struct gapInfo *gapList)
/* Count up number of gaps with intron ends. */
{
struct gapInfo *gap;
int count = 0;
for (gap = gapList; gap != NULL; gap = gap->next)
    if (gap->hasIntronEnds && gap->hasStrongHomology)
        ++count;
return count;
}

int countHsym(struct gapInfo *gapList, char hSym)
/* Count up number of gaps that have given hSym. */
{
struct gapInfo *gap;
int count = 0;
for (gap = gapList; gap != NULL; gap = gap->next)
    if (hSym == gap->hSym)
        ++count;
return count;
}

int countStrongCoding(struct gapInfo *gapList)
/* Count up number of gaps in coding regions with strong homology */
{
struct gapInfo *gap;
int count = 0;
for (gap = gapList; gap != NULL; gap = gap->next)
    if (gap->type == cCodingGap && gap->hasStrongHomology)
        ++count;
return count;
}

int countSpecific(struct gapInfo *gapList, boolean hasStrongHomology, boolean hasIntronEnds, char hSym, int type)
/* Count up number of gaps that match homology, intron ends, etc. */
{
struct gapInfo *gap;
int count = 0;
for (gap = gapList; gap != NULL; gap = gap->next)
    {
    if (gap->hasStrongHomology == hasStrongHomology 
     && gap->hasIntronEnds == hasIntronEnds
     && gap->hSym == hSym
     && gap->type == type)
        ++count;
    }
return count;
}


void reportGaps(struct gapInfo *gapList, FILE *f)
/* Write out info on gaps to file, and summary info to 
 * console as well. */
{
int gapCount;
int qGaps, tGaps;
int weakGaps, strongGaps;
int intronGaps;
int typeGaps[5];
int i;


gapCount = slCount(gapList);
qGaps = countHsym(gapList, 'Q');
tGaps = gapCount - qGaps;
countGapHomology(gapList, &weakGaps, &strongGaps);
intronGaps = countIntronEnds(gapList);
for (i=0; i<ArraySize(typeGaps); ++i)
    typeGaps[i] = countGapType(gapList, i);

report(f, "%d total inserts of length more than 32\n", gapCount);
report(f, "%d C. elegans inserts\n", qGaps);
report(f, "%d C. briggsae inserts\n", tGaps);
report(f, "%d inserts in regions of strong homology\n", strongGaps);
report(f, "%d inserts in regions of weak homology\n", weakGaps);
report(f, "%d inserts slidable to GT...AG ends in regions of strong homology\n", intronGaps);
report(f, "%d inserts in coding regions\n", typeGaps[cCodingGap]);
report(f, "%d inserts in intron regions\n", typeGaps[cIntronGap]);
report(f, "%d inserts in intergenic regions\n", typeGaps[cIgGap]);
report(f, "%d inserts straddling intron and exons\n", typeGaps[cIeMixedGap]);
report(f, "%d inserts straddling genes and intergenic\n", typeGaps[cIgMixedGap]);

report(f, "%d inserts in coding regions with strong homology\n", 
    countStrongCoding(gapList));

report(f, "%d introns unique to C. elegans\n",
    countSpecific(gapList, TRUE, TRUE, 'Q', cCodingGap));
report(f, "%d introns unique to C. briggsae\n",
    countSpecific(gapList, TRUE, TRUE, 'T', cCodingGap));
}

void usage()
/* Print usage summary and exit. */
{
errAbort(
"newIntron - find introns present in one species but not the other\n"
"usage:\n"
"    newIntron firstSpecies wabaFile outFile\n"
"where 'firstSpecies' is either 'elegans' or 'briggsae'\n");
}

char *chromFromPath(char *path)
/* Extract chromosome name from path */
{
static char chromosome[128];
char dir[256], name[128], extension[64];
char *s;
splitPath(path, dir, name, extension);
s = strchr(name, '.');
if (s != NULL)
    *s = 0;
s = cloneString(name);
tolowers(s);
return s;
}

void swapSym(char *sym, int symCount)
/* Swap 'Q' and 'T' symbols. */
{
char s;
int i;
for (i=0; i<symCount; ++i)
    {
    s = sym[i];
    if (s == 'Q')
	sym[i] = 'T';
    else if (s == 'T')
	sym[i] = 'Q';
    }
}

int main(int argc, char *argv[])
{
FILE *xaFile;
struct xaAli *xa;
struct gapInfo *gapList = NULL, *gaps;
int count = 0;
long startTime = clock1000();
char *xaName, *newName;
char *first;
boolean cbFirst;

if (argc != 4)
    usage();
first = argv[1];
xaName = argv[2];
newName = argv[3];
if (sameWord("elegans", first))
    cbFirst = FALSE;
else if (sameWord("briggsae", first))
    cbFirst = TRUE;
else
    usage();
dnaUtilOpen();
intronHash = newHash(0);
out = mustOpen(newName, "w");
xaFile = mustOpen(xaName, "r");
while ((xa = xaReadNext(xaFile, FALSE)) != NULL)
    {
    char *s;
    if (!cbFirst)
	{
	char *swaps;
	int swapi;
	char swapc;
	uglyf("Swapping....\n");
	swaps = xa->query;
	xa->query = xa->target;
	xa->target = swaps;
	swapi = xa->qStart;
	xa->qStart = xa->tStart;
	xa->tStart = swapi;
	swapi = xa->qEnd;
	xa->qEnd = xa->tEnd;
	xa->tEnd = swapi;
	swapc = xa->qStrand;
	xa->qStrand = xa->tStrand;
	xa->tStrand = swapc;
	swaps = xa->qSym;
	xa->qSym = xa->tSym;
	xa->tSym = swaps;
	swapSym(xa->hSym, xa->symCount);
	}
    uglyf("%d  query %s target %s\n", count, xa->query, xa->target);
    s = chromFromPath(xa->target);
    freeMem(xa->target);
    xa->target = s;
    if (++count % 500 == 0)
        printf("Processing %d\n", count);
    gaps = findLargeGaps(xa, gapList);
    gapList = slCat(gaps, gapList);
    xaAliFree(xa);
    }
slReverse(&gapList);
report(out, "Processing took %f seconds\n", (clock1000()-startTime)*0.001);

reportGaps(gapList, out);
printAllHistograms(out);
calcCeHomoCount();
printHomologousEndStats(out);
printSameIntronStats(out);
return 0;
}

