/* Copy a text file to another text file stopping at line that matches a pattern. */
#include "common.h"

int main(int argc, char *argv[])
{
char *pat = "Aligning FOSMIDS\\G45N02.SEQ:11000-13000 + to v:20932180-20937180 +";
int patLen = strlen(pat);
char *inName, *outName;
FILE *in, *out;
char line[1024];
int lineCount = 0;

if (argc != 3)
    errAbort("usage: truncat source dest");
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "r");
out = mustOpen(outName, "w");
while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    if (lineCount % 100000 == 0)
        printf("Processing line %d\n", lineCount);
    if (strncmp(line, pat, patLen) == 0)
        {
        printf("Breaking at line %d\n", lineCount);
        break;
        }
    fputs(line, out);
    }
fclose(in);
fclose(out);
return 0;
}