/* patSpace - cDNA alignment algorithm that occurs mostly in 
 * pattern space (as opposed to offset space).  This particular
 * implementation tries to stitch together small contigs in
 * unfinished Bacterial Artificial Chromosomes (BACs) by looking
 * for spanning cDNAs. A BAC is a roughly 150,000 base sequence
 * of genomic DNA.  An unfinished BAC consists of multiple
 * unordered non-overlapping subsequences. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "portable.h"
#include "errabort.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nt4.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "cda.h"
#include "sig.h"
#include "patSpace.h"
#include "htmshell.h"
FILE *bigHtmlFile;
FILE *littleHtmlFile;
int htmlIx;

FILE *hitOut;    /* Output file for cDNA/BAC hits. */
FILE *mergerOut; /* Output file for mergers. */
FILE *dumpOut;   /* Debugging output. */

boolean dumpMe = FALSE;     /* Set on if dumping extra info. */

/* Stuff to help print meaningful error messages when on a
 * compute cluster. */
char *hostName = "";
char *aliSeqName = "";


void patSpaceWarnHandler(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) {
    fprintf(stderr, "\n[error host %s accession %s] ", hostName, aliSeqName);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    }
}


void ffFindEnds(struct ffAli *ff, struct ffAli **retLeft, struct ffAli **retRight)
/* Find left and right ends of ffAli. */
{
while (ff->left != NULL)
    ff = ff->left;
*retLeft = ff;
while (ff->right != NULL)
    ff = ff->right;
*retRight = ff;
}



int scoreCdna(struct ffAli *left, struct ffAli *right)
/* Score ali using cDNA scoring. */
{
int size, nGap, hGap;
struct ffAli *ff, *next;
int score = 0, oneScore;


for (ff=left; ;)
    {
    next = ff->right;

    size = ff->nEnd - ff->nStart;
    oneScore = ffScoreMatch(ff->nStart, ff->hStart, size);
    
    if (next != NULL && ff != right)
        {
        /* Penalize all gaps except for intron-looking ones. */
        nGap = next->nStart - ff->nEnd;
        hGap = next->hStart - ff->hEnd;

        /* Process what are stretches of mismatches rather than gaps. */
        if (nGap > 0 && hGap > 0)
            {
            if (nGap > hGap)
                {
                nGap -= hGap;
                oneScore -= hGap;
                hGap = 0;
                }
            else
                {
                hGap -= nGap;
                oneScore -= nGap;
                nGap = 0;
                }
            }
   
        if (nGap < 0)
            nGap = -nGap-nGap;
        if (hGap < 0)
            hGap = -hGap-hGap;
        if (nGap > 0)
            oneScore -= 8*nGap;
        if (hGap > 30)
            oneScore -= 1;
        else if (hGap > 0)
            oneScore -= 8*hGap;
        }
    score += oneScore;
    if (ff == right)
        break;
    ff = next;
    } 
return score;
}

boolean solidMatch(struct ffAli **pLeft, struct ffAli **pRight, DNA *needle, 
    int *retStartN, int *retEndN)
/* Return start and end (in needle coordinates) of solid parts of 
 * match if any. Necessary because fuzzyFinder algorithm will extend
 * ends a little bit beyond where they're really solid.  We want
 * to effectively save these bases for aligning somewhere else. */
{
int minSegSize = 11;
struct ffAli *next;
int segSize;
int runTotal = 0;
int gapSize;
struct ffAli *left = *pLeft, *right = *pRight;

/* Get rid of small segments on left end that are separated from main body. */
for (;;)
    {
    if (left == NULL)
        return FALSE;
    next = left->right;
    segSize = left->nEnd - left->nStart;
    runTotal += segSize;
    if (segSize > minSegSize || runTotal > minSegSize*2)
        break;
    if (next != NULL)
        {
        gapSize = next->nStart - left->nEnd;
        if (gapSize > 1)
            runTotal = 0;
        }
    left = next;
    }
*retStartN = left->nStart - needle;

/* Do same thing on right end... */
runTotal = 0;
for (;;)
    {
    if (right == NULL)
        return FALSE;
    next = right->left;
    segSize = right->nEnd - right->nStart;
    runTotal += segSize;
    if (segSize > minSegSize || runTotal > minSegSize*2)
        break;
    if (next != NULL)
        {
        gapSize = next->nStart - left->nEnd;
        if (gapSize > 1)
            runTotal = 0;
        }
    right = next;
    }
*retEndN = right->nEnd - needle;

*pLeft = left;
*pRight = right;
return *retEndN - *retStartN >= minSegSize;
}

