/* strainLumps - strain out the most popular lumps from a file created
 * by lumpRep, to make a set of repeats to screen against. */
#include "common.h"
#include "dnautil.h"

struct lump
/* Holds a repeating lump. */
    {
    struct lump *next;
    char *seq;
    int count;
    };

int cmpCount(const void *va, const void *vb)
/* Compare based on count. */
{
const struct lump *a = *((struct lump **)va);
const struct lump *b = *((struct lump **)vb);
return b->count - a->count;
}


struct lump *readLumps(char *fileName)
/* Read in lumps from file. */
{
struct lump *lumpList = NULL, *lump = NULL;
char line[1024];
int lineCount;
char *words[3];
int wordCount;
boolean isIndented;
FILE *f = mustOpen(fileName, "r");

while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    isIndented = isspace(line[0]);
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;   /* Allow blank lines. */
    if (isIndented)
        {
        if (wordCount != 2 || !isdigit(words[0][0]))
            errAbort("Bad line %d of %s\n", lineCount, fileName);
        lump->count += atoi(words[0]);
        }
    else
        {
        AllocVar(lump);
        lump->seq = cloneString(words[0]);
        slAddHead(&lumpList, lump);
        }
    }
fclose(f);
slReverse(&lumpList);
return lumpList;
}

void saveBigLumps(struct lump *lumpList, int minSize, char *fileName)
/* Save lumps of minSize or greater to fileName in fa format. */
{
struct lump *lump;
FILE *f = mustOpen(fileName, "w");
int count = 0;

for (lump = lumpList; lump != NULL; lump = lump->next)
    {
    if (lump->count >= minSize)
        {
        ++count;
        fprintf(f, ">ce_repeat_%d  x%d\n", count, lump->count);
        fprintf(f, "%s\n", lump->seq);
        }
    }
fclose(f);
}

int main(int argc, char *argv[])
{
char *inName = "../lumpRep/repeats.out";
char *outName = "repeats.fa";
struct lump *lumpList;

dnaUtilOpen();
lumpList = readLumps(inName);
slSort(&lumpList, cmpCount);
saveBigLumps(lumpList, 6, outName);
return 0;
}