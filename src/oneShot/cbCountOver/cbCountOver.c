/* cbCountOver - count the number of crude alignment that score over a threshold. */
#include "common.h"

void printOverThreshold(char *inName, int minCount, int minScore, FILE *out)
/* Print alignments over threshold. */
{
FILE *in;
char line[512];
int lineCount;
char *words[64];
int wordCount;
char seqName[256];
int start, end;
int overCount;
boolean overReported;
int reportCount = 0;

in = mustOpen(inName, "r");
while (fgets(line, sizeof(line), in))
    {
    if (++lineCount % 100000 == 0)
        printf("Processing line %d of %s\n", lineCount, inName);
    wordCount = chopLine(line, words);
    if (wordCount < 1)
        continue;
    if (sameString(words[0], "Aligning"))
        {
        overCount = 0;
        overReported = FALSE;
        strcpy(seqName, words[6]);
        start = atoi(words[2]);
        end = atoi(words[4]);
        }
    else if (isdigit(words[0][0]))
        {
        int score = atoi(words[0]);
        if (score >= minScore)
            {
            ++overCount;
            if (overCount >= minCount && !overReported)
                {
                overReported = TRUE;
                fprintf(out, "%s:%d-%d\n", seqName, start, end);
                ++reportCount;
                }
            }
        }
    }
printf("%d sequences with at least %d hits of score %d or more\n",
    reportCount, minCount, minScore);
fprintf(out, "%d sequences with at least %d hits of score %d or more\n",
    reportCount, minCount, minScore);
}

int main(int argc, char *argv[])
{
int minCount, minScore;
char *inName;
FILE *out;

if (argc != 5 || !isdigit(argv[2][0]) || !isdigit(argv[3][0]))
    {
    errAbort("cbCountOver - count the number of crude alignments over a threshold.\n"
             "usage:\n"
             "     cbCountOver cbAli.out minCount minScore outputFile\n");
    }
inName = argv[1];
minCount = atoi(argv[2]);
minScore = atoi(argv[3]);
out = mustOpen(argv[4], "w");
printOverThreshold(inName, minCount, minScore, out);
return 0;
}