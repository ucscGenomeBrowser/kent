/* memeint - make up a Meme input file from intron data. */
#include "common.h"
#include "wormdna.h"


int main(int argc, char *argv[])
{
FILE *in, *out;
char inName[512], *outName;
char line[4096];
int lineCount;
char *words[512];
int wordCount;
char *pat;
int patLen;
int matchCount = 0;

if (argc != 3)
    {
    errAbort("memeint - make a meme input file from intron pattern matching.\n"
             "usage:\n"
             "      memint 5'pat memeFile");
    }
pat = argv[1];
tolowers(pat);
patLen = strlen(pat);
outName = argv[2];

sprintf(inName, "%s%s", wormCdnaDir(), "introns.txt");
in = mustOpen(inName, "rb");
out = mustOpen(outName, "w");

while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    if (lineCount%5000 == 0)
        printf("processing line %d of %s\n", lineCount, inName);
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;   /* Blank lines ok with me. */
    if (wordCount < 11)
        errAbort("Not enough words line %d of %s\n", lineCount, inName);
    if (strncmp(words[7], pat, patLen) == 0)
        {
        ++matchCount;
        if (matchCount > 300)
            break;
        fprintf(out, ">n%d in %s %s:%s-%s\n", matchCount, words[5], 
            words[1], words[2], words[3]);
        //fprintf(out, "%s\n", words[6]);
        fprintf(out, "%s\n", words[8]);
        }
    } 
return 0;    
}