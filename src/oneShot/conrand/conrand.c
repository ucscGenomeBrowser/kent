/* conrand.c - condense information on Improbizer run on random sequences */
#include "common.h"

int main(int argc, char *argv[])
{
char *inFileName;
FILE *in;
char line[1024];
int lineCount;
char *words[100];
int wordCount;
int seqCount, seqSize;
int take = 0;
int maxTakes = 2;
int lastSeqCount = -1, lastSeqSize = -1;
int motif = 0;
int maxMotifs = 2;
double scores[4];

if (argc != 2)
    {
    errAbort("conrand - condense info on Improbizer runs on random sequences.\n"
             "usage: conrand infile\n");
    }
inFileName = argv[1];
in = fopen(inFileName, "r");
zeroBytes(scores, sizeof(scores));
while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    if (line[0] == '-')
        {
        wordCount = chopLine(line, words);
        seqCount = atoi(words[1]);
        seqSize = atoi(words[5]);
        motif = 0;
        take += 1;
        }    
    else if (isdigit(line[0]))
        {
        scores[motif++] += atof(line);
        if (motif == maxMotifs && take == maxTakes)
            {
            printf("%4.2f ", scores[1]/maxTakes);
            //if (seqCount == 100)
              //  printf("\n");
            take = 0;
            zeroBytes(scores, sizeof(scores));
            }
        }
    }
fclose(in);
return 0;
}
