/* Synteny - figure out synteny statistics for cross-species alignment. 
 * (Synteny - how much order of genes in one species resembles another. */
#include "common.h"
#include "dnautil.h"
#include "portable.h"
#include "dlist.h"
#include "dnaseq.h"
#include "fa.h"

struct contig
/* A contiguously aligned sequence pair. */
    {
    char *name;
    char *query;
    int qStart, qEnd;
    int qOffset, qEndOffset;
    char qStrand;
    char *target;
    int tStart, tEnd;
    int tOffset, tEndOffset;
    char tStrand;
    int score;
    char *qSym;     /* Query symbols (nucleotides and insert char) */
    char *tSym;     /* Target symbols (nucleotides and insert char) */
    char *hSym;     /* "Hidden" state symbols. */
    int symCount;
    boolean isComplete;
    };

static void freeContig(struct contig **pContig)
/* Free up a contig. */
{
struct contig *contig = *pContig;
if (contig != NULL)
    {
    freeMem(contig->name);
    freeMem(contig->target);
    freeMem(contig->qSym);
    freeMem(contig->tSym);
    freeMem(contig->hSym);
    freez(pContig);
    }
}

static void freeContigList(struct dlList **pContigList)
/* Free up list of contigs. */
{
struct dlNode *node;
struct contig *contig;

for (node = (*pContigList)->head; node->next != NULL; node = node->next)
    {
    contig = node->val;
    freeContig(&contig);
    }
freeDlList(pContigList);
}

static void checkComplete(struct dlList *contigList)
/* Make sure all the contigs on the list are complete. */
{
struct dlNode *node;
struct contig *contig;

for (node = contigList->head; node->next != NULL; node = node->next)
    {
    contig = node->val;
    if (!contig->isComplete)
        {
        errAbort("Contig %s:%d-%d %c %s:%d-%d %c isn't complete",
            contig->query, contig->qStart, contig->qEnd, contig->qStrand,
            contig->target, contig->tStart, contig->tEnd, contig->tStrand);
        }
    }
}

static int cmpContigQueryStart(const void *va, const void *vb)
/* Compare two contigs by start position and then by inverse
 * stop position. (This ensures that if one contig completely
 * encompasses another it will appear on list first.) */
{
struct contig **pA = (struct contig **)va;
struct contig **pB = (struct contig **)vb;
struct contig *a = *pA, *b = *pB;
int dif;

dif = a->qStart - b->qStart;
if (dif == 0)
   dif = b->qEnd - a->qEnd;
return dif;
}

static int countNonGap(char *s, int size)
/* Count number of non-gap characters in string s of given size. */
{
int count = 0;
while (--size >= 0)
    if (*s++ != '-') ++count;
return count;
}

int minScore = 80000;

int kCounts[100];
int cloneSegCounts[100];

int segCount;
int segTotalSize;
int segAverageSize;


void syntenyOnClone(char *queryName, struct dlList *contigList, FILE *out)
/* Figure out synteny for one query sequence thats in contigList. */
{
struct dlNode *node;
struct contig *lastContig = NULL, *contig = NULL;
int tStart, tEnd, qStart, qEnd, qSize;
boolean inRange;
int maxSynSkip = 50000;
int contigCount = dlCount(contigList);
int cloneSegCount = 0;
int kIx;

if (contigCount < 1)
    return;
for (node = contigList->head; node->next != NULL; node = node->next)
    {
    contig = node->val;
    if (contig->score < minScore)
        continue;
    inRange = TRUE;
    if (lastContig != NULL && contig != NULL)
        {
        inRange = (sameString(contig->target, lastContig->target) && 
            contig->tOffset - maxSynSkip < lastContig->tOffset && contig->tOffset + maxSynSkip > lastContig->tOffset
            && contig->tEndOffset - maxSynSkip < lastContig->tEndOffset && contig->tEndOffset + maxSynSkip > lastContig->tEndOffset);
        }

    if (!inRange || contig == NULL)
        {
        tEnd = lastContig->tEndOffset;
        qEnd = lastContig->qEnd;
        qSize = qEnd - qStart;
        fprintf(out, "%s:%d-%d (%d) %s:%d-%d %c\n", queryName, qStart, qEnd, qSize,
            lastContig->target, tStart, tEnd, lastContig->qStrand);
        ++cloneSegCount;
        kIx = qSize/1000;
        if (kIx >= ArraySize(kCounts))
            kIx = ArraySize(kCounts)-1;
        ++kCounts[kIx];        
        ++segCount;
        segTotalSize += qSize;
        }
    if (!inRange || lastContig == NULL)
        {
        tStart = contig->tOffset;
        qStart = contig->qStart;
        }
    lastContig = contig;
    }
if (lastContig != NULL)
    {
    tEnd = lastContig->tEndOffset;
    qEnd = lastContig->qEnd;
    qSize = qEnd - qStart;
    fprintf(out, "%s:%d-%d (%d) %s:%d-%d %c\n", queryName, qStart, qEnd, qSize,
        lastContig->target, tStart, tEnd, lastContig->qStrand);
    ++cloneSegCount;
    kIx = qSize/1000;
    if (kIx >= ArraySize(kCounts))
        kIx = ArraySize(kCounts)-1;
    ++kCounts[kIx];        
    ++segCount;
    segTotalSize += qSize;
    }
if (cloneSegCount > ArraySize(cloneSegCounts))
    cloneSegCount = ArraySize(cloneSegCounts)-1;
++cloneSegCounts[cloneSegCount];
fprintf(out, "\n");
}

void figureSynteny(char *inName, FILE *out)
/* Figure out synteny stats - how much in a row aligns. */
{
FILE *in;
char line[512];
int lineCount = 0;
char *words[64];
int wordCount;
char *firstWord;
char *queryName = "";
struct contig *contig;
struct dlNode *contigNode;
struct dlList *contigList = newDlList();
int lineState = 0;   /* Keeps track of groups of four lines. */
struct slName *queryNameList = NULL;
int maxSymCount = 64*1024;
char *qSymBuf = needMem(maxSymCount+1);
char *tSymBuf = needMem(maxSymCount+1);
char *hSymBuf = needMem(maxSymCount+1);
int symCount = 0;
int qSymLen, tSymLen, hSymLen;
int bestSegScore;
int lastQoff = -1;
int i;

in = mustOpen(inName, "r");
while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    if ((lineCount%100000) == 0)
        {
        printf("Processing line %d of %s\n", lineCount, inName);
        }
    if (++lineState == 5)
        lineState = 0;
    wordCount = chopLine(line, words);
    if (wordCount <= 0)
        continue;
    firstWord = words[0];
    if (sameString(firstWord, "Aligning"))
        {
        char *queryString;
        char *targetString;
        char queryStrand, targetStrand;
        char *parts[8];
        int partCount;

        /* Do some preliminary checking of this line. */
        if (wordCount < 6)
            errAbort("Short line %d of %s", lineCount, inName);
        queryString = words[1];
        queryStrand = words[2][0];
        targetString = words[4];
        targetStrand = words[5][0];

        /* Extract the name of the query sequence.  If it's new,
         * then write out contigs on previous query we've accumulated
         * so far and start a new list. */
        partCount = chopString(queryString, ":-", parts, ArraySize(parts));
        if (!sameString(parts[0], queryName))
            {
            /* Allocate new name and keep track of it. */
            struct slName *newName = newSlName(parts[0]);
            slAddHead(&queryNameList, newName);

            /* Set last Segment for this clone to impossible val. */
            bestSegScore = -0x3fffffff;
            lastQoff = -1;

            /* Write out old contigs and empty out contig list. */
            syntenyOnClone(queryName, contigList, out);
            freeContigList(&contigList);
            contigList = newDlList();
            queryName = newName->name;
            }
        
        /* Make up a new contig, and fill it in with the data we
         * have so far about query. */
        AllocVar(contig);
        contig->query = queryName;
        contig->qOffset = atoi(parts[1]);
        contig->qEndOffset = atoi(parts[2]);
        contig->qStrand = queryStrand;

        if (lastQoff != contig->qOffset)
            {
            lastQoff = contig->qOffset;
            bestSegScore = -0x3fffffff;
            }
        /* Parse target string and fill in contig with it's info. */
        chopString(targetString, ":-", parts, ArraySize(parts));
        contig->target = cloneString(parts[0]);
        contig->tOffset = atoi(parts[1]);
        contig->tEndOffset = atoi(parts[2]);
        contig->tStrand = targetStrand;

        /* We don't know start and end yet - set them to values
         * that will get easily replace by max/min. */
        contig->qStart = contig->tStart = 0x3fffffff;

        lineState = -1;
        symCount = 0;
        }
    else if (sameString(firstWord, "best"))
        {
        if (wordCount < 3)
            errAbort("Short line %d of %s", lineCount, inName);
        contig->score = atoi(words[2]);
        if (contig->score > bestSegScore && contig->score >= minScore)
            {
            struct dlNode *tailNode;
            struct contig *tailContig;
            bestSegScore = contig->score;
            contig->isComplete = TRUE;
            contig->qSym = cloneStringZ(qSymBuf, symCount);
            contig->tSym = cloneStringZ(tSymBuf, symCount);
            contig->hSym = cloneStringZ(hSymBuf, symCount);
            contig->symCount = symCount;
            contig->qEnd = contig->qStart + countNonGap(qSymBuf, symCount);
            contig->tEnd = contig->tStart + countNonGap(tSymBuf, symCount);
            tailNode = contigList->tail;
            if (tailNode != NULL)
                {
                tailContig = tailNode->val;
                if (tailContig->qOffset == contig->qOffset)
                    {
                    freeContig(&tailContig);
                    dlRemove(tailNode);
                    freeMem(tailNode);
                    }
                }
            contigNode = dlAddValTail(contigList, contig);
            }
        }
    else if (wordCount > 1 && isdigit(firstWord[0]) || firstWord[0] == '-')
        {
        int start, end;
        char *sym = words[1];
        int symLen = strlen(sym);
        char firstChar = firstWord[0];
        if (lineState != 0 && lineState != 2)
            errAbort("Bummer - phasing mismatch on lineState line %d of %s!\n", lineCount, inName);
        assert(lineState == 0 || lineState == 2);
        start = atoi(firstWord);
        end = start + symLen;
        if (symCount + symLen > maxSymCount)
            {
            errAbort("Single contig too long line %d of %s, can only handle up to %d symbols\n",
                lineCount, inName, maxSymCount);
            }
        if (lineState == 0) /* query symbols */
            {
            qSymLen = symLen;
            if (isdigit(firstChar))
                {
                start += contig->qOffset;
                end += contig->qOffset;
                contig->qStart = min(contig->qStart, start);
                contig->qEnd = max(contig->qEnd, end);
                }
            memcpy(qSymBuf+symCount, sym, symLen);
            }
        else               /* target symbols */
            {
            tSymLen = symLen;
            if (tSymLen != qSymLen)
                {
                errAbort("Target symbol size not same as query line %d of %s",
                    lineCount, inName);
                }            
            if (isdigit(firstChar))
                {
                start += contig->tOffset;
                end += contig->tOffset;
                contig->tStart = min(contig->tStart, start);
                }
            memcpy(tSymBuf+symCount, sym, symLen);
            }
        }
    else if (firstWord[0] == '(')
        {
        lineState = -1;
        }
    else
        {
        assert(lineState == 1 || lineState == 3);
        if (lineState == 3)  /* Hidden symbols. */
            {
            char *sym = firstWord;
            int symLen = strlen(sym);
            hSymLen = symLen;
            if (hSymLen != qSymLen)
                {
                errAbort("Hidden symbol size not same as query line %d of %s",
                    lineCount, inName);
                }
            memcpy(hSymBuf+symCount, sym, symLen);
            symCount += symLen;
            }        
        }
    } 