struct cdnaAliList
/* This structure keeps a list of where a particular cDNA aligns.
 * The list isn't kept long, just while that cDNA is in memory. */
    {
    struct cdnaAliList *next;   /* Link to next element on list. */
    int bacIx;                  /* Which BAC. */
    int seqIx;                  /* Which sequence in BAC. */
    int start, end;             /* Position within cDNA sequence. */
    char strand;                /* Was cdna reverse complemented? + if no, - if yes. */
    char dir;                   /* 5 or 3. */
    double cookedScore;         /* Score - roughly as % of bases aligning. */
    };

int cmpCal(const void *va, const void *vb)
/* Compare two cdnaAliLists. */
{
const struct cdnaAliList *a = *((struct cdnaAliList **)va);
const struct cdnaAliList *b = *((struct cdnaAliList **)vb);
int dif;
dif = a->bacIx - b->bacIx;
if (dif == 0)
    dif = b->dir - a->dir;
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

struct cdnaAliList *calFreeList = NULL; /* List of reusable cdnaAliList elements. */

struct cdnaAliList *newCal(int bacIx, int seqIx, int start, int end, int size, char strand,
    char dir, double cookedScore)
/* Get a new element for this cdnaList, off of free list if possible. */
{
struct cdnaAliList *cal;
if ((cal = calFreeList) == NULL)
    {
    AllocVar(cal);
    }
else
    {
    calFreeList = cal->next;
    }
cal->next = NULL;
cal->bacIx = bacIx;
cal->seqIx = seqIx;;
cal->strand = strand;
cal->dir = dir;
cal->cookedScore = cookedScore;
if (strand == '-')
    {
    cal->start = size - end;
    cal->end = size - start;
    }
else
    {
    cal->start = start;
    cal->end = end;
    }
return cal;
}

void freeCal(struct cdnaAliList *cal)
/* Free up one cal. */
{
slAddHead(&calFreeList, cal);
}

void freeCalList(struct cdnaAliList **pList)
/* Free up cal list for reuse. */
{
struct cdnaAliList *cal, *next;

for (cal = *pList; cal != NULL; cal = next)
    {
    next = cal->next;
    freeCal(cal);
    }
*pList = NULL;
}

int ffSubmitted = 0;
int ffAccepted = 0;
int ffOkScore = 0;
int ffSolidMatch = 0;

void writeClump(struct blockPos *first, struct blockPos *last,
    char *cdnaName, char strand, char dir, DNA *cdna, int cdnaSize, struct cdnaAliList **pList)
/* Write hitOut one clump. */
{
struct dnaSeq *seq = first->seq;
char *bacName = seq->name;
int seqIx = first->seqIx;
int start = first->offset;
int end = last->offset+last->size;
struct ffAli *ff, *left, *right;
int extraAtEnds = minMatch*patSize;
struct cdnaAliList *cal;

start -= extraAtEnds;
if (start < 0)
    start = 0;
end += extraAtEnds;
if (end >seq->size)
    end = seq->size;

++ffSubmitted;
if (dumpMe)
	fprintf(dumpOut, "%s %d %s %d-%d\n", cdnaName, cdnaSize, bacName, start, end);
ff = ffFind(cdna, cdna+cdnaSize, seq->dna+start, seq->dna+end, ffCdna);
if (dumpMe)
    {
    fprintf(dumpOut, "ffFind = %x\n", ff);
    }
if (ff != NULL)
    {
    int ffScore = ffScoreCdna(ff);
    ++ffAccepted;
    if (dumpMe) fprintf(dumpOut, "ffScore = %d\n", ffScore);
    if (ffScore >= 22)
        {
        int hiStart, hiEnd;
        int oldStart, oldEnd;

        ffFindEnds(ff, &left, &right);
        hiStart = oldStart = left->nStart - cdna;
        hiEnd = oldEnd = right->nEnd - cdna;
        ++ffOkScore;

        if (solidMatch(&left, &right, cdna, &hiStart, &hiEnd))
            {
            int solidSize = hiEnd - hiStart;
            int solidScore;
            int seqStart, seqEnd;
            double cookedScore;

            solidScore = scoreCdna(left, right);
            cookedScore = (double)solidScore/solidSize;
            if (cookedScore > 0.25)
                {
                ++ffSolidMatch;

                seqStart = left->hStart - seq->dna;
                seqEnd = right->hEnd - seq->dna;
                fprintf(hitOut, "%3.1f%% %c %s:%d-%d (old %d-%d) of %d at %s.%d:%d-%d\n", 
                    100.0 * cookedScore, strand, cdnaName, 
                    hiStart, hiEnd, oldStart, oldEnd, cdnaSize,
                    bacName, seqIx, seqStart, seqEnd);

                if (dumpMe)
                    {
                    fprintf(bigHtmlFile, "<A NAME=i%d>", htmlIx);
                    fprintf(bigHtmlFile, "<H2>%4.1f%% %4d %4d %c %s:%d-%d of %d at %s.%d:%d-%d</H2><BR>", 
                        100.0 * cookedScore, solidScore, ffScore, strand, cdnaName, 
                        hiStart, hiEnd, cdnaSize,
                        bacName, seqIx, seqStart, seqEnd);
                    fprintf(bigHtmlFile, "</A>");
                    ffShAli(bigHtmlFile, ff, cdnaName, cdna, cdnaSize, 0,
                        bacName, seq->dna+start, end-start, start, FALSE);
                    fprintf(bigHtmlFile, "<BR><BR>\n");

                    fprintf(littleHtmlFile, "<A HREF=\"patAli.html#i%d\">", htmlIx);
                    fprintf(littleHtmlFile, "%4.1f%% %4d %4d %c %s:%d-%d of %d at %s.%d:%d-%d\n", 
                        100.0 * cookedScore, solidScore, ffScore, strand, cdnaName, 
                        hiStart, hiEnd, cdnaSize,
                        bacName, seqIx, seqStart, seqEnd);
                    fprintf(littleHtmlFile, "</A><BR>");
                    ++htmlIx;
                    }

                cal = newCal(first->bacIx, seqIx, hiStart, hiEnd, cdnaSize, strand, dir, cookedScore);
                slAddHead(pList, cal);
                }
            }
        }
    ffFreeAli(&ff);
    }
}

boolean noMajorOverlap(struct cdnaAliList *cal, struct cdnaAliList *clump)
/* Return TRUE if cal doesn't overlap in a big way with what's already in clump. */
{
int calStart = cal->start;
int calEnd = cal->end;
int calSize = calEnd - calStart;
int maxOverlap = calSize/3;
char dir = cal->dir;
int s, e, o;

for (; clump != NULL; clump = clump->next)
    {
    if (clump->dir == dir)
        {
        s = max(calStart, clump->start);
        e = min(calEnd, clump->end);
        o = e - s;
        if (o > maxOverlap)
            return FALSE;
        }
    }
return TRUE;
}

boolean allSameContig(struct cdnaAliList *calList)
/* Return TRUE if all members of list are part of same contig. */
{
struct cdnaAliList *cal;
int bacIx, seqIx;

if (calList == NULL)
    return TRUE;
bacIx = calList->bacIx;
seqIx = calList->seqIx;
for (cal = calList->next; cal != NULL; cal = cal->next)
    {
    assert(cal->bacIx == bacIx);
    if (cal->seqIx != seqIx)
        return FALSE;
    }
return TRUE;
}

void writeMergers(struct cdnaAliList *calList, char *cdnaName, char *bacNames[])
/* Write out any mergers indicated by this cdna. This destroys calList. */
{
struct cdnaAliList *startBac, *endBac, *cal, *prevCal, *nextCal;
int bacCount;
int bacIx;
    
slSort(&calList, cmpCal);
for (startBac = calList; startBac != NULL; startBac = endBac)
    {
    /* Scan until find a cal that isn't pointing into the same BAC. */
    bacCount = 1;
    bacIx = startBac->bacIx;
    prevCal = startBac;
    for (cal =  startBac->next; cal != NULL; cal = cal->next)
        {
        if (cal->bacIx != bacIx)
            {
            prevCal->next = NULL;
            break;
            }
        ++bacCount;
        prevCal = cal;
        }
    endBac = cal;
    if (bacCount > 1)
        {
        while (startBac != NULL)
            {
            struct cdnaAliList *clumpList = NULL, *leftoverList = NULL;
            for (cal = startBac; cal != NULL; cal = nextCal)
                {
                nextCal = cal->next;
                if (noMajorOverlap(cal, clumpList))
                    {
                    slAddHead(&clumpList, cal);
                    }
                else
                    {
                    slAddHead(&leftoverList, cal);
                    }
                }
            slReverse(&clumpList);
            slReverse(&leftoverList);
            if (slCount(clumpList) > 1)
                {
                char lastStrand = 0;
                boolean switchedStrand = FALSE;
                if (!allSameContig(clumpList))
                    {
                    fprintf(mergerOut, "%s glues %s contigs", cdnaName, bacNames[bacIx]);
                    lastStrand = clumpList->strand;
                    for (cal = clumpList; cal != NULL; cal = cal->next)
                        {
                        if (cal->strand != lastStrand)
                            switchedStrand = TRUE;
                        fprintf(mergerOut, " %d %c %c' (%d-%d) %3.1f%%", cal->seqIx, cal->strand, 
                            cal->dir,
                            cal->start, cal->end, 100.0*cal->cookedScore);
                        }
                    fprintf(mergerOut, "\n");
                    }
                }
            freeCalList(&clumpList);
            startBac = leftoverList;
            }        
        }
    else
        {
        freeCalList(&startBac);
        }
    }
}


void usage()
{
errAbort("patSpace - tell where a cDNA is located quickly.\n"
         "usage:\n"
         "    patSpace genomeListFile cdnaListFile ignore.ooc 5and3.pai outRoot\n"
         "The program will create the files outRoot.hit outRoot.glu outRoot.ok\n"
         "which contain the cDNA hits, gluing cDNAs, and a sign that the program\n"
         "ended ok respectively.");
}


boolean fastFaReadNext(FILE *f, DNA **retDna, int *retSize, char **retName)
/* Read in next FA entry as fast as we can. Return FALSE at EOF. */
{
static int bufSize = 0;
static DNA *buf;
int c;
int bufIx = 0;
static char name[256];
int nameIx = 0;
boolean gotSpace = FALSE;

/* Seek to next '\n' and save first word as name. */
name[0] = 0;
for (;;)
    {
    if ((c = fgetc(f)) == EOF)
        {
        *retDna = NULL;
        *retSize = 0;
        *retName = NULL;
        return FALSE;
        }
    if (!gotSpace && nameIx < ArraySize(name)-1)
        {
        if (isspace(c))
            gotSpace = TRUE;
        else
            {
            name[nameIx++] = c;
            }
        }
    if (c == '\n')
        break;
    }
name[nameIx] = 0;
/* Read until next '>' */
for (;;)
    {
    c = fgetc(f);
    if (isspace(c) || isdigit(c))
        continue;
    if (c == EOF || c == '>')
        c = 0;
    else
        c = tolower(c);
    if (bufIx >= bufSize)
        {
        if (bufSize == 0)
            {
            bufSize = 64 * 1024;
            buf = needMem(bufSize);
            }
        else
            {
            DNA *newBuf;
            int newBufSize = bufSize + bufSize;
            newBuf = needMem(newBufSize);
            memcpy(newBuf, buf, bufIx);
            freeMem(buf);
            buf = newBuf;
            bufSize = newBufSize;
            }
        }
    buf[bufIx++] = c;
    if (c == 0)
        {
        *retDna = buf;
        *retSize = bufIx-1;
        *retName = name;
        return TRUE;
        }
    }
}

struct seq
/* Little structure that holds DNA sequence and size. */
    {
    DNA *dna;
    int size;
    };

struct estPair
/* This struct manages a 3' and 5' pair of ests.  The main loop will check
 * to see if an est is part of a pair.  If so it will store the sequence
 * rather than process it immediately.  When the second part of a pair is
 * found both are processed. */
    {
    struct estPair *next;
    char *name5;            /* Name of 5' part of pair. */
    char *name3;            /* Name of 3' part of pair. */
    struct seq seq5;   /* Sequence of 5' pair - kept loaded until get 3' seq too. */
    struct seq seq3;   /* Sequence of 3' pair - kept loaded until get 5' seq too. */
    };
struct estPair *estPairList = NULL;

struct hash *makePairHash(char *pairFile)
/* Make up a hash table out of paired ESTs. */
{
FILE *f = mustOpen(pairFile, "r");
char line[256];
char *words[3];
int wordCount;
int lineCount = 0;
struct hash *hash;
struct hashEl *h5, *h3;
struct estPair *ep;
char *name5, *name3;

hash = newHash(19);
while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount != 2)
        errAbort("%d words in pair line %d of %s", wordCount, lineCount, pairFile);
    name5 = words[0];
    name3 = words[1];
    AllocVar(ep);
    h5 = hashAdd(hash, name5, ep);
    h3 = hashAdd(hash, name3, ep);
    ep->name5 = h5->name;
    ep->name3 = h3->name;
    slAddHead(&estPairList, ep);
    }
