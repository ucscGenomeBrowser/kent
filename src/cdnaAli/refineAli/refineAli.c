/* RefineAli.c - Stuff to check and refine alignments.
 *
 * This program checks alignments and refines alignments.
 *      
 * The alignments are checked at the edges, and if at least decent in
 * the middle, but not as good on the edges, the alignment is done
 * again against a larger chunk of genomic DNA on the side(s) that don't
 * match up.
 *
 * After any reallignment, full data about decent or better alignments 
 * is written to a file.  This info is:
 *      cdnaName
 *          score, targetChromosome, targetStrand
 *                length, cdnaOffset, targetOffset
 *                length, cdnaOffset, targetOffset
 *          score, targetChromosome, targetStrand
 *                length, cdnaOffset, targetOffset
 *                length, cdnaOffset, targetOffset
 * Poor and none alignments are written to another file as simply
 *      cdnaName
 * with the intention of eventually blasting these, or running them
 * with an exonAli with smaller tile size or something. */   
                
#include "common.h"
#include "portable.h"
#include "hash.h"
#include "obscure.h"
#include "errabort.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "snof.h"
#include "fa.h"
#include "nt4.h"
#include "wormdna.h"
#include "fuzzyFind.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cdnaAli.h"


void loadGenome(char *nt4Dir, struct nt4Seq ***retNt4Seq, char ***retChromNames, int *retNt4Count)
/* Load up entire packed  genome into memory. */
{
struct slName *nameList, *name;
int count;
int i;
char **chromNames;
struct nt4Seq **chroms;
char fileName[512];
char chromName[256];

nameList = listDir(nt4Dir, "*.nt4");
count = slCount(nameList);
chroms = needMem(count*sizeof(chroms[0]));
chromNames = needMem(count*sizeof(chromNames[0]));

for (name = nameList, i=0; name != NULL; name = name->next, i+=1)
    {
    char *end;

    sprintf(fileName, "%s/%s", nt4Dir, name->name);
    
    /* Cut off .nt4 suffix. */
    strcpy(chromName, name->name);
    end = strrchr(chromName, '.');
    assert(end != NULL);
    *end = 0;
    
    chroms[i] = loadNt4(fileName, chromName);
    chromNames[i] = chroms[i]->name;
    }
slFreeList(&nameList);
*retNt4Seq = chroms;
*retChromNames = chromNames;
*retNt4Count = count;
}

void freeGenome(struct nt4Seq ***pNt4Seq, char ***pChromNames, int chromCount)
/* Free up genome. */
{
int i;
struct nt4Seq **nt4s = *pNt4Seq;
for (i=0; i<chromCount; ++i)
    {
    freeNt4(&nt4s[i]);
    }
freez(pNt4Seq);
freez(pChromNames);
}



struct fineAli
    /* A (hopefully) refined alignment. */
    {
    struct fineAli *next;
    boolean isDupe;
    boolean isGood;
    int chromIx;
    boolean isRc, isBackwards;
    int score;
    struct ffAli *blocks;
    DNA *virtNeedle;    /* Not referenced, just used for arithmetic. */
    DNA *virtHaystack;  /* Also not referenced. */
    long nStart, nEnd, hStart, hEnd;
    char *geneName;
    long geneStart, geneEnd;
    };

struct roughAli
    /* A rough alignment .*/
    {
    struct roughAli *next;
    boolean isDupe;
    int chromIx;
    int score;
    int cStart, cEnd;   /* Boundaries on cDNA. */
    int gStart, gEnd;   /* Boundaries on genome. */
    };

struct cdnaInfo
    /* Accumulated info about one cDNA. */
    {
    struct cdnaInfo *next;
    struct cdnaInfo *crcHashNext;
    int ix;
    char *name;
    int baseCount;
    bits32 baseCrc;
    boolean finished;
    boolean isDupe;
    boolean isBackwards;
    int fineScore;
    int roughScore;
    struct roughAli *roughAli;
    struct fineAli *fineAli;
    boolean isEmbryonic;
    };


char *inName, *goodLogName, *badLogName, *unusualName, *errName;
FILE *inFile, *goodLogFile, *badLogFile, *unusualFile, *errFile;
char *cdnaName, *chromDir, *c2gName = NULL;
struct nt4Seq **chroms;
int chromCount;

struct hash *redoHash;

void startRedoHash()
{
redoHash = newHash(12);
}

void addRedoHash(struct cdnaInfo *ci, char *reason)
{
struct hashEl *hel;
if ((hel = hashLookup(redoHash, ci->name)) == NULL)
    hashAdd(redoHash, ci->name, reason);
}

#ifdef OLD
void endRedoHash()
{
char *redoName = "redo.txt";
FILE *redoFile = mustOpen(redoName, "w");
struct hashEl *hel;
int i;

for (i=0; i<redoHash->size; ++i)
    {
    for (hel = redoHash->table[i]; hel != NULL; hel = hel->next)
        {
        fprintf(redoFile, "%s %s\n", hel->name, hel->val);
        }
    }
freeHash(&redoHash);
}
#endif

boolean anyCdnaSeq(char *name, struct dnaSeq **retDna, struct wormCdnaInfo *retInfo)
/* Get a single  cDNA sequence. Optionally (if retInfo is non-null) get additional
 * info about the sequence. */
{
static FILE *cdnaFa;
static struct snof *cdnaSnof = NULL;
long offset;
char *faComment;
char **pFaComment = (retInfo == NULL ? NULL : &faComment);

if (cdnaSnof == NULL)
    {
    char buf[512];

    cdnaSnof = snofMustOpen(cdnaName);
    sprintf(buf, "%s%s", cdnaName, ".fa");
    cdnaFa = mustOpen(buf, "rb");
    }
if (!snofFindOffset(cdnaSnof, name, &offset))
    return FALSE;
fseek(cdnaFa, offset, SEEK_SET);
if (!faReadNext(cdnaFa, name, TRUE, pFaComment, retDna))
    return FALSE;
if (retInfo != NULL)
    {
    /* Kludge - only look up info if format is more or less right. */
    int fieldCount = countChars(faComment, '|');
    if (fieldCount >= 8)
        wormFaCommentIntoInfo(faComment, retInfo);
    else
        zeroBytes(retInfo, sizeof(*retInfo));
    }
return TRUE;
}

