/* Quick lookup of cDNA based on binary search of index file, etc. */
#include "common.h"
#include "snof.h"

void *mustAlloc(int size)
{
void *ret;
ret = malloc(size);
if (ret == NULL)
    {
    fprintf(stderr, "Out of memory %d bytes\n", size);
    exit(-2);
    }
return ret;
}

void printTo(FILE *f, char untilC)
{
int c;

for (;;)
    {
    c = getc(f);
    if (c == EOF)
        break;
    if (c == untilC)
        {
        putchar(c);
        break;
        }
    }
for (;;)
    {
    c = getc(f);
    if (c == untilC || c == EOF)
	break;
    putchar(c);
    }
}

struct nameList
    {
    struct nameList *next;
    char *name;
    };

struct nameList *newNameList(char *name)
{
int len = strlen(name);
struct nameList *nl;
int allocSize = len+1+sizeof(*nl);
nl = mustAlloc(allocSize);
nl->next = NULL;
nl->name = (char *)(nl+1);
strcpy(nl->name, name);
return nl;
}

struct nameList *findCdnaNames(char *geneName, struct snof *snof, FILE *f)
{
char lineBuf[1024*2];
char *words[256];
int wordCount;
int i;
struct nameList *list = NULL;
long offset;

if (!snofFindOffset(snof, geneName, &offset))
    return NULL;
fseek(f, offset, SEEK_SET);
fgets(lineBuf, sizeof(lineBuf), f);
wordCount = chopString(lineBuf, whiteSpaceChopper, words, ArraySize(words));
assert(strcmp(words[0], geneName) == 0);
for (i=1; i<wordCount; ++i)
    {
    struct nameList *el;
    el = newNameList(words[i]);
    el->next = list;
    list = el;
    }
slReverse(&list);
return list;
}

int main(int argc, char *argv[])
{
FILE *faFile, *g2cFile, *inputFile;
long faOffset;
char *faName = "\\biodata\\celegans\\cDNA\\allcdna.fa";
char *faIndexName = "\\biodata\\celegans\\cDNA\\allcdna";
char *g2cName = "\\biodata\\celegans\\cDNA\\gene2cdna.txt";
char *g2cIndexName = "\\biodata\\celegans\\cDNA\\gene2cdna";
char *inputName;
struct snof *faSnof;
struct snof *g2cSnof;

if (argc != 2)
    {
    fprintf(stderr, "Usage: %s inputFile\n", argv[0]);
    exit(-1);
    }

/* Grab input file name and open file. */
inputName = argv[1];
inputFile = mustOpen(inputName, "r");

/* Open fa file (with actual sequence) */
faFile = mustOpen(faName, "rb");

/* Open file that has list of cDNA's and genes */
g2cFile = mustOpen(g2cName, "r");

/* Open and initialize index file. */
if ((faSnof = snofOpen(faIndexName)) == NULL)
    {
    fprintf(stderr, "%s isn't a good index file, sorry\n", faIndexName);
    exit(-1);
    }

if ((g2cSnof = snofOpen(g2cIndexName)) == NULL)
    {
    fprintf(stderr, "%s isn't a good index file, sorry\n", g2cIndexName);
    exit(-1);
    }

/* Loop through each line of input file.  Grab gene name from it.
 * Then scan through g2c file to find names of cDNAs that go with it. */
for (;;)
    {
    char lineBuf[256];
    char *words[16];
    int wordCount;
    if (fgets(lineBuf, sizeof(lineBuf), inputFile) == NULL)
	break;
    printf("%s", lineBuf);
    wordCount = chopString(lineBuf, whiteSpaceChopper, words, ArraySize(words));
    if (wordCount > 0)
	{
	char *geneName = words[0];
 	struct nameList *cdnaNames = findCdnaNames(geneName, g2cSnof, g2cFile);
	struct nameList *el;
	printf(">>Gene %s\n", geneName);
        if (cdnaNames == NULL)
            printf("No cDNA\n");
	for (el = cdnaNames; el != NULL; el = el->next)
	    {
            if (!snofFindOffset(faSnof, el->name, &faOffset))
		{
		fprintf(stderr, "Can't find cDNA %s\n", el->name);
		}
	    else
		{
		fseek(faFile, faOffset, SEEK_SET);
		printTo(faFile, '>');
		}
	    }
	printf("\n");
	}
    }
snofClose(&faSnof);
snofClose(&g2cSnof);
return 0;
}