printf("Read %d lines of pair info\n", lineCount);
return hash;
}

void writeClump(struct blockPos *first, struct blockPos *last,
    char *cdnaName, char strand, char dir, DNA *cdna, int cdnaSize, struct cdnaAliList **pList)
/* Write hitOut one clump. */
{
struct dnaSeq *seq = first->seq;
char *bacName = seq->name;
int seqIx = first->seqIx;
int start = first->offset;
int end = last->offset+last->size;
struct ffAli *ff, *left, *right;
int extraAtEnds = minMatch*patSize;
struct cdnaAliList *cal;

start -= extraAtEnds;
if (start < 0)
    start = 0;
end += extraAtEnds;
if (end >seq->size)
    end = seq->size;

++ffSubmitted;
if (dumpMe)
	fprintf(dumpOut, "%s %d %s %d-%d\n", cdnaName, cdnaSize, bacName, start, end);
ff = ffFind(cdna, cdna+cdnaSize, seq->dna+start, seq->dna+end, ffCdna);
if (dumpMe)
    {
    fprintf(dumpOut, "ffFind = %x\n", ff);
    }
if (ff != NULL)
    {
    int ffScore = ffScoreCdna(ff);
    ++ffAccepted;
    if (dumpMe) fprintf(dumpOut, "ffScore = %d\n", ffScore);
    if (ffScore >= 22)
        {
        int hiStart, hiEnd;
        int oldStart, oldEnd;

        ffFindEnds(ff, &left, &right);
        hiStart = oldStart = left->nStart - cdna;
        hiEnd = oldEnd = right->nEnd - cdna;
        ++ffOkScore;

        if (solidMatch(&left, &right, cdna, &hiStart, &hiEnd))
            {
            int solidSize = hiEnd - hiStart;
            int solidScore;
            int seqStart, seqEnd;
            double cookedScore;

            solidScore = scoreCdna(left, right);
            cookedScore = (double)solidScore/solidSize;
            if (cookedScore > 0.25)
                {
                ++ffSolidMatch;

                seqStart = left->hStart - seq->dna;
                seqEnd = right->hEnd - seq->dna;
                fprintf(hitOut, "%3.1f%% %c %s:%d-%d (old %d-%d) of %d at %s.%d:%d-%d\n", 
                    100.0 * cookedScore, strand, cdnaName, 
                    hiStart, hiEnd, oldStart, oldEnd, cdnaSize,
                    bacName, seqIx, seqStart, seqEnd);

                if (dumpMe)
                    {
                    fprintf(bigHtmlFile, "<A NAME=i%d>", htmlIx);
                    fprintf(bigHtmlFile, "<H2>%4.1f%% %4d %4d %c %s:%d-%d of %d at %s.%d:%d-%d</H2><BR>", 
                        100.0 * cookedScore, solidScore, ffScore, strand, cdnaName, 
                        hiStart, hiEnd, cdnaSize,
                        bacName, seqIx, seqStart, seqEnd);
                    fprintf(bigHtmlFile, "</A>");
                    ffShAli(bigHtmlFile, ff, cdnaName, cdna, cdnaSize, 0,
                        bacName, seq->dna+start, end-start, start, FALSE);
                    fprintf(bigHtmlFile, "<BR><BR>\n");

                    fprintf(littleHtmlFile, "<A HREF=\"patAli.html#i%d\">", htmlIx);
                    fprintf(littleHtmlFile, "%4.1f%% %4d %4d %c %s:%d-%d of %d at %s.%d:%d-%d\n", 
                        100.0 * cookedScore, solidScore, ffScore, strand, cdnaName, 
                        hiStart, hiEnd, cdnaSize,
                        bacName, seqIx, seqStart, seqEnd);
                    fprintf(littleHtmlFile, "</A><BR>");
                    ++htmlIx;
                    }

                cal = newCal(first->bacIx, seqIx, hiStart, hiEnd, cdnaSize, strand, dir, cookedScore);
                slAddHead(pList, cal);
                }
            }
        }
    ffFreeAli(&ff);
    }
}