boolean weAreWeb()
/* Return true if we're executing as a CGI app from a browser. */
{
static boolean checked = FALSE;
static boolean result = FALSE;

if (!checked)
    {
    checked = TRUE;
    if (getenv("QUERY_STRING") != NULL)
        result = TRUE;
    }
return result;
}

char *lineBreak()
/* Return line break string. */
{
if (weAreWeb())
    return "<BR>\n";
else    
    return "\n";
}

void vaReportFile(char *format, va_list args, FILE *file, char *pre, char *post)
/* Write printf formatted report to file, prepending extra if non-NULL */
{
if (pre != NULL)
    fprintf(file, "%s", pre);
vfprintf(file, format, args);
if (post != NULL)
    fprintf(file, "%s", post);
}

void reportGood(char *format, ...)
/* Printf to good log file and console. */
{
va_list args;
va_start(args, format);
//vaReportFile(format, args, stdout, NULL, NULL);
vaReportFile(format, args, goodLogFile, NULL, NULL);
va_end(args);
}

void reportBad(char *format, ...)
/* Printf to bad log file and console. */
{
va_list args;
va_start(args, format);
//vaReportFile(format, args, stdout, NULL, NULL);
vaReportFile(format, args, badLogFile, NULL, NULL);
va_end(args);
}

void reportUnusual(char *format, ...)
/* Printf to unusual log file. */
{
va_list args;
va_start(args, format);
vaReportFile(format, args, stdout, NULL, NULL);
vaReportFile(format, args, unusualFile, NULL, NULL);
va_end(args);
}

void reportWarning(char *format, va_list args)
/* Print warning to console and also to log files. */
{
vaReportFile(format, args, stdout, "###", lineBreak());
vaReportFile(format, args, errFile, "###", "\n");
fflush(errFile);
}

void usageErr()
/* Print usage info and bail */
{
errAbort(
"This program turns rough alignments into fine ones.\n"
"usage:\n"
"      refineAli roughInputFile cdnaBase chromDir goodAlignFile badAlignFile coolFile errorFile startIx endIx [c2gFile]\n"
"example:\n"
"      refineAli ea\\all.out cDNA\\allcdna chrom ra\\good.txt ra\\bad.txt ra\\cool.txt ra\\err.txt 0 100000 features\\c2g\n");
}

char **chromNames;


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
while (hel != NULL)
    {
    if (hel->baseCount == ci->baseCount && hel->baseCrc == ci->baseCrc)
        {
        if (!anyCdnaSeq(hel->name, &otherSeq, NULL))
            errAbort("Can't find other cDNA %s", hel->name);
        if (memcmp(cdnaSeq->dna, otherSeq->dna, cdnaSeq->size) == 0)
            {
            warn("%s Duplicate cDNA (%d %s and %d %s)", 
                ci->name, ci->ix, ci->name, hel->ix, hel->name);
            ci->isDupe = TRUE;
            break;
            }
        else
            {
            warn("%s Misleading CRC match (%d %s and %d %s)",
                ci->name, ci->ix, ci->name, hel->ix, hel->name);
            }
        }
    hel = hel->crcHashNext;
    }
ci->crcHashNext = crcHash[hashVal];
crcHash[hashVal] = ci;
}

struct cdnaInfo *lookupInfo(struct hash *hash, char *name)
{
struct hashEl *el = hashLookup(hash, name);
if (el == NULL)
    return NULL;
return (struct cdnaInfo *)(el->val);
}


