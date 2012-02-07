/* ppSanger - Analyse paired plasmid reads from Sanger and make pairs file.. */
#include "common.h"
#include "jksql.h"
#include "hash.h"
#include "linefile.h"
#include "sangInsert.h"
#include "sangRange.h"
#include "sangRead.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ppSanger - Analyse paired plasmid reads from Sanger and make pairs file.\n"
  "usage:\n"
  "   ppSanger databaseDir sanger.pair\n");
}

struct sangPair
/* Info on a pair of sanger reads. */
    {
    struct sangPair *next;	/* Next in list. */
    char *name;                 /* Pair name (ascii number) */
    struct sangRange *range;    /* Range of sizes for insert. */
    struct sangRead *fRead;     /* The forward side of read. */
    struct sangRead *rRead;     /* The reverse side of read. */
    };

struct sangRange *readRanges(char *fileName, struct hash *hash)
/* Read range file into list/hash. */
{
struct sangRange *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[3];
int wordCount;

printf("Reading %s\n", fileName);
while (lineFileNextRow(lf, words, 3))
    {
    el = sangRangeLoad(words);
    slAddHead(&list, el);
    hashAddUnique(hash, el->name, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct sangPair *readPairs(char *fileName, struct hash *pairHash, struct hash *rangeHash)
/* Read in pair file and connect pairs to relevant range. */
{
struct sangPair *list = NULL, *el;
struct hashEl *hel;
struct sangInsert si;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[2];
int wordCount;

printf("Reading %s\n", fileName);
while (lineFileNextRow(lf, words, 2))
    {
    sangInsertStaticLoad(words, &si);
    AllocVar(el);
    hel = hashAddUnique(pairHash, si.id, el);
    el->name = hel->name;
    el->range = hashMustFindVal(rangeHash, si.name);
    slAddHead(&list, el);
    }
slReverse(&list);
lineFileClose(&lf);
return list;
}

struct sangRead *readReads(char *fileName, struct hash *pairHash)
/* Read in read database file and hook it up to pairs in pairHash. */
{
struct sangRead *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[4];
int wordCount;
struct sangPair *pair;

printf("Reading %s\n", fileName);
while (lineFileNextRow(lf, words, 4))
    {
    el = sangReadLoad(words);
    slAddHead(&list, el);
    pair = hashMustFindVal(pairHash, el->id);
    if (el->pq[0] == 'p')
	{
	if (pair->fRead)
	    warn("%s - duplicate p read line %d of %s\n", el->id, lf->lineIx, lf->fileName);
	pair->fRead = el;
	}
    else
	{
	if (pair->rRead)
	    warn("%s - duplicate q read line %d of %s\n", el->id, lf->lineIx, lf->fileName);
        pair->rRead = el;
	}
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void writePairs(struct sangPair *pairList, char *fileName)
/* Write out pairs to file. */
{
FILE *f = mustOpen(fileName, "w");
struct sangPair *pair;

printf("Writing %d pairs to %s\n", slCount(pairList), fileName);
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (pair->fRead && pair->rRead)
	{
	fprintf(f, "%s\t%s\t%s\t%d\t%d\n",
	    pair->fRead->name, pair->rRead->name, pair->name,
	    round(0.9*pair->range->minSize), round(1.1*pair->range->maxSize));
	}
    }
fclose(f);
}

void ppSanger(char *databaseDir, char *pairFileName)
/* ppSanger - Analyse paired plasmid reads from Sanger and make pairs file.. */
{
char readFile[512], insertFile[512], rangeFile[512];
struct hash *rangeHash = newHash(7);
struct hash *pairHash = newHash(19);
struct sangPair *pairList, *pair;
struct sangRange *rangeList, *range;
struct sangRead *readList, *read;

sprintf(readFile, "%s/TBL_INSERT_FILE", databaseDir);
sprintf(insertFile, "%s/TBL_INSERT_LENGTH", databaseDir);
sprintf(rangeFile, "%s/TBL_LENGTH_RANGE", databaseDir);

rangeList = readRanges(rangeFile, rangeHash);
pairList = readPairs(insertFile, pairHash, rangeHash);
readList = readReads(readFile, pairHash);
writePairs(pairList, pairFileName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
ppSanger(argv[1], argv[2]);
return 0;
}
