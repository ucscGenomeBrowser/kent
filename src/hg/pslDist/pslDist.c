/* pslDist - distribute psl alignments into dirs by tName and qName. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "dystring.h"
#include "dlist.h"

#define MAXFILES	5000

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslDist - distribute psl alignments into dirs by tName and qName\n"
  "usage:\n"
  "   pslDist inPsl numDirs outRoot\n"
  " where outRoot is a directory\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct dirEntry
{
    int nextFile;
    struct hash *fileHash;
    char **fileNames;
};

struct lruEl
{
    FILE *stream;
};

FILE *getFileLRU(char *name)
{
static struct hash *nameHash = NULL;
struct dlNode *lruNode;
static struct dlList *lru = NULL;
struct lruEl *lruEl;

if (lru == NULL)
    lru = newDlList();

if (nameHash == NULL)
    nameHash = newHash(8);  

if ((lruNode = hashFindVal(nameHash, name)) == NULL)
    {
    AllocVar(lruNode);
    hashAdd(nameHash, name, lruNode);
    }
else
    dlRemove(lruNode);

dlAddHead(lru, lruNode);

if (lruNode->val == NULL)
    {
    AllocVar(lruEl);
    lruNode->val = (void *) lruEl;
    }
else
    lruEl = (struct lruEl *)lruNode->val;

if (lruEl->stream == NULL)
    {
    if ((lruEl->stream = fopen(name, "a")) == NULL)
	{
	struct dlNode *node = lru->tail;
	struct lruEl *lruElT;
	for(;node ; node=node->prev)
	    {
	    lruElT = (struct lruEl *)node->val;
	    if (lruElT->stream != NULL)
		{
		fclose(lruElT->stream);
		lruElT->stream = NULL;
		break;
		}
	    }
	lruEl->stream = mustOpen(name, "a");
	}
    }

return lruEl->stream;
}

char *getNextFilename(struct dirEntry *dirEntry, int maxFiles)
{
char buffer[128];

if (dirEntry->fileNames == NULL)
    dirEntry->fileNames = needMem(maxFiles * sizeof(char *));

if (dirEntry->nextFile == maxFiles)
    dirEntry->nextFile = 0;

if (dirEntry->fileNames[dirEntry->nextFile] == NULL)
    {
    sprintf(buffer, "x%d", dirEntry->nextFile);
    dirEntry->fileNames[dirEntry->nextFile] = cloneString(buffer);
    }
return dirEntry->fileNames[dirEntry->nextFile++];
}

void pslDist(char *inPsl, int numDirs, char *outRoot)
/* pslDist - distribute psl alignments into dirs by tName and qName. */
{
struct lineFile *pslLf = pslFileOpen(inPsl);
struct dyString *dy = dyStringNew(256);
struct dyString *dy2 = dyStringNew(256);
struct hash *dirHash = newHash(8);  
struct psl *psl;
char *file;
FILE *stream;
struct dirEntry *dirEntry;

while ((psl = pslNext(pslLf)) != NULL)
    {
    assert(psl->strand[1] != 0);
    dyStringClear(dy);
    dyStringPrintf(dy, "%s/%s%c",outRoot,psl->tName,  psl->strand[1]);
    dirEntry = hashFindVal(dirHash, dy->string);
    if (dirEntry == NULL)
	{
	AllocVar(dirEntry);
	dirEntry->fileHash = newHash(8);  
	hashAdd(dirHash, dy->string, dirEntry);
	if (mkdir(dy->string, 0777) < 0)
	    errAbort("cannot mkdir %s",dy->string);
	}

    file = hashFindVal(dirEntry->fileHash, psl->qName);
    if (file == NULL)
	{
	file = getNextFilename(dirEntry, numDirs);
	dyStringClear(dy2);
	dyStringPrintf(dy2, "%s/%s",dy->string, file);
	file = cloneString(dy2->string);
	hashAdd(dirEntry->fileHash, psl->qName, file);
	}

    stream = getFileLRU(file);

    pslTabOut(psl, stream);
    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int numFiles;

optionInit(&argc, argv, options);
if (argc != 4)
    usage();
numFiles = atoi(argv[2]);
if ((numFiles < 1) || (numFiles > MAXFILES))
    errAbort("# of files must be between 1 and %d", MAXFILES);
if (mkdir(argv[3], 0777) < 0)
    errAbort("cannot mkdir %s",argv[3]);
pslDist(argv[1], numFiles, argv[3]);
return 0;
}