void findAliEnds(struct ffAli *ali, DNA *needle, DNA *hay,
    long *retNeedleStart, long *retNeedleEnd,
    long *retHayStart, long *retHayEnd)
{
DNA *hStart = NULL;
DNA *hEnd = NULL;
DNA *nStart = NULL;
DNA *nEnd = NULL;
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

void fetchUnpacked(int chromIx, int gStart, int gEnd, int extraOnEnds,
    DNA **retDna, int *retStart, int *retEnd)
/* This routine fetches DNA for a segment of chromosome. 
 * Because of packing issues it may fetch a bit more than
 * requested.  retStart and retEnd are the beginning and
 * end offsets of what's actually fetched. */
{
struct nt4Seq *nt4 = chroms[chromIx];
DNA *unpacked;

/* Adjust endpoints to include extra clipped to fit inside chromosome. */
gStart -= extraOnEnds;
if (gStart < 0)
    gStart = 0;
gEnd += extraOnEnds;
if (gEnd > nt4->baseCount)
    gEnd = nt4->baseCount;

unpacked = nt4Unpack(nt4, gStart, gEnd-gStart);

/* Set return variables. */
*retDna = unpacked;
*retStart = gStart;
*retEnd = gEnd;
}

int goodNts(DNA *dna, int size)
/* Count up number of non-N nucleotides. */
{
int count = 0;
while (--size >= 0)
    {
    if (ntVal[*dna++] >= 0)
        ++count;
    }
return count;
}

#define minGoodSize 12

int leftFlakySize(struct ffAli *ali, DNA *needle, int needleSize)
/* Returns # of unalligned or flakily aligned bases on left side
 * of needle. */
{
int aliSize;
DNA *firstGood = NULL;

while (ali->left)   ali = ali->left;
while (ali != NULL)
    {
    aliSize = ali->nEnd - ali->nStart;
    if (ffScoreMatch(ali->nStart, ali->hStart, aliSize) >= minGoodSize)
        {
        firstGood = ali->nStart;
        break;
        }
    ali = ali->right;
    }
if (firstGood == NULL)
    return needleSize;
else
    return firstGood - needle;
}

int rightFlakySize(struct ffAli *ali, DNA *needle, int needleSize)
/* Returns # of unalligned or flakily aligned bases on right side
 * of needle. */
{
int aliSize;
DNA *firstGood = NULL;
while (ali->right) ali = ali->right;

while (ali != NULL)
    {
    aliSize = ali->nEnd - ali->nStart;
    if (ffScoreMatch(ali->nStart, ali->hStart, aliSize) >= minGoodSize)
        {
        firstGood = ali->nEnd;
        break;
        }
    ali = ali->left;
    }
if (firstGood == NULL)
    return needleSize;
else
    return (needle + needleSize) - firstGood;
}

int polyaSize(DNA *cdna, int cdnaSize)
/* Return size of polyA tail. */
{
int count=0;
while (--cdnaSize >= 0)
    {
    if (cdna[cdnaSize] != 'a')
        break;
    ++count;
    }
if (count < 4)  /* Not really poly ... */
    return 0;
return count;
}

int gapPenalty(struct ffAli *left, struct ffAli *ali)
/* Calculate gap penalty using exon scoring. */
{
int nGap, hGap;
int minGap;

nGap = ali->nStart - left->nEnd;
assert(nGap >= 0);
hGap = ali->hStart - left->hStart;
if (hGap < 0)       
    {
    hGap = 0;
    nGap -= hGap;
    }
minGap = (nGap < hGap ? nGap : hGap);
return (2 + digitsBaseTwo(hGap+minGap) + (nGap-minGap));
}
    
int scoreExonAli(struct ffAli *ali)
/* Score alignment using assumptions that it may have introns. */
{
struct ffAli *left;
int oneScore;
int onePenalty;
int score;
score = ffScoreMatch(ali->nStart, ali->hStart, ali->nEnd - ali->nStart);
for (;;)
    {
    left = ali;
    if ((ali = ali->right) == NULL)
        break;
    onePenalty = gapPenalty(left, ali);
    score -= onePenalty;
    oneScore = ffScoreMatch(ali->nStart, ali->hStart, ali->nEnd - ali->nStart);
    score += oneScore;
    }
return score;
}

void clipEnds(int leftMin, int *left, int *right, int rightMax)
/* Make sure left and right are within bounds. */
{
if (*left < leftMin)
    *left = leftMin;
if (*right > rightMax)
    *right = rightMax;
}



int cmpRoughAli(const void *va, const void *vb)
/* Compare two rough alis by score. */
{
struct roughAli **pA = (struct roughAli **)va;
struct roughAli **pB = (struct roughAli **)vb;
struct roughAli *a = *pA, *b = *pB;

return b->score - a->score;
}


void sortRoughAlis(struct roughAli **pAli)
{
slSort(pAli, cmpRoughAli);
}


int cmpFineAli(const void *va, const void *vb)
/* Compare two rough alis by score. */
{
struct fineAli **pA = (struct fineAli **)va;
struct fineAli **pB = (struct fineAli **)vb;
struct fineAli *a = *pA, *b = *pB;

return b->score - a->score;
}


void sortFineAlis(struct fineAli **pAli)
{
slSort(pAli, cmpFineAli);
}


void flagOneRough(struct roughAli *list, struct roughAli *suspect)
/* Check list from beginning to suspect for things that 
 * completely overlap with suspect. */
{
struct roughAli *better;
for (better = list; better != suspect; better = better->next)
    {
    if (better->chromIx == suspect->chromIx)
        {
        if ((better->gStart <= suspect->gStart && better->gEnd >= suspect->gEnd)
         || (better->gStart >= suspect->gStart && better->gEnd <= suspect->gEnd)) 
            {
            suspect->isDupe = TRUE;
            break;
            }
        }
    }
}

void flagDupeRoughAlis(struct roughAli *raList)
/* Get rid of lower or equal scoring roughAlis that
 * cover same region. raList should be sorted. */
{
int count = slCount(raList);
struct roughAli *ra;
if (count <= 1)
    return;
for (ra = raList->next; ra != NULL; ra = ra->next)
    {
    flagOneRough(raList, ra);
    }
}

void flagOneFine(struct fineAli *list, struct fineAli *suspect)
/* Check list from beginning to suspect for things that 
 * completely overlap with suspect. */
{
struct fineAli *better;
for (better = list; better != suspect; better = better->next)
    {
    if (better->chromIx == suspect->chromIx && better->isRc == suspect->isRc)
        {
        long snStart, snEnd, shStart, shEnd, bnStart, bnEnd, bhStart, bhEnd;
        findAliEnds(suspect->blocks, suspect->virtNeedle, suspect->virtHaystack,
            &snStart, &snEnd, &shStart, &shEnd);
        findAliEnds(better->blocks, better->virtNeedle, better->virtHaystack,
            &bnStart, &bnEnd, &bhStart, &bhEnd);
        if ((bhStart <= shStart && bhEnd >= shEnd) ||
            (bhStart >= shStart && bhEnd <= shEnd))
            {
            suspect->isDupe = TRUE;
            break;
            }
        }
    }
}


void flagDupeFineAlis(struct fineAli *faList)
/* Get rid of duplicate fineAlis
 */
{
int count = slCount(faList);
struct fineAli *fa;
if (count <= 1)
    return;
for (fa = faList->next; fa != NULL; fa = fa->next)
    {
    flagOneFine(faList, fa);
    }
}


struct cr2g
    {
    struct cr2g *next;
    char strand;
    int start, end;
    char geneName[32];
    };

int findChromIx(char *name)
{
int i;
for (i=0; i<chromCount; ++i)
    {
    if (!differentWord(chromNames[i], name))
        return i;
    }
errAbort("Unknown chromosome %s", name);
return -1;
}


void loadCr2g(char *fileName, struct cr2g **chromLists)
{
char buf[512];
struct cr2g *el;
FILE *f = mustOpen(fileName, "r");
char *words[8];
int wordCount;
char *parts[3];
int partCount;
int chromIx;

uglyf("Loading %s<BR>\n", fileName);
while (fgets(buf, sizeof(buf), f) != NULL)
    {
    wordCount = chopString(buf, whiteSpaceChopper, words, ArraySize(words));
    if (wordCount != 3)
        errAbort("Bad cr2g file\n");
    partCount = chopString(words[0], ":-", parts, ArraySize(parts));
    if (partCount != 3)
        errAbort("Bad cr2g file\n");
    AllocVar(el);
    chromIx = findChromIx(parts[0]);
    el->strand = words[1][0];
    el->start = atoi(parts[1]);
    el->end = atoi(parts[2]);
    strncpy(el->geneName, words[2], sizeof(el->geneName));

    el->next = chromLists[chromIx];
    chromLists[chromIx] = el;
    }
uglyf("Done loading %s<BR>\n", fileName);
fclose(f);
}


static boolean isLoadedCtg = FALSE;
static struct cr2g *cr2gs[8];

void findClosestGene(char *chromName, int chromStart, int chromEnd, int strand,
    char **retGeneName, long *retGeneStart, long *retGeneEnd)
{
struct cr2g *cr2g;
int chromIx;
int closestDistance = 0x7fffffff;
struct cr2g *closestGene = NULL;
int distance;
int okOutsideDistance = GENE_SPACE;

if (c2gName == NULL)
    {
    char buf[64];
    sprintf(buf, "%s:%d-%d", chromName, chromStart, chromEnd);
    *retGeneName = cloneString(buf);
    *retGeneStart = chromStart;
    *retGeneEnd = chromEnd;
    return;
    }
if (!isLoadedCtg)
    {
    loadCr2g(c2gName, cr2gs);
    isLoadedCtg = TRUE;
    }
chromIx = findChromIx(chromName);
for (cr2g = cr2gs[chromIx]; cr2g != NULL; cr2g = cr2g->next)
    {
    if (chromStart > cr2g->end)
        distance = chromStart - cr2g->end;
    else if (chromEnd < cr2g->start)
        distance = cr2g->start - chromEnd;
    else
        {
        /* Overlaps */
        distance = 0;
        }
    if (distance < closestDistance)
        {
        closestGene = cr2g;
        closestDistance = distance;
        }
    }
if (closestDistance <= okOutsideDistance)
    {
    *retGeneName = closestGene->geneName;
    *retGeneStart = closestGene->start;
    *retGeneEnd = closestGene->end;
    }
else
    {
    char buf[64];
    sprintf(buf, "%s:%d-%d", chromName, chromStart, chromEnd);
    *retGeneName = cloneString(buf);
    *retGeneStart = chromStart;
    *retGeneEnd = chromEnd;
    }
}

void hyperReportAlis(struct cdnaInfo *ci)
/* Report alignments in hypertext format. */
{
struct fineAli *fa;
printf("%d <A HREF=\"../cgi-bin/getgene.exe?geneName=%s\">%s</A> ",
    ci->ix, ci->name, ci->name);
printf("%d bases %d rough alignments. Fine alignments:\n", 
    ci->baseCount, slCount(ci->roughAli));
for (fa = ci->fineAli; fa != NULL; fa = fa->next)
    {
    if (fa->isDupe)
        continue;
    printf("%s %ld-%ld hits ", ci->name, fa->nStart, fa->nEnd);
    printf("<A HREF=\"../cgi-bin/fuzzyFind.exe?cDNA=%s&gene=%s+%ld-%ld\">",
        ci->name, chromNames[fa->chromIx], fa->hStart-100, fa->hEnd+100);
    printf("chromosome %s %ld-%ld</A>", chromNames[fa->chromIx], fa->hStart, fa->hEnd);
    printf(" gene <A HREF=\"../cgi-bin/intronerator.exe?gene=%s&withCdna=On\">%s</A>", 
        fa->geneName, fa->geneName);
    printf(" score %f\n", (double)fa->score / (double)ci->baseCount);
    }
htmlHorizontalLine();
}

boolean correctIsBackwards(boolean putativeIsReversed, boolean isRc, struct ffAli *ffAli, char *cdnaName)
/* Look for introns to double check orientation */
{
int fCount = 0;
int rCount = 0;
struct ffAli *left, *right;
int nGap, hGap;
int leftSize, rightSize;
boolean isReversed;

right = ffAli;
for (;;)
    {
    left = right;
    if ((right = left->right) == NULL)
        break;
    leftSize = left->nEnd - left->nStart;
    rightSize = right->nEnd - right->nStart;
    nGap = right->nStart - left->nEnd;
    hGap = right->hStart - left->hEnd;
    if (nGap == 0 && hGap >= 16 && leftSize >= 5 && rightSize >= 5)
        {
        DNA *iStart = left->hEnd;
        DNA *iEnd = right->hStart;
        if (iStart[0] == 'g' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'g')
            ++fCount;
        else if (iStart[0] == 'c' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'c')
            ++rCount;
        }
    }
if (fCount == rCount)
    return putativeIsReversed;
if (fCount != 0 && rCount != 0)
    return putativeIsReversed;
isReversed = (rCount > fCount);
if (isRc)
    isReversed = !isReversed;
if (isReversed != putativeIsReversed)
    {
    warn("Correcting backwards EST %s", cdnaName);
    }
return isReversed;
}

void refineAlis(struct cdnaInfo *ci, struct dnaSeq *cdnaSeq)
/* Turn ci->roughAli into ci->fineAli.  Refine alignment. */
{
struct roughAli *ra;
struct fineAli *fa;
struct ffAli *ffAli = NULL;
DNA *unpacked;
int outerStart, outerEnd;
int score;
boolean isRc;
boolean leftCruddyCount;
boolean rightCruddyCount;
DNA *hayStart;
int hayLen;
DNA *hayEnd;
int gStart, gEnd;
boolean badFind;

sortRoughAlis(&ci->roughAli);
flagDupeRoughAlis(ci->roughAli);

for (ra = ci->roughAli; ra != NULL; ra = ra->next)
    {
    int bestScore = -0x7fffffff;
    int oldBestScore = -0x7fffffff;
    int oldScore;
    struct ffAli *bestAli = NULL;
    boolean bestIsRc = FALSE;

    if (ra->isDupe)
        continue;
    /* If score is less than 1/8 of cdna size, don't bother
     * with further processing. */
    if (ra->score < ci->baseCount/8)
        {
        continue;
        }

    gStart = ra->gStart - 16;
    gEnd = ra->gEnd + 16;

    /* Unpack dna, including extra at either end. */
    fetchUnpacked(ra->chromIx, ra->gStart, ra->gEnd, 15250, 
        &unpacked, &outerStart, &outerEnd);

    badFind = FALSE;    
    for (;;)
        {
        clipEnds(outerStart, &gStart, &gEnd, outerEnd);
        hayStart = unpacked + gStart-outerStart;
        hayLen = gEnd - gStart;
        hayEnd = hayStart + hayLen;
        if (!ffFindEitherStrandN(cdnaSeq->dna, cdnaSeq->size, hayStart, hayLen,
            ffCdna, &ffAli, &isRc))
            {
            if (ra->score > 20 && ra->score > oldBestScore + 5 && ra == ci->roughAli)
                {
                if (badFind)
                    {
                    warn("%s - still couldn't ffFind after expansion",
                        ci->name);
                    break;
                    }
                warn("%s Couldn't ffFind %s (%d bases %d score %d bestScore) in chromosome %s %d-%d", 
                    ci->name, ci->name, ci->baseCount, ra->score, oldBestScore, chromNames[ra->chromIx], gStart, gEnd);
                addRedoHash(ci, "ffFind");
                }
            else
                break;
            badFind = TRUE;
            gStart -= 500;
            gEnd += 500;
            continue;
            }
        if (isRc)
            reverseComplement(cdnaSeq->dna, cdnaSeq->size);
        score = scoreExonAli(ffAli);
        oldScore = ffScoreCdna(ffAli);
        leftCruddyCount = leftFlakySize(ffAli, cdnaSeq->dna, cdnaSeq->size);
        rightCruddyCount = rightFlakySize(ffAli, cdnaSeq->dna, cdnaSeq->size) -
            polyaSize(cdnaSeq->dna, cdnaSeq->size);
        if (isRc)
            reverseComplement(cdnaSeq->dna, cdnaSeq->size);
        if (score <= bestScore)
            {
            ffFreeAli(&ffAli);
            break;
            }
        bestScore = score;
        oldBestScore = oldScore;
        ffFreeAli(&bestAli);
        bestAli = ffAli;
        bestIsRc = isRc;
        if (leftCruddyCount <= 0 && rightCruddyCount <= 0)
            break;
        if (gStart == outerStart || gEnd == outerEnd)
            break;
        if (leftCruddyCount < 16)
            gStart -= 2*leftCruddyCount;
        else
            gStart -= 5000;
        if (rightCruddyCount > 0)
            {
            if (rightCruddyCount < 16)
                gEnd += 2*rightCruddyCount;
            else
                gEnd += 5000;
            }
        }
    if (bestAli != NULL)
        {
        AllocVar(fa);
        fa->chromIx = ra->chromIx;
        fa->isRc = bestIsRc;
        fa->score = bestScore;
        fa->blocks = bestAli;
        fa->virtNeedle = cdnaSeq->dna;
        fa->virtHaystack = unpacked - outerStart;
        findAliEnds(fa->blocks, fa->virtNeedle, fa->virtHaystack,
            &fa->nStart, &fa->nEnd, &fa->hStart, &fa->hEnd);
        findClosestGene(chromNames[fa->chromIx], fa->hStart, fa->hEnd,
            (fa->isRc ? '-' : '+'), &fa->geneName, &fa->geneStart, &fa->geneEnd);
        fa->isBackwards = correctIsBackwards(ci->isBackwards, fa->isRc, fa->blocks, cdnaSeq->name);
        fa->next = ci->fineAli;
        ci->fineAli = fa;
        }
    freez(&unpacked);
    }
slReverse(&ci->fineAli);
sortFineAlis(&ci->fineAli);
flagDupeFineAlis(ci->fineAli);
if (weAreWeb())
    hyperReportAlis(ci);
else
    printf("%d %s\n", ci->ix, ci->name);
slFreeList(&ci->roughAli);
}

int bestFineScore(struct fineAli *fines)
/* Return best score in a list of fineAlis. */
{
int bestScore = -0x7fffffff;
struct fineAli *fa;
for (fa = fines; fa != NULL; fa = fa->next)
    {
    if (fa->score > bestScore)
        bestScore = fa->score;
    }
return bestScore;
}

int bestRoughScore(struct roughAli *roughs)
{
int bestScore = -0x7fffffff;
struct roughAli *ra;
for (ra = roughs; ra != NULL; ra = ra->next)
    {
    if (ra->score > bestScore)
        bestScore = ra->score;
    }
return bestScore;
}

struct textLine
    {
    struct textLine *next;
    char line[1];   /* This is allocated to be bigger. */
    };

struct textLine *loadLines(char *fileName)
/* Load in each line of file into a textLine structure. */
{
char buf[512];
struct textLine *list = NULL;
struct textLine *tl;
int textSize;
FILE *f = mustOpen(fileName, "r");

while (fgets(buf, sizeof(buf), f) != NULL)
    {
    textSize = strlen(buf);
    tl = needMem(sizeof(*tl) + textSize);
    strcpy(tl->line, buf);
    tl->next = list;
    list = tl;
    }
slReverse(&list);
fclose(f);
return list;
}

void doGoodBad(struct cdnaInfo *ciList)
{
struct cdnaInfo *ci;
int goodCount = 0;
int badCount = 0;
int poorCount = 0;
struct fineAli *fa;
int bestScore;

for (ci=ciList; ci!=NULL; ci = ci->next)
    {
    if (ci->isDupe)
        continue;
    if (ci->fineAli == NULL)
        {
        reportBad("%s score %d\n", ci->name, 0);
        if (ci->roughScore > 32 && ci->roughScore >  ci->baseCount/8)
            addRedoHash(ci, "dropped");
        ++badCount;
        continue;
        }
    bestScore = ci->fineScore;
    if (bestScore < ci->baseCount/2)
        {
        fa = ci->fineAli;
        reportBad("%s score %f at chromosome %s:%ld-%ld gene %s\n", ci->name, (double)bestScore / (double)ci->baseCount,
            chromNames[fa->chromIx], fa->hStart, fa->hEnd, fa->geneName);
        addRedoHash(ci, "poor");
        ++poorCount;
        continue;
        }
    reportGood("cDNA %s size %d %s\n", 
        ci->name, ci->baseCount, (ci->isEmbryonic ? "embryo" : "unknown"));
    for(fa = ci->fineAli;fa!=NULL; fa = fa->next)
        {
        if (fa->isDupe)
            continue;
        if (fa->score >= bestScore)
            {
            char *chromName = chromNames[fa->chromIx];
            struct ffAli *ali, *lastAli = NULL;

            fa->isGood = TRUE;
            reportGood("\tscore %f chromosome %s strand %c %c gene %s\n",
                (double)fa->score / (double)ci->baseCount, chromName, 
                (fa->isRc ? '-' : '+'), (fa->isBackwards ? '-' : '+'), fa->geneName);
            for (ali=fa->blocks; ali != NULL; ali = ali->right)
                {
                reportGood("\t\t%3d %3d | %d %d size %3d goodEnds %2d %2d", 
                    ali->nStart - fa->virtNeedle, ali->nEnd - fa->virtNeedle,
                    ali->hStart - fa->virtHaystack, ali->hEnd - fa->virtHaystack,
                    ali->nEnd - ali->nStart, ali->startGood, ali->endGood);
                if (lastAli != NULL)
                    reportGood(" gap %d | %d", 
                        ali->nStart - lastAli->nEnd, ali->hStart - lastAli->hEnd);
                reportGood("\n");
                lastAli = ali;
                }
            }
        }
    ++goodCount;
    }
printf("%d Good %d Poor %d Bad\n", goodCount, poorCount, badCount);
}

void reportBackSkippers(struct cdnaInfo *ci, struct fineAli *fa)
/* Report cases where the alignment has one exon behind another. */
{
struct ffAli *ali = fa->blocks; 
struct ffAli *right = ali->right;
int skip;

while (right != NULL)
    {
    skip = right->hStart - ali->hEnd;
    if (skip < 0)
        {
        reportUnusual("Skips %s %s:%d-%d alignment skips %d\n",
           ci->name, chromNames[fa->chromIx], fa->hStart, fa->hEnd, skip);
        addRedoHash(ci, "skipsBack");
        }
    ali = right;
    right = right->right;
    }
}


void doUnusual(struct cdnaInfo *cdnaList)
/* Report unusual things. */
{
struct cdnaInfo *ci;
struct fineAli *fa;
int bestScore;
int greatCount;
int closeCount;
int decentCount;
int greatCutoff;
int closeCutoff;
int decentCutoff;
char *comment;

for (ci = cdnaList; ci != NULL; ci = ci->next)
    {
    if (ci->isDupe || ci->fineAli == NULL)
        continue;
    if (ci->baseCount < 100)
        reportUnusual("%s is short - %d bases\n", ci->name, ci->baseCount);

    bestScore = ci->fineScore;
    greatCutoff = ci->baseCount * 9 / 10;
    closeCutoff = 9*bestScore/10;
    decentCutoff = ci->baseCount/2;
    
    greatCount = closeCount = decentCount = 0;
    comment = NULL;

    for (fa = ci->fineAli; fa != NULL; fa = fa->next)
        {
        if (fa->isDupe)
            continue;
        if (fa->isGood)
            {
            ++decentCount;
            reportBackSkippers(ci, fa);
            }
        if (fa->score >= closeCutoff)
            ++closeCount;
        if (fa->score >= greatCutoff)
            ++greatCount;
        }
    if (greatCount > 1)
        comment = "great";
    else if (closeCount > 1)
        comment = "similar";
    else if (decentCount > 1)
        comment = "decent";

    if (comment != NULL)
        {
        reportUnusual("Multiple %s hits for %s\n", comment, ci->name);
        for (fa = ci->fineAli; fa != NULL; fa = fa->next)
            {
            if (fa->isDupe)
                continue;
            if (fa->score >= decentCutoff || fa->score >= closeCutoff)
                {
                long hStart, hEnd, nStart, nEnd;
                findAliEnds(fa->blocks, fa->virtNeedle, fa->virtHaystack,
                   &nStart, &nEnd, &hStart, &hEnd);

                reportUnusual("\t%s:%d-%d score %f\n", 
                    chromNames[fa->chromIx], hStart, hEnd, 
                    (double)fa->score/ci->baseCount);
                }
            }
        }
    }
}

struct geneHit
    {
    struct geneHit *next;
    char *cdnaName;
    int chromOffset;
    };

struct geneHitList
    {
    struct geneHitList *next;
    char *geneName;
    struct geneHit *hits;
    };

int cmpGhlName(const void *va, const void *vb)
/* Compare two geneHitLists by name. */
{
struct geneHitList **pA = (struct geneHitList **)va;
struct geneHitList **pB = (struct geneHitList **)vb;
struct geneHitList *a = *pA, *b = *pB;

return strcmp(a->geneName, b->geneName);
}

int cmpGhOffset(const void *va, const void *vb)
/* Compare two geneHits by chromosome offset. */
{
struct geneHit **pA = (struct geneHit **)va;
struct geneHit **pB = (struct geneHit **)vb;
struct geneHit *a = *pA, *b = *pB;

return a->chromOffset - b->chromOffset;
}


#ifdef OLD
void makeCdnaToGene(struct cdnaInfo *cdnaList)
/* Make cdna to gene translation file. */
{
struct hash *hash = newHash(12);
struct cdnaInfo *ci;
struct fineAli *fa;
struct geneHit *gh;
struct geneHitList *geneHitList = NULL;
struct geneHitList *ghl;
struct hashEl *he;

uglyf("Making cdnaToGene file<BR>\n");

for (ci = cdnaList; ci != NULL; ci = ci->next)
    {
    if (ci->isDupe)
        continue;
    for (fa = ci->fineAli; fa != NULL; fa = fa->next)
        {
        if (fa->isDupe || !fa->isGood) 
            continue;
        if ((he = hashLookup(hash, fa->geneName)) == NULL)
            {
            AllocVar(ghl);
            ghl->geneName = fa->geneName;
            ghl->next = geneHitList;
            geneHitList = ghl;
            he = hashAdd(hash, fa->geneName, ghl);
            }
        ghl = (struct geneHitList *)(he->val);
        AllocVar(gh);
        gh->cdnaName = ci->name;
        gh->chromOffset = fa->hStart;
        gh->next = ghl->hits;
        ghl->hits = gh;
        }
    }
slSort(&geneHitList, cmpGhlName);

for (ghl=geneHitList; ghl!=NULL; ghl = ghl->next)
    {
    slReverse(&ghl->hits);
    slSort(&ghl->hits, cmpGhOffset);
    fprintf(cdnaToGeneFile, "%s ", ghl->geneName);
    for (gh = ghl->hits; gh != NULL; gh = gh->next)
        fprintf(cdnaToGeneFile, "%s ", gh->cdnaName);
    fprintf(cdnaToGeneFile, "\n");
    }

freeHash(&hash);
slFreeList(&geneHitList);
uglyf("Done making cdnaToGene file<BR>\n");
}
#endif /* OLD */



void hitLine(struct cdnaInfo *ci, int lineCount, char *cdnaName, char *cdnaRange,
    char *mitoChrom, char *chromName, char *genomeRange, char *scoreString)
/* Process line with hit info. */
{
char *s;
struct roughAli *ra;

if (ci == NULL || differentWord(cdnaName, ci->name))
    {
    errAbort("Got hit line %d without corresponding blasting for cDNA %s",
        lineCount, cdnaName);
    }
/* Allocate structure to hold data for line. */
ra = needMem(sizeof(*ra));
/* Grab range of bases covered in cDNA */
if (cdnaRange != NULL)
    {
    s = cdnaRange;
    ra->cStart = atoi(s);
    s = strchr(s, '-')+1;
    ra->cEnd = atoi(s)+1;
    if (ra->cStart >= ra->cEnd)
        {
        int temp;
        warn("%s line %d cdna %s: cStart %d > cEnd %d. Swapping.",
            cdnaName, lineCount, cdnaName, ra->cStart, ra->cEnd);
        addRedoHash(ci, "swappedCdna");
        temp = ra->cStart;
        ra->cStart = ra->cEnd;
        ra->cEnd = ra->cStart;
        }
    }
/* Grab range of bases covered in genome. */
s = genomeRange;
ra->gStart = atoi(s);
s = strchr(s, '-')+1;
ra->gEnd = atoi(s)+1;
if (ra->gStart >= ra->gEnd)
    {
    int temp;
    warn("%s line %d cdna %s: gStart %d > gEnd %d. Swapping.",
        cdnaName, lineCount, cdnaName, ra->gStart, ra->gEnd);
    addRedoHash(ci, "swappedGenome");
    temp = ra->gStart;
    ra->gStart = ra->gEnd;
    ra->gEnd = ra->gStart;
    }
/* Figure out chromosome it's on. */
if (!differentWord(mitoChrom, "Mitochondrial"))
    ra->chromIx = 6;
else
    {
    int i;
    for (i=0; i<chromCount; ++i)
        {
        if (!differentWord(chromName, chromNames[i]))
            {
            ra->chromIx = i;
            break;
            }
        }
    }
/* Get alignment score. */
ra->score = atoi(scoreString);
/* Hang it on list. */
ra->next = ci->roughAli;
ci->roughAli = ra;
}

void analyse(int start, int stop)
{
struct hash *hash;
char line[512];
int lineCount = 0;
char *words[32];
int wordCount;
struct cdnaInfo *cdnaList = NULL;
struct cdnaInfo *ci = NULL;
int cdnaCount;
int maxCdnaCount = stop - start;

cdnaCount = 1;
if (start > 1)
    {
    for (;;)
        {
        if (!fgets(line, sizeof(line), inFile))
            errAbort("Not %d cDNAs in file, only %d\n", start, cdnaCount);
        ++lineCount;
        if (line[0] == '#') /* Skip comments. */
            continue;
        wordCount = chopString(line, whiteSpaceChopper, words, ArraySize(words));
        if (wordCount <= 0) /* Skip empty lines. */
            continue;
        if (!differentWord(words[1], "alignments"))
            {
            ++cdnaCount;
            if (cdnaCount >= start)
                break;
            }
        }
    }
cdnaCount = 0;
hash = newHash(14); /* Hash table with 16k entries. */
for (;;)
    {
    if (!fgets(line, sizeof(line), inFile))
        break;
    ++lineCount;
    if (line[0] == '#') /* Skip comments. */
        continue;
    wordCount = chopString(line, whiteSpaceChopper, words, ArraySize(words));
    if (wordCount <= 0) /* Skip empty lines. */
        continue;
    if (wordCount < 4)  /* Everyone else has at least four words. */
        {
        errAbort("Short line %d:\n", lineCount);
        }
    if (sameWord(words[1], "Blasting"))
        {
        char *cdnaName = words[2];
        if ((ci = lookupInfo(hash, cdnaName)) == NULL)
            {
            struct hashEl *hel;
            ci = needMem(sizeof(*ci));
            hel = hashAdd(hash, cdnaName, ci);
            ci->next = cdnaList;
            cdnaList = ci;
            ci->ix = atoi(words[0]);
            ci->name = hel->name;
            }
        }
    else if (sameWord(words[2], "hits"))
        {
        /* Newer style - includes cDNA matching range. */
        if (ci == NULL)
            continue;
        hitLine(ci, lineCount, words[0], words[1], words[3], words[4], words[5], words[9]);
        }
    else if (sameWord(words[1], "hits"))
        /* Older style - no cDNA matching range. */
        {
        if (ci == NULL)
            continue;
        hitLine(ci, lineCount, words[0],     NULL, words[2], words[3], words[4], words[8]);
        }
   else if (sameWord(words[1], "alignments"))
        {
        struct dnaSeq *cdnaSeq;
        struct wormCdnaInfo info;
        if (ci == NULL)
            continue;
        if (differentWord(ci->name, words[3]))
            errAbort("Line %d - %s is not %s", lineCount, words[3], ci->name);
        if (!ci->finished)
            {
            if (!anyCdnaSeq(ci->name, &cdnaSeq, &info))
                {
                warn("Can't find cDNA %s", ci->name);
                ci->isDupe = TRUE;
                }
            else
                {
                ci->baseCount = cdnaSeq->size;
                ci->baseCrc = dnaCrc(cdnaSeq->dna, cdnaSeq->size);
                slReverse(&ci->roughAli);
                ci->roughScore = bestRoughScore(ci->roughAli);
                filterDupeCdna(ci, cdnaSeq);
                ci->isBackwards = (info.orientation == '-');
                refineAlis(ci, cdnaSeq);
                ci->fineScore = bestFineScore(ci->fineAli);
                ci->isEmbryonic = info.isEmbryonic;  
                ci->finished = TRUE;  
                freeDnaSeq(&cdnaSeq);
                ++cdnaCount;
                if (cdnaCount >= maxCdnaCount)
                    break;
                }
            }
        }
    else
        {
        errAbort("Can't deal with line %d\n", lineCount);
        }
    }

slReverse(&cdnaList);

doGoodBad(cdnaList);
doUnusual(cdnaList);
//makeCdnaToGene(cdnaList);

/* Clean up. */

/* These two are slow and not really necessary. */
#ifdef FASTIDIOUS
slFreeList(&cdnaList);
freeHash(&hash);
#endif

uglyf("Done analyse\n");
}

int test(int argc, char *argv[])
{
int start, stop;

if (argc != 9 && argc != 10)
    usageErr();
inName = argv[0];
cdnaName = argv[1];
chromDir = argv[2];
goodLogName = argv[3];
badLogName = argv[4];
unusualName = argv[5];
errName = argv[6];
start = atoi(argv[7]);
stop = atoi(argv[8]);
if (start >= stop)
    usageErr();
if (argc == 10)
    c2gName = argv[9];

inFile = mustOpen(inName, "r");
goodLogFile = mustOpen(goodLogName, "w");
badLogFile = mustOpen(badLogName, "w");
unusualFile = mustOpen(unusualName, "w");
errFile = mustOpen(errName, "w");
pushWarnHandler(reportWarning);

dnaUtilOpen();

printf("Loading chromosomes\n");
loadGenome(chromDir, &chroms, &chromNames, &chromCount);
startRedoHash();

printf("Analysing %s\n", inName);
if (weAreWeb())
    htmlHorizontalLine();

analyse(start, stop);

//endRedoHash();
freeGenome(&chroms, &chromNames, chromCount);
popWarnHandler();
fclose(inFile);
fclose(goodLogFile);
fclose(badLogFile);
fclose(unusualFile);
fclose(errFile);
return 0;
}

void doMiddle()
{
char *commandLine = cgiString("commandLine");
char *words[50];
int wordCount;
wordCount = chopString(commandLine, whiteSpaceChopper, words, ArraySize(words));
printf("<PRE>");
test(wordCount, words);
printf("</PRE>");
}

int main(int argc, char *argv[])
{
if (weAreWeb())
    htmShell("refineAli Output", doMiddle, "QUERY");
else
    test(argc-1, argv+1);
return 0;
}

