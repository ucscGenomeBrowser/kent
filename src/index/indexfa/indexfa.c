/* This program builds an index for a .fa file. 
 * The format for the file is:
 * signature - a 16 byte number that identifies this as an index file.
 * nameSize -  a 4 byte number saying how much storage for each string.
 *             (which will be zero terminated)
 * alphabetical list of records.  Each record consists of the name
 *          and a 32 bit offset into the file.
 */

#include "common.h"
#include "snofmake.h"

static boolean faNextRecord(FILE *f, void *data, char **rName, int *rNameLen)
/* Reads next record of gdf file and returns the name from it. */
{
long offset;
char lineBuf[256];
static char nameBuf[256];
boolean gotName = FALSE;
static int lineCount = 1;

for (;;)
    {
    offset = ftell(f);
    if (fgets(lineBuf, sizeof(lineBuf), f) == NULL)
        break;
    if (lineBuf[0] == '>')
	{
        if (gotName)  
            {
            /* This line is part of next record - send it back! */
            fseek(f, offset, SEEK_SET);
            break;
            }
        else
            {
	    char *words[10];
	    int count;
	    count = chopString(lineBuf+1, whiteSpaceChopper, words, ArraySize(words));
	    if (count < 1)
	        {
	        fprintf(stderr, "Expecting a name after > on line %d\n", lineCount);
	        exit(-1);
	        }
            strcpy(nameBuf, words[0]);
            *rName = nameBuf;
            *rNameLen = strlen(nameBuf);
            gotName = TRUE;
	    }
        }
    ++lineCount;
    }
return gotName;
}


int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in;

if (argc != 3)
    {
    fprintf(stderr, "This program makes an index file for a .fa file\n");
    fprintf(stderr, "usage: %s file.fa indexFile\n", argv[0]);
    exit(-1);
    }
inName = argv[1];
outName = argv[2];
if ((in = fopen(inName, "rb")) == NULL)
    {
    fprintf(stderr, "Couldn't open %s, sorry\n", inName);
    exit(-2);
    }
if (!snofMakeIndex(in, outName, faNextRecord, NULL))
    {
    exit(-4);
    }
return 0;
}
