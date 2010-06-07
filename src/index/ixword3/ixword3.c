/* ixword3 - Build an index file on a text file using the third
 * word of the line as the key, and the entire line as the record.
 */
#include "common.h"
#include "snofmake.h"

static boolean textNextRecord(FILE *f, void *data, char **rName, int *rNameLen)
/* Reads next record of file and returns the name from it. */
{
static char lineBuf[16*1024];
char *words[5];
int count;
static int lineCount = 0;

++lineCount;
if (fgets(lineBuf, sizeof(lineBuf), f) == NULL)
    return FALSE;
count = chopByWhite(lineBuf, words, ArraySize(words));
if (count < 3)
    {
    fprintf(stderr, "Short line %d.  I can't handle it!!\n", lineCount);
    exit(-1);
    }
*rName = words[2];
*rNameLen = strlen(words[2]);
return TRUE;
}

int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in;

if (argc != 3)
    {
    fprintf(stderr, "This program makes an index file for text file,\nindexing the third word of each line.\n");
    fprintf(stderr, "usage: %s textFile indexFile\n", argv[0]);
    exit(-1);
    }
inName = argv[1];
outName = argv[2];
if ((in = fopen(inName, "rb")) == NULL)
    {
    fprintf(stderr, "Couldn't open %s, sorry\n", inName);
    exit(-2);
    }
if (!snofMakeIndex(in, outName, textNextRecord, NULL))
    {
    exit(-4);
    }
return 0;
}
