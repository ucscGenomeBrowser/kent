/* subdyn.c - Extracts a subset of a .dyn file. */
#include "common.h"

void usage()
/* Explain usage of program. */
{
errAbort("subdyn - extract a subset of a .dyn file\n"
         "usage\n"
         "   subdyn in.dyn pattern out.dyn");
}

int main(int argc, char *argv[])
{
char *inName, *outName;
char *pattern;
FILE *in, *out;
char *key = "Aligning";
int keySize = strlen(key);
char line[512];
int lineCount = 0;
int outLineCount = 0;
boolean inPat = FALSE;

if (argc != 4)
    usage();
inName = argv[1];
pattern = argv[2];
outName = argv[3];

in = mustOpen(inName, "r");
out = mustOpen(outName, "w");

while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    if (lineCount%500000 == 0)
        printf("processing line %d of %s\n", lineCount, inName);
    if (memcmp(line, key, keySize) == 0)
        {
        inPat = (strstr(line, pattern) != NULL);
        if (inPat)
            fputs(line, stdout);
        }
    if (inPat)
        {
        ++outLineCount;
        fputs(line, out);
        }
    }
printf("Subsetted %d of %d lines (%s)\n", outLineCount, lineCount, pattern);
fclose(out);
return 0;
}