void glueFindOne(struct patSpace *ps, DNA *dna, int dnaSize, 
    char strand, char dir, char *estName, struct cdnaAliList **pList)
/* Find occurrences of DNA in patSpace and print to hitOut. */
{
struct patClump *clumpList, *clump;

clumpList = patSpaceFindOne(ps, dna, dnaSize);
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct ffAli *ali, *a;
    boolean isRc;
    int score;
    struct dnaSeq *t = clump->seq;
    DNA *tStart = t->dna + clump->start;

    ++ffSubmitted;
    ali = ffFind(est->dna, est->dna+est->size, tStart, tStart + clump->size, ffCdna);
    if (ali != NULL)
        {
        ++ffAccepted;
        if (ffScore >= 22)
            {
            int hiStart, hiEnd;
            int oldStart, oldEnd;

            ffFindEnds(ff, &left, &right);
            hiStart = oldStart = left->nStart - cdna;
            hiEnd = oldEnd = right->nEnd - cdna;
            ++ffOkScore;

            if (solidMatch(&left, &right, cdna, &hiStart, &hiEnd))
                {
                int solidSize = hiEnd - hiStart;
                int solidScore;
                int seqStart, seqEnd;
                double cookedScore;

                solidScore = scoreCdna(left, right);
                cookedScore = (double)solidScore/solidSize;
                if (cookedScore > 0.25)
                    {
                    ++ffSolidMatch;

                    seqStart = left->hStart - seq->dna;
                    seqEnd = right->hEnd - seq->dna;
                    fprintf(hitOut, "%3.1f%% %c %s:%d-%d (old %d-%d) of %d at %s.%d:%d-%d\n", 
                        100.0 * cookedScore, strand, cdnaName, 
                        hiStart, hiEnd, oldStart, oldEnd, cdnaSize,
                        bacName, seqIx, seqStart, seqEnd);

                    if (dumpMe)
                        {
                        fprintf(bigHtmlFile, "<A NAME=i%d>", htmlIx);
                        fprintf(bigHtmlFile, "<H2>%4.1f%% %4d %4d %c %s:%d-%d of %d at %s.%d:%d-%d</H2><BR>", 
                            100.0 * cookedScore, solidScore, ffScore, strand, cdnaName, 
                            hiStart, hiEnd, cdnaSize,
                            bacName, seqIx, seqStart, seqEnd);
                        fprintf(bigHtmlFile, "</A>");
                        ffShAli(bigHtmlFile, ff, cdnaName, cdna, cdnaSize, 0,
                            bacName, seq->dna+start, end-start, start, FALSE);
                        fprintf(bigHtmlFile, "<BR><BR>\n");

                        fprintf(littleHtmlFile, "<A HREF=\"patAli.html#i%d\">", htmlIx);
                        fprintf(littleHtmlFile, "%4.1f%% %4d %4d %c %s:%d-%d of %d at %s.%d:%d-%d\n", 
                            100.0 * cookedScore, solidScore, ffScore, strand, cdnaName, 
                            hiStart, hiEnd, cdnaSize,
                            bacName, seqIx, seqStart, seqEnd);
                        fprintf(littleHtmlFile, "</A><BR>");
                        ++htmlIx;
                        }

                    cal = newCal(first->bacIx, seqIx, hiStart, hiEnd, cdnaSize, strand, dir, cookedScore);
                    slAddHead(pList, cal);
                    }
                }
            }
        ffFreeAli(&ali);
        }
    }
slFreeList(&clumpList);
}


int main(int argc, char *argv[])
{
char *genoListName;
char *cdnaListName;
char *oocFileName;
char *pairFileName;
struct patSpace *patSpace;
long startTime, endTime;
char **genoList;
int genoListSize;
char *genoListBuf;
char **cdnaList;
int cdnaListSize;
char *cdnaListBuf;
char *genoName;
int i;
int estIx = 0;
struct dnaSeq **seqListList = NULL, *seq;
static char hitFileName[512], mergerFileName[512], okFileName[512];
char *outRoot;
struct hash *pairHash;

if (dumpMe)
    {
    bigHtmlFile = mustOpen("C:\\inetpub\\wwwroot\\test\\patAli.html", "w");
    littleHtmlFile = mustOpen("C:\\inetpub\\wwwroot\\test\\patSpace.html", "w");
    htmStart(bigHtmlFile, "PatSpace Alignments");
    htmStart(littleHtmlFile, "PatSpace Index");
    }

if ((hostName = getenv("HOST")) == NULL)
    hostName = "";

if (argc != 6)
    usage();

pushWarnHandler(patSpaceWarnHandler);
startTime = clock1000();
dnaUtilOpen();
makePolys();
genoListName = argv[1];
cdnaListName = argv[2];
oocFileName = argv[3];
pairFileName = argv[4];
outRoot = argv[5];

sprintf(hitFileName, "%s.hit", outRoot);
sprintf(mergerFileName, "%s.glu", outRoot);
sprintf(okFileName, "%s.ok", outRoot);

readAllWords(genoListName, &genoList, &genoListSize, &genoListBuf);
readAllWords(cdnaListName, &cdnaList, &cdnaListSize, &cdnaListBuf);
pairHash = makePairHash(pairFileName);

hitOut = mustOpen(hitFileName, "w");
mergerOut = mustOpen(mergerFileName, "w");
dumpOut = mustOpen("dump.out", "w");
seqListList = needMem(genoListSize*sizeof(seqListList[0]) );
fprintf(hitOut, "Pattern space 0.2 cDNA matcher\n");
fprintf(hitOut, "cDNA files: ", cdnaListSize);
for (i=0; i<cdnaListSize; ++i)
    fprintf(hitOut, " %s", cdnaList[i]);
fprintf(hitOut, "\n");
fprintf(hitOut, "%d genomic files\n", genoListSize);
for (i=0; i<genoListSize; ++i)
    {
    genoName = genoList[i];
    if (!startsWith("//", genoName)  )
        {
        seqListList[i] = seq = faReadAllDna(genoName);
        fprintf(hitOut, "%d els in %s ", slCount(seq), genoList[i]);
        for (; seq != NULL; seq = seq->next)
            fprintf(hitOut, "%d ", seq->size);
        fprintf(hitOut, "\n");
        }
    }

patSpace = makePatSpace(seqListList, genoListSize, oocFileName, 4, 100000);

for (i=0; i<cdnaListSize; ++i)
    {
    FILE *f;
    char *estFileName;
    DNA *dna;
    char *estName;
    int size;
    int c;
    int maxSizeForFuzzyFind = 20000;
    int dotCount = 0;

    estFileName = cdnaList[i];
    if (startsWith("//", estFileName)  )
		continue;

    f = mustOpen(estFileName, "rb");
    while ((c = fgetc(f)) != EOF)
    if (c == '>')
        break;
    printf("%s", cdnaList[i]);
    fflush(stdout);
    while (fastFaReadNext(f, &dna, &size, &estName))
        {
	aliSeqName = estName;
        if (size < maxSizeForFuzzyFind)  /* Some day need to fix this somehow... */
            {
            struct hashEl *hel;
            struct cdnaAliList *calList = NULL;

            hel = hashLookup(pairHash, estName);
            if (hel != NULL)    /* Do pair processing. */
                {
                struct estPair *ep;
                struct seq *thisSeq, *otherSeq;

                ep = hel->val;
                if (hel->name == ep->name3)
                    {
                    thisSeq = &ep->seq3;
                    otherSeq = &ep->seq5;
                    }
                else
                    {
                    thisSeq = &ep->seq5;
                    otherSeq = &ep->seq3;
                    }
                if (otherSeq->dna == NULL)  /* First in pair - need to save sequence. */
                    {
                    thisSeq->size = size;
                    thisSeq->dna = needMem(size);
                    memcpy(thisSeq->dna, dna, size);
                    }
                else                        /* Second in pair - do gluing and free partner. */
                    {
                    char mergedName[64];
                    thisSeq->dna = dna;
                    thisSeq->size = size;
                    sprintf(mergedName, "%s_AND_%s", ep->name5, ep->name3);

                    glueFindOne(patSpace, ep->seq5.dna, ep->seq5.size,
                        '+', '5', ep->name5, &calList);
                    reverseComplement(ep->seq5.dna, ep->seq5.size);
                    glueFindOne(patSpace, ep->seq5.dna, ep->seq5.size,
                        '-', '5', ep->name5, &calList);
                    glueFindOne(patSpace, ep->seq3.dna, ep->seq3.size,
                        '+', '3', ep->name3, &calList);
                    reverseComplement(ep->seq3.dna, ep->seq3.size);
                    glueFindOne(patSpace, ep->seq3.dna, ep->seq3.size,
                        '-', '3', ep->name3, &calList);
                    slReverse(&calList);
                    writeMergers(calList, mergedName, genoList);

                    freez(&otherSeq->dna);
                    thisSeq->dna = NULL;
                    thisSeq->size =otherSeq->size = 0;
                    }
                }
            else
                {
                glueFindOne(patSpace, dna, size, '+', '5', estName, &calList);
                reverseComplement(dna, size);
                glueFindOne(patSpace, dna, size, '-', '5', estName, &calList);
                slReverse(&calList);
                writeMergers(calList, estName, genoList);
                }
            ++estIx;
            if ((estIx & 0xfff) == 0)
                {
                printf(".");
                ++dotCount;
                fflush(stdout);
                }
            }
        }
    printf("\n");
    }
aliSeqName = "";
printf("ffSubmitted %3d ffAccepted %3d ffOkScore %3d ffSolidMatch %2d\n",
    ffSubmitted, ffAccepted, ffOkScore, ffSolidMatch);

endTime = clock1000();

printf("Total time is %4.2f\n", 0.001*(endTime-startTime));

/* Write out file who's presense say's we succeeded */
    {
    FILE *f = mustOpen(okFileName, "w");
    fputs("ok", f);
    fclose(f);
    }

if (dumpMe)
    {
    htmEnd(bigHtmlFile);
    htmEnd(littleHtmlFile);
    }
return 0;
}

