#include "common.h"
#include "fof.h"

static boolean faNextRecord(FILE *f, void *data, char **rName, int *rNameLen)
/* Reads next record of fa file and returns the name from it. */
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


void usage()
/* Print usage and exit. */
{
errAbort("fofFa - make an index out of multiple fa files\n"
         "usage:\n"
         "    fofFa index.fof file(s).fa");
}


int main(int argc, char *argv[])
{
if (argc < 3)
    usage();
fofMake(argv+2, argc-2, argv[1], NULL, faNextRecord, NULL, FALSE);
return 0;
}

