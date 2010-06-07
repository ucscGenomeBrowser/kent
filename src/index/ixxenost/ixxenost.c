/* ixxenost - Index cross species alignment file short stitched (.st) format. */
#include "common.h"
#include "snof.h"
#include "snofmake.h"

static void skipThroughLf(FILE *f)
/* Just skip from current file position to next line feed. */
{
int c;

while ((c = fgetc(f)) != EOF)
    if (c == '\n')
        break;
}

static boolean stNextRecord(FILE *f, void *data, char **rName, int *rNameLen)
/* Reads next record of st file and returns the name from it. */
{
static char lineBuf[256];
char *words[32];
int wordCount;

if (fgets(lineBuf, sizeof(lineBuf), f) == NULL)
    return FALSE;
wordCount = chopLine(lineBuf, words);
*rName = words[0];
*rNameLen = strlen(words[0]);
skipThroughLf(f);
skipThroughLf(f);
skipThroughLf(f);
return TRUE;
}



int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in;

if (argc != 3)
    {
    errAbort("ixxenost - index cross species alignment file.\n"
             "usage\n"
             "    ixxenost infile.st outfile.ix");
    }
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "rb");
if (!snofMakeIndex(in, outName, stNextRecord, NULL))
    errAbort("Couldn't make index file %s\n", outName);
return 0;
}