/* Index a ra file on any key. */
#include "common.h"
#include "sig.h"
#include "fof.h"


int recordCount = 0;
int keyCount = 0;
boolean justFirstWord;

static boolean raNextRecord(FILE *f, void *data, char **rName, int *rNameLen)
/* Reads next record of ra file and returns value of the field associated with
 * the key. */
{
char *key = data;
int keySize = strlen(data);
static char line[16*1024];
static char valBuf[16*1024];
static int lineCount;
int nameLen = 0;
char *val;
char *tag;
int tagSize;


if (fgets(line, sizeof(line), f) == NULL)
    return FALSE;
++recordCount;
for (;;)
    {
    ++lineCount;
    tag = line;
    val = strchr(line, ' ');
    if (val == NULL)
        break;
    tagSize = val-tag;
    val = trimSpaces(val);
    if (tagSize == keySize && memcmp(key, tag, tagSize) == 0)
        {
	if (justFirstWord)
	    {
	    char *s = strchr(val, ' ');
	    if (s != NULL)
		{
		nameLen = s - val;
		*s = 0;
		}
	    else
		nameLen = strlen(val);
	    }
	else
	    nameLen = strlen(val);
        memcpy(valBuf, val, nameLen+1);
        ++keyCount;
        }
    if (fgets(line, sizeof(line), f) == NULL)
        errAbort("Truncated input line %d", lineCount);
    }
*rNameLen = nameLen;
*rName = valBuf;
return TRUE;
}

void usage()
{
errAbort("This program makes an index file for a .fa file\n"
             "usage: indexRa indexFile.fof key dupe/unique file(s).ra\n");
}

int main(int argc, char *argv[])
{
char *outName, *key, *dupUniq;
boolean dupeOk;

if (argc < 5)
    {
    usage();
    }
outName = argv[1];
key = argv[2];
dupUniq = argv[3];
if (sameWord(dupUniq, "dupe"))
    dupeOk = TRUE;
else if (sameWord(dupUniq, "unique"))
    dupeOk = FALSE;
else
    usage();

if (sameWord(key, "acc"))
    justFirstWord = TRUE;
	
fofMake(argv+4, argc-4, outName, NULL, raNextRecord, key, dupeOk);
printf("Got %d records of which %d had key %s\n", recordCount, keyCount, key);
return 0;
}




