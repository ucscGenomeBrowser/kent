/* Noali - this looks through the big blast output file from my non-aligners. */
#include "common.h"

struct bh
    {
    struct bh *next;
    int idP, idQ;
    };

struct info
    {
    struct info *next;
    char *cdnaName;
    int cdnaSize;
    char *cosmidName;
    struct bh *hitList;
    int score;
    double scaledScore;
    };

void gatherInfo(char *inName, struct info **retInfo, int *retHitCount, int *retMissCount)
{
FILE *in;
char line[512];
char *words[128];
int wordCount;
int lineCount = 0;
struct info *infoList = NULL, *info = NULL;
struct bh *bh = NULL;
boolean nextHasCosmid = FALSE;
int hitCount = 0, queryCount = 0;

in = mustOpen(inName, "r");
while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    if (lineCount%10000 == 0)
        printf("%s %d\n", inName, lineCount);
    wordCount = chopLine(line, words);
    if (wordCount < 1) continue;
    if (sameString(words[0], "####"))
        {
        char *parts[3];
        chopString(words[1], "/\\", parts, ArraySize(parts));
        AllocVar(info);
        slAddHead(&infoList, info);
        info->cdnaName = cloneString(parts[1]);
        ++queryCount;
        }    
    else if (wordCount > 1 && sameString(words[1], "letters)"))
        {
        info->cdnaSize = atoi(words[0]+1);
        }
    else if (wordCount > 4 && sameString(words[0], "*****") && sameString(words[1], "No") 
        && sameWord(words[2], "hits") )
        {
        infoList = infoList->next;  /* Erase info */
        }
    else if (wordCount > 3 && sameString(words[2], "significant") )
        {
        nextHasCosmid = TRUE;
        ++hitCount;
        }
    else if (nextHasCosmid)
        {
        int cosmidIx;
        char *cosmidName;
        char *commaPos;
        nextHasCosmid = FALSE;
        cosmidIx = stringArrayIx("cosmid", words, wordCount);
        if (cosmidIx < 1)
            errAbort("Couldn't find cosmid in line %d\n", lineCount);
        cosmidName = words[cosmidIx + 1];
        if ((commaPos = strchr(cosmidName, ',')) != NULL)
            *commaPos = 0;
        info->cosmidName = cloneString(cosmidName);
        }
    else if (sameString("Identities", words[0]) && wordCount > 2)
        {
        char *parts[3];
        AllocVar(bh);
        slAddTail(&info->hitList, bh);
        chopString(words[2], "/", parts, ArraySize(parts));
        bh->idP = atoi(parts[0]);
        bh->idQ = atoi(parts[1]);
        }
    }
slReverse(&infoList);
*retInfo = infoList;
*retHitCount = hitCount;
*retMissCount = queryCount - hitCount;
}

int cmpInfo(const void *va, const void *vb)
/* Compare two introns. */
{
struct info **pA = (struct info **)va;
struct info **pB = (struct info **)vb;
struct info *a = *pA, *b = *pB;
return (int)(10000*(b->scaledScore - a->scaledScore));
}

int sumIdP(struct info *info)
{
struct bh *bh;
int sum = 0;
for (bh = info->hitList; bh != NULL; bh = bh->next)
    sum += bh->idP;
return sum;
}

struct info *processInfo(struct info *info)
{
struct info *inf;
for (inf = info; inf != NULL; inf = inf->next)
    {
    inf->score = sumIdP(inf);
    inf->scaledScore = (double)(inf->score) / inf->cdnaSize;
    }
slSort(&info, cmpInfo);
return info;
}

void saveInfo(char *fileName, struct info *info)
{
FILE *f = mustOpen(fileName, "w");
for (;info != NULL; info = info->next)
    {
    fprintf(f, "%s hits %s about %d bases out of %d (%d%%)\n",
        info->cdnaName, info->cosmidName, info->score, info->cdnaSize,
        (int)((info->scaledScore)*100 + 0.5));
    }
}
              
int main(int argc, char *argv[])
{
struct info *info;
char *inName, *outName;
int hitCount, missCount;

if (argc != 3)
    {
    errAbort("Noali - analyses blast output of nonaligners\n"
             "usage:\n"
             "  noali input output");
    }
inName = argv[1];
outName = argv[2];
gatherInfo(inName, &info, &hitCount, &missCount);
printf("%d hits %d misses in %s\n", hitCount, missCount, inName);
info = processInfo(info);
saveInfo(outName, info);
return 0;
}