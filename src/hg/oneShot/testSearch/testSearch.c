/* testSearch - Set up a search program that does free text indexing and retrieval.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: testSearch.c,v 1.3 2005/12/21 04:38:55 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testSearch - Set up a search program that does free text indexing and retrieval.\n"
  "usage:\n"
  "   testSearch file word\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

#define prefixSize 5

static char unhexTable[128];	/* Lookup table to help with hex conversion. */

static void initUnhexTable()
{
int i;
unhexTable['0'] = 0;
unhexTable['1'] = 1;
unhexTable['2'] = 2;
unhexTable['3'] = 3;
unhexTable['4'] = 4;
unhexTable['5'] = 5;
unhexTable['6'] = 6;
unhexTable['7'] = 7;
unhexTable['8'] = 8;
unhexTable['9'] = 9;
unhexTable['A'] = 10;
unhexTable['B'] = 11;
unhexTable['C'] = 12;
unhexTable['D'] = 13;
unhexTable['E'] = 14;
unhexTable['F'] = 15;
}

static off_t unhex(char hex[10])
/* Convert 10 character hex string to off_t */
{
off_t  x = 0;
int i;
char c;

for (i=0; i<10; ++i)
   {
   x <<= 4;
   x += unhexTable[hex[i]];
   }
return x;
}

struct trixHitPos 
/* A hit to the index. */
    {
    struct trixHitPos *next;	/* Next in list */
    char *itemId;		/* Associated itemId */
    int docIx;			/* Which document this is part of. */
    int wordIx;			/* Which word this is part of. */
    };

struct trixWordResults
/* Results of a search on one word. */
    {
    struct trixWordResults *next;
    char *word;			/* Word name. */
    struct trixHitPos *hitList;	/* Hit list. */
    };

struct trixIxx
/* A prefix and */
    {
    off_t pos;	   /* Position where prefix first occurs in file. */
    char prefix[prefixSize];/* Space padded first five letters of what we're indexing. */
    };

struct trix
/* A two level index */
    {
    struct lineFile *lf;	/* Open file on first level index. */
    struct trixIxx *ixx;	/* Second level index in memory. */
    int ixxSize;		/* Size of second level index. */
    int ixxAlloc;	        /* Space allocated for index. */
    };

struct trix *trixNew()
/* Create a new empty trix index. */
{
struct trix *trix;
AllocVar(trix);
trix->ixxAlloc = 8*1024;
AllocArray(trix->ixx, trix->ixxAlloc);
return trix;
}

void trixAddToIxx(struct trix *trix, off_t pos, char *prefix)
/* Add to trix->ixx. */
{
struct trixIxx *ixx;
if (trix->ixxSize >= trix->ixxAlloc)
     {
     trix->ixxAlloc += trix->ixxAlloc;	/* Double allocation. */
     ExpandArray(trix->ixx, trix->ixxSize, trix->ixxAlloc);
     }
ixx = trix->ixx + trix->ixxSize;
ixx->pos = pos;
memcpy(ixx->prefix, prefix, sizeof(ixx->prefix));
trix->ixxSize += 1;
}

struct trix *trixOpen(char *ixFile)
/* Open up index.  Load second level index in memory. */
{
char ixxFile[PATH_LEN];
struct trix *trix;
struct lineFile *lf;
char *line;

initUnhexTable();
safef(ixxFile, sizeof(ixxFile), "%sx", ixFile);
lf = lineFileOpen(ixxFile, TRUE);
trix = trixNew();
while (lineFileNext(lf, &line, NULL))
    {
    off_t pos = unhex(line+prefixSize);
    trixAddToIxx(trix, pos, line);
    }
lineFileClose(&lf);
trix->lf = lineFileOpen(ixFile, TRUE);
return trix;
}

void trixCopyToPrefix(char *word, char *prefix)
/* Copy first part of word to prefix.  If need be end pad with spaces. */
{
int len = strlen(word);
if (len >= prefixSize)
    memcpy(prefix, word, prefixSize);
else
    {
    memset(prefix, ' ', prefixSize);
    memcpy(prefix, word, len);
    }
}

off_t trixFindIndexStartLine(struct trix *trix, char *word)
/* Find start position of line we want to start at in the first level
 * index. */
{
char wordPrefix[prefixSize];
int i;
off_t pos = 0;

trixCopyToPrefix(word, wordPrefix);
toLowerN(wordPrefix, prefixSize);
for (i=0; i<trix->ixxSize; ++i)
    {
    struct trixIxx *ixx = trix->ixx + i;
    if (memcmp(wordPrefix, ixx->prefix, prefixSize) < 0)
       break;
    pos = ixx->pos;
    }
return pos;
}

struct trixWordResults *trixWordResultsParse(char *hitWord, char *hitString)
/* This will create a trixWordsResult from hitWord and hitString.  It will
 * insert zeros in place of spaces in hit string while doing this. */
{
struct trixWordResults *twr;
struct trixHitPos *hit;
char *word;

AllocVar(twr);
twr->word = cloneString(hitWord);
while ((word = nextWord(&hitString)) != NULL)
    {
    char *parts[4];
    int partCount;
    partCount = chopByChar(word, ',', parts, ArraySize(parts));
    if (partCount != 3)
        errAbort("Error in index format at word %s", hitWord);
    AllocVar(hit);
    hit->itemId = cloneString(parts[0]);
    hit->docIx = atoi(parts[1]);
    hit->wordIx = atoi(parts[2]);
    slAddHead(&twr->hitList, hit);
    }
slReverse(&twr->hitList);
return twr;
}

struct trixWordResults *trixSearchWordResults(struct trix *trix, char *searchWord)
/* Get results for single word from index.  Returns NULL if no matches. */
{
char *line, *word;
off_t ixPos = trixFindIndexStartLine(trix, searchWord);
lineFileSeek(trix->lf, ixPos, SEEK_SET);
while (lineFileNext(trix->lf, &line, NULL))
    {
    int diff;
    word = nextWord(&line);
    diff = strcmp(searchWord, word);
    if (diff == 0)
	return trixWordResultsParse(word, line);
    else if (diff < 0)
	break;
    }
return NULL;
}


void testSearch(char *inFile, char *searchWord)
/* testSearch - Set up a search program that does free text indexing and retrieval.. */
{
struct trix *trix;
char ixFile[PATH_LEN];
struct trixWordResults *twr;
struct trixHitPos *hit;
int hitIx, maxHits = 8;

safef(ixFile, sizeof(ixFile), "%s.ix", inFile);
trix = trixOpen(ixFile);
twr = trixSearchWordResults(trix, searchWord);
if (twr == NULL)
    errAbort("%s not found", searchWord);
printf("%d matches to %s:", slCount(twr->hitList), searchWord);

for (hit=twr->hitList, hitIx=0; hit != NULL & hitIx < maxHits; hit=hit->next, hitIx+=1)
    printf(" %s", hit->itemId);
if (hit != NULL)
    printf(" ...");
printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
testSearch(argv[1], argv[2]);
return 0;
}