syntenyOnClone(queryName, contigList, out);
freeContigList(&contigList);
fclose(in);

slFreeList(&queryNameList);
freeMem(qSymBuf);
freeMem(tSymBuf);
freeMem(hSymBuf);

fprintf(out, "CloneSegCounts[] = \n");
for (i=0; i<ArraySize(cloneSegCounts); ++i)
    fprintf(out, "%d %d\n", i, cloneSegCounts[i]);
fprintf(out, "\n");

fprintf(out, "kCounts[] = \n");
for (i=0; i<ArraySize(kCounts); ++i)
    fprintf(out, "%d %d\n", i, kCounts[i]);

segAverageSize = round((double)segTotalSize/segCount);
fprintf(out, "\n%d Segments, average size %d\n", segCount, segAverageSize);

}

int cosmidCount;
int cosmidTotalSize;
int cosmidAverageSize;

void countCosmids(char *listFileName, FILE *out)
/* Read each cosmid in list file and find out how big it is. */
{
FILE *listFile = mustOpen(listFileName, "r");
char line[512], *s;
int lineCount;
struct dnaSeq *seq;
char path[512];

while (fgets(line, sizeof(line), listFile))
    {
    ++lineCount;
    s = trimSpaces(line);
    sprintf(path, "%s/%s", "C:/biodata/cbriggsae/finish", s);
    seq = faReadDna(path);
    ++cosmidCount;
    cosmidTotalSize += seq->size;
    freeDnaSeq(&seq);
    }
fclose(listFile);
cosmidAverageSize = round((double)cosmidTotalSize/cosmidCount);
fprintf(out, "%d cosmids, average length %d\n", cosmidCount, cosmidAverageSize);
}

int main(int argc, char *argv[])
{
char *inName = "C:/biodata/cbriggsae/test/all.dyn";
char *outName = "synteny.out";
FILE *out = mustOpen(outName, "w");
long start, end;

dnaUtilOpen();
start = clock1000();
figureSynteny(inName, out);
countCosmids("C:/biodata/cbriggsae/finish/allcosmi.txt", out);
end = clock1000();
printf("%4.2f seconds to process\n", (end-start)*0.001);
return 0;
}