/* gbNoDna - filters DNA out of a gb file */
#include "common.h"


int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in, *out;
char line[1024];
char dupeLine[1024];
int lineCount = 0;
char *words[100];
int wordCount;
boolean inDna = FALSE;

if (argc != 3)
    {
    errAbort("gbNoDna - filter DNA sequences out of a gb file\n"
             "usage:\n"
             "     gbNoDna inFile outFile");
    }
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "r");
out = mustOpen(outName, "w");
while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    if (lineCount%1000 == 0)
        printf("Processing line %d\n", lineCount);
    if (memcmp(line, "LOCUS", 5) == 0)
        {
        strcpy(dupeLine, line);
        wordCount = chopLine(dupeLine, words);
        if (wordCount >= 5 && sameString(words[4], "DNA"))
            inDna = TRUE;
        else
            {
            inDna = FALSE;
            fputs(line, out);
            }
        }
    else if (!inDna)
        fputs(line, out);
    }
return 0;
}