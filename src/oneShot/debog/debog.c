/* Debog - The Sanger chromosome .FA files contain some unusual letters
 * for nucleotides. Earlier I just threw these out. Alas, I needed to
 * keep them as placeholders. Now my offsets into the chromosomes are
 * messed up.  My choice was to rerun a week-long computing job to
 * generate the correct offsets, or to write this program, which patches
 * them up. */
#include "common.h"
#include "dnautil.h"
#include "wormdna.h"

void makeBogLists(char *bogDir, char *chromNames[], int chromCount, int *bogLists[],
    int bogCounts[])
{
FILE *bogFile;
int chromIx;
int *bog;
int maxBogSize = 25000;
char bogFileName[256];

for (chromIx=0; chromIx<chromCount; ++chromIx)
    {
    sprintf(bogFileName, "%s\\%s.bog", bogDir, chromNames[chromIx]);
    if ((bogFile = fopen(bogFileName, "r")) != NULL)
        {
        int bogCount = 0;
        char lineBuf[256];
        char *words[2];
        int wordCount;

        bogLists[chromIx] = bog = needLargeMem(maxBogSize*sizeof(*bog));
        while (fgets(lineBuf, sizeof(lineBuf), bogFile) != NULL)
            {
            wordCount = chopString(lineBuf, whiteSpaceChopper, words, ArraySize(words));
            if (wordCount > 0)
                {
                if (bogCount >= maxBogSize)
                    errAbort("More than %d bogs\n", maxBogSize);
                bog[bogCount++] = atoi(words[0]);
                }
            }
        bogCounts[chromIx] = bogCount;
        fclose(bogFile);
        uglyf("Read %d bogs in %s\n", bogCount, bogFileName);
        }
    else
        {
        bogLists[chromIx] = NULL;
        bogCounts[chromIx] = 0;
        }
    }
}

boolean findStringIx(char *string, char *list[], int listSize, int *retIx)
{
int i;
for (i=0; i<listSize; ++i)
    {
    if (!differentWord(string, list[i]))
        {
        *retIx = i;
        return TRUE;
        }
    }
return FALSE;
}

int debogOne(int x, int *bogs, int bogCount)
{
int i, bog;

if (bogs == NULL)
    return x;
for (i=0; i<bogCount; ++i)
    {
    bog = bogs[i];
    if (bog > x)
        break;
    ++x;
    }
return x;
}

void debogFilter(int *bogLists[], int bogCounts[], char *chromNames[], int chromCount, 
    FILE *in, FILE *out)
{
char lineBuf[512];
char wordBuf[512];
char *words[12];
int wordCount;

while (fgets(lineBuf, sizeof(lineBuf), in) != NULL)
    {
    strcpy(wordBuf, lineBuf);
    wordCount = chopString(wordBuf, whiteSpaceChopper, words, ArraySize(words));
    if (wordCount >= 5 && strcmp(words[2], "hits") == 0)
        {
        char *chromName;
        int chromIx;
        int start, end;
        char *startEndString;
        char *rangeWords[2];
        int rangeWordCount;
        int i;

        chromName = words[4];
        if (!findStringIx(chromName, chromNames, chromCount, &chromIx))
            {
            if (!differentWord(chromName, "Genome"))    /* Mitochondria */
                chromIx = 6;
            else
                errAbort("Couldn't find chromosome named %s\n", chromName);
            }
        startEndString = words[5];
        rangeWordCount = chopString(startEndString, "-", rangeWords, ArraySize(rangeWords));
        start = atoi(rangeWords[0]);
        end = atoi(rangeWords[1]);
        start = debogOne(start, bogLists[chromIx], bogCounts[chromIx]);
        end = debogOne(end, bogLists[chromIx], bogCounts[chromIx]);
        for (i=0; i<wordCount; ++i)
            {
            if (i == 5)
                {
                fprintf(out, "%d-%d ", start, end);
                }
            else
                {
                fprintf(out, "%s ", words[i]);
                }
            }
        fprintf(out, "\n");
        }
    else
        fputs(lineBuf, out);
    }
}

int main(int argc, char *argv[])
{
char *bogDir, *inName, *outName;
FILE *inFile, *outFile;
int *bogLists[8];
int bogCounts[8];
char **chromNames;
int chromCount;

if (argc != 4)
    {
    errAbort("Debog - fix up offsets in exonAli.out files according to\n"
             "lists in .bog files.\n"
             "Usage:\n"
             "    debog bogFileDir inFile outFile\n");
    }
bogDir = argv[1];
inName = argv[2];
outName = argv[3];
inFile = mustOpen(inName, "r");
outFile = mustOpen(outName, "w");

wormChromNames(&chromNames, &chromCount);
assert(chromCount < ArraySize(bogLists));

makeBogLists(bogDir, chromNames, chromCount, bogLists, bogCounts);


debogFilter(bogLists, bogCounts, chromNames, chromCount, inFile, outFile);

return 0;
}
