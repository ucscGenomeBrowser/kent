/* countSym - count hidden Markov symbols in file. Also make list of shortish repeats.
 */
#include "common.h"
#include "hash.h"

boolean skipLine(FILE *f)
/* Skip until end of line.  Return FALSE at EOF. */
{
int c;
for (;;)
    {
    c = fgetc(f);
    if (c == '\n')
        return TRUE;
    else if (c == EOF)
        return FALSE;
    }
}

struct lenCount
/* A list of counts. */
    {
    struct lenCount *next;
    char *name;
    int size;
    int count;
    };

struct aliInfoOne
/* Information on one alignment. */
    {
    struct aliInfoOne *next;
    char *name;
    double percent;
    int symCount;
    char *query;
    int qStart, qEnd;
    char *target;
    int tStart, tEnd;
    char qStrand, tStrand;
    };

int cmpSymCount(const void *va, const void *vb)
/* Compare aliInfoOne based on symbol count. */
{
const struct aliInfoOne *a = *((struct aliInfoOne **)va);
const struct aliInfoOne *b = *((struct aliInfoOne **)vb);
return a->symCount - b->symCount;
}


struct aliInfoOne *readInfoLine(FILE *f)
/* Read info line and return parsed struct.   Example info line:
 *  G11A11.c1 align 54.3% of 5934 ACTIN2~1\G11A11:0-4999 - v:9730935-9736764 + 
 *     0        1     2    3   4           5             6          7        8 */
{
struct aliInfoOne *ai;
char line[256];
char *words[16];
int wordCount;
char *parts[4];
int partCount;

if (fgets(line, sizeof(line), f) == NULL)
    return NULL;
AllocVar(ai);
wordCount = chopLine(line, words);
if (wordCount != 9)
    errAbort("Short info line, aborting\n");
if (!sameString(words[1], "align"))
    errAbort("Missing 'align' in info, aborting\n");
ai->name = cloneString(words[0]);
ai->percent = atof(words[2]);
ai->symCount = atoi(words[4]);
partCount = chopString(words[5], ":-", parts, ArraySize(parts));
ai->query = cloneString(parts[0]);
ai->qStart = atoi(parts[1]);
ai->qEnd = atoi(parts[2]);
ai->qStrand = words[6][0];
partCount = chopString(words[7], ":-", parts, ArraySize(parts));
ai->target = cloneString(parts[0]);
ai->tStart = atoi(parts[1]);
ai->tEnd = atoi(parts[2]);
ai->tStrand = words[8][0];
return ai;
}

int lenCountCmp(const void *va, const void *vb)
/* Compare two lenCounts by name. */
{
const struct lenCount *a = *((struct lenCount **)va);
const struct lenCount *b = *((struct lenCount **)vb);
return strcmp(a->name, b->name);
}

boolean countSym(FILE *f, struct hash *hashes[256], struct lenCount *counts[256])
/* Add characters to counts until newline or EOF.  Return
 * FALSE at EOF. */
{
int c;
int lastC = -2;
int sameCount = 0;
struct hash *hash;
struct lenCount *counter;
struct hashEl *hEl;

for (;;)
    {
    c = fgetc(f);
    if (c == '1' || c == '2' || c == '3')
        c = 'C';
    if (c == lastC)
        ++sameCount;
    else
        {
        if (lastC >= 0)
            {
            char nameBuf[16];
            sprintf(nameBuf, "%5d", sameCount);
            hash = hashes[lastC];
            if ((hEl = hashLookup(hash, nameBuf)) == NULL)
                {
                AllocVar(counter);
                slAddHead(&counts[lastC], counter);
                hEl = hashAdd(hash, nameBuf, counter);
                counter->name = hEl->name;
                counter->size = sameCount;
                }
            counter = hEl->val; 
            counter->count += 1;
            }
        lastC = c;
        sameCount = 1;
        } 
    if (c == '\n')
        return TRUE;
    else if (c == EOF)
        return FALSE;
    }
}

void printCounts(struct lenCount *counts[256], FILE *f)
/* Print count of characters. */
{
int i;
struct lenCount *lc;

for (i=0; i<256; ++i)
    {
    if (counts[i] != NULL)
        {
        int totalSize = 0;
        int totalNumber = 0;
        double average;
        double outProb, inProb;

        for (lc = counts[i]; lc != NULL; lc = lc->next)
            {
            totalSize += lc->size * lc->count;
            totalNumber += lc->count;
            }
        average = (double)totalSize/totalNumber;
        outProb = 1.0/average;
        inProb = 1.0 - outProb;
        fprintf(f, "%c length distribution (average size %5.2f total# %d)\n", i, average, totalNumber);
        fprintf(stdout, "%c length distribution (average size %5.2f total# %d)\n", i, average, totalNumber);
        slSort(&counts[i], lenCountCmp);
        for (lc = counts[i]; lc != NULL; lc = lc->next)
            {
            fprintf(f, "%4d : %4d\n", lc->size, lc->count);
            }
        fprintf(f, "\n");
        }
    }
}

struct smallMatch
/* Store info about small alignment. */
    {
    struct smallMatch *next;
    char *ceSeq;
    int count;
    struct aliInfoOne *ai;
    };

int cmpSmallCount(const void *va, const void *vb)
/* Compare smallMatch based on count. */
{
const struct smallMatch *a = *((struct smallMatch **)va);
const struct smallMatch *b = *((struct smallMatch **)vb);
return b->count - a->count;
}

void printSmall(struct smallMatch *smallList, int smallSize, FILE *f)
/* Print info on (sorted) list to file. */
{
struct smallMatch *sm;

fprintf(f, "Info on alignments %d or smaller\n", smallSize);
fprintf(stdout, "Info on alignments %d or smaller\n", smallSize);
for (sm = smallList; sm != NULL; sm = sm->next)
    {
    fprintf(f, "%d %s\n", sm->count, sm->ceSeq);
    }
}

void readSaveSmall(struct hash *smallHash, struct aliInfoOne *ai, FILE *f, struct smallMatch **pSmallList)
/* Read in line of symbols and save it in smallHash. */
{
char line[256];
struct hashEl *hel;
struct smallMatch *sm;

fgets(line, sizeof(line), f);
line[ai->symCount] = 0;
if ((hel = hashLookup(smallHash, line)) == NULL)
    {
    AllocVar(sm);
    sm->ai = ai;
    slAddHead(pSmallList, sm);
    hel = hashAdd(smallHash, line, sm);
    sm->ceSeq = hel->name;
    }
else
    sm = hel->val;
sm->count += 1;
}

void countFile(char *fileName, struct hash *hashes[256], struct lenCount *counts[256],
    struct aliInfoOne **pInfoList, struct hash *smallHash, int smallSize, 
    struct smallMatch **pSmallList)
/* Do counting on all of file. */
{
FILE *f = mustOpen(fileName, "r");
int aliCount = 0;
struct aliInfoOne *infoList = NULL, *ai;

for (;;)
    {
    if (++aliCount % 500 == 0)
        printf("Processing alignment %d\n", aliCount);
    if ((ai = readInfoLine(f)) == NULL)
        break;
    slAddHead(&infoList, ai);
    skipLine(f);
    if (ai->symCount <= smallSize)
        {
        readSaveSmall(smallHash, ai, f, pSmallList);
        }
    else
        skipLine(f);
    if (!countSym(f, hashes, counts))
        break;
    }
fclose(f);
slReverse(&infoList);
*pInfoList = infoList;
}

void printAliSizes(struct aliInfoOne *infoList, FILE *f)
/* Print out relevant aliInfo parts. */
{
int size, lastSize = -1;
struct aliInfoOne *ai, *lastAi = NULL;
int count = 0;
int totalSize = 0, aiCount = 0;
double average;

for (ai = infoList; ai != NULL; ai = ai->next)
    {
    totalSize += ai->symCount;
    aiCount += 1;
    }
average = (double)totalSize/aiCount;
fprintf(f, "Alignment size distribution (average size %5.2f total# %d)\n", average, aiCount);
fprintf(stdout, "Alignment size distribution (average size %5.2f total# %d)\n", average, aiCount);
for (ai = infoList; ai != NULL; ai = ai->next)
    {
    size = ai->symCount;
    if (size != lastSize)
        {
        if (count > 0)
            fprintf(f, "%4d : %4d\n", lastSize, count);
        count = 1;
        lastSize = size;
        }
    else
        {
        ++count;
        }
    }
if (count > 0)
    fprintf(f, "%4d : %4d\n", size, count);
}


int main(int argc, char *argv[])
{
static struct hash *hashes[256];
static struct lenCount *counts[256];
struct hash *smallHash;
struct smallMatch *smallList = NULL;
struct aliInfoOne *infoList = NULL;
char syms[5] = "HLTQC";
int i;
FILE *out = mustOpen("C:/temp/countSym.out", "w");
int smallSize = 50;

for (i=0; i<ArraySize(syms); ++i)
    hashes[syms[i]] = newHash(12);
smallHash = newHash(14);
countFile("C:/biodata/celegans/xeno/cbriggsae/all.st", hashes, counts, &infoList, smallHash, smallSize, &smallList);
slSort(&smallList, cmpSmallCount);
printSmall(smallList, smallSize, mustOpen("/temp/repeats.txt", "w"));
fprintf(out, "\n");
slSort(&infoList, cmpSymCount);
printAliSizes(infoList, out);
printCounts(counts, out);
return 0;
}
