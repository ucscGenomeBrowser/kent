/* testSearch - Set up a search program that does free text indexing and retrieval.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: testSearch.c,v 1.5 2005/12/21 21:26:37 kent Exp $";

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

struct trixWordResult
/* Results of a search on one word. */
    {
    struct trixWordResult *next;
    char *word;			/* Word name. */
    struct trixHitPos *hitList;	/* Hit list. */
    struct trixHitPos *hit;	/* Current position while iterating through hit list. */
    struct trixHitPos *iHit;	/* Current position during an inner iteration. */
    };

struct trixIxx
/* A prefix and */
    {
    off_t pos;	   /* Position where prefix first occurs in file. */
    char prefix[prefixSize];/* Space padded first five letters of what we're indexing. */
    };

struct trixSearchResult
/* Result of a trix search. */
    {
    struct trixSearchResult *next;
    char *itemId;		/* ID of matching item */
    bool inSameDoc;		/* True if have matches all in same doc. */
    int unorderedSpan;		/* Minimum span in single doc with words in any order. */
    int orderedSpan;		/* Minimum span in single doc with words in search order. */
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

struct trixWordResult *trixWordResultsParse(char *hitWord, char *hitString)
/* This will create a trixWordsResult from hitWord and hitString.  It will
 * insert zeros in place of spaces in hit string while doing this. */
{
struct trixWordResult *twr;
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

struct trixWordResult *trixSearchWordResults(struct trix *trix, char *searchWord)
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


void trwDump(struct trixWordResult *twr)
/* Dump out one trixWordResult to stdout. */
{
struct trixHitPos *hit;
int hitIx, maxHits = 8;

printf("%d matches to %s:", slCount(twr->hitList), twr->word);
for (hit=twr->hitList, hitIx=0; hit != NULL & hitIx < maxHits; hit=hit->next, hitIx+=1)
    printf(" %s", hit->itemId);
if (hit != NULL)
    printf(" ...");
printf("\n");
}

static char *highestId(struct trixWordResult *twrList)
/* Return highest itemId at current twr->hit */
{
char *itemId = twrList->hit->itemId;
struct trixWordResult *twr;

for (twr = twrList->next; twr != NULL; twr = twr->next)
    {
    if (strcmp(itemId, twr->hit->itemId) < 0)
        itemId = twr->hit->itemId;
    }
return itemId;
}

static boolean seekOneToId(struct trixWordResult *twr, char *itemId)
/* Move twr->hit forward until it hits itemId.  Return FALSE if
 * moved past where itemId would be without hitting it. */
{
struct trixHitPos *hit;
int diff = -1;
for (hit = twr->hit; hit != NULL; hit = hit->next)
    {
    diff = strcmp(itemId, hit->itemId);
    if (diff <= 0)
        break;
    }
twr->hit = hit;
return diff == 0;
}

static boolean seekAllToId(struct trixWordResult *twrList, char *itemId)
/* Try to seek all twr's in list to the same itemId */
{
struct trixWordResult *twr;
boolean allHit = TRUE;
for (twr = twrList; twr != NULL; twr = twr->next)
    {
    if (!seekOneToId(twr, itemId))
        allHit = FALSE;
    }
return allHit;
}

static void seekAllPastId(struct trixWordResult *twrList, char *itemId)
/* Try to seek all twr's in list to past the itemId. */
{
struct trixWordResult *twr;
for (twr = twrList; twr != NULL; twr = twr->next)
    {
    struct trixHitPos *hit;
    for (hit = twr->hit; hit != NULL; hit = hit->next)
	if (!sameString(hit->itemId, itemId))
	    break;
    twr->hit = hit;
    }
}

static boolean anyTwrDone(struct trixWordResult *twrList)
/* Return TRUE if any of the items in list are done */
{
struct trixWordResult *twr;
for (twr = twrList; twr != NULL; twr = twr->next)
    if (twr->hit == NULL)
        return TRUE;
return FALSE;
}

static int highestDoc(struct trixWordResult *twrList)
/* Return highest docIx at current twr->hit */
{
int docIx = twrList->hit->docIx;
struct trixWordResult *twr;

for (twr = twrList->next; twr != NULL; twr = twr->next)
    {
    if (docIx < twr->hit->docIx)
        docIx = twr->hit->docIx;
    }
return docIx;
}

static boolean seekOneToDoc(struct trixWordResult *twr, char *itemId, int docIx)
/* Move twr->hit forward until it hits docIx.  Return FALSE if
 * moved past where docIx would be without hitting it.  Won't seek
 * on to another itemId*/
{
struct trixHitPos *hit;
int diff = -1;
boolean foundIt = FALSE;
for (hit = twr->hit; hit != NULL; hit = hit->next)
    {
    if (!sameString(hit->itemId, itemId))
	break;
    if (docIx == hit->docIx)
        {
	foundIt = TRUE;
	break;
	}
    if (docIx < hit->docIx)
	break;
    }
twr->hit = hit;
return foundIt;
}

static boolean seekAllToDoc(struct trixWordResult *twrList, char *itemId, 
	int docIx)
/* Try to seek all twr's in list to the same docIx.  Don't seek them
 * to another docId. */
{
struct trixWordResult *twr;
boolean allHit = TRUE;
for (twr = twrList; twr != NULL; twr = twr->next)
    {
    if (!seekOneToDoc(twr, itemId, docIx))
        allHit = FALSE;
    }
return allHit;
}

static void seekAllPastDoc(struct trixWordResult *twrList, char *itemId, int docIx)
/* Try to seek all twr's in list to past the docIx. */
{
struct trixWordResult *twr;
for (twr = twrList; twr != NULL; twr = twr->next)
    {
    struct trixHitPos *hit;
    for (hit = twr->hit; hit != NULL; hit = hit->next)
	{
	if (!sameString(hit->itemId, itemId))
	    break;
	if (hit->docIx != docIx)
	    break;
	}
    twr->hit = hit;
    }
}

static boolean anyPastItem(struct trixWordResult *twrList, char *itemId)
/* Return TRUE if any of the items in list are done */
{
struct trixWordResult *twr;
for (twr = twrList; twr != NULL; twr = twr->next)
    {
    if (twr->hit == NULL)
        return TRUE;
    if (!sameString(itemId, twr->hit->itemId))
        return TRUE;
    }
return FALSE;
}


static int findUnorderedSpan(struct trixWordResult *twrList,
	char *itemId, int docIx)
/* Find out smallest number of words in doc that will cover
 * all words in search. */
{
int minSpan = BIGNUM;
struct trixWordResult *twr;

/* Set up iHit pointers we use to keep track of our 
 * search.  Don't want to mess with hit pointers as they
 * will be used later. */
for (twr = twrList; twr != NULL; twr = twr->next)
    twr->iHit = twr->hit;

for (;;)
    {
    int minWord = BIGNUM, maxWord=0, span;

    /* Figure out current span and save as min if it's smallest so far. */
    for (twr = twrList; twr != NULL; twr = twr->next)
        {
	int curWord = twr->iHit->wordIx;
	if (curWord < minWord)
	    minWord = curWord;
	if (curWord > maxWord)
	    maxWord = curWord;
	}
    span = maxWord - minWord;
    if (span < minSpan)
        minSpan = span;

    /* Advance iHit past minWord.  Break if we go outside of our doc or item. */
    for (twr = twrList; twr != NULL; twr = twr->next)
        {
	if (twr->iHit->wordIx == minWord)
	    {
	    struct trixHitPos *hit = twr->iHit = twr->iHit->next;
	    if (hit == NULL || hit->docIx != docIx || !sameString(hit->itemId, itemId))
	        {
		return minSpan+1;
		}
	    }
	}
    }
}

static int findOrderedSpan(struct trixWordResult *twrList,
	char *itemId, int docIx)
/* Find out smallest number of words in doc that will cover
 * all words in search. */
{
int minSpan = BIGNUM;
struct trixWordResult *twr;

/* Set up iHit pointers we use to keep track of our 
 * search.  Don't want to mess with hit pointers as they
 * will be used later. */
for (twr = twrList; twr != NULL; twr = twr->next)
    twr->iHit = twr->hit;

for (;;)
    {
    int startWord = twrList->iHit->wordIx;
    int endWord = startWord;
    int span;
    struct trixHitPos *hit;

    /* Set up twr->iHit to be closest one past hit of previous twr. */
    for (twr = twrList->next; twr != NULL; twr = twr->next)
        {
	for (hit = twr->iHit; ; hit = hit->next)
	    {
	    if (hit == NULL || hit->docIx != docIx || !sameString(hit->itemId, itemId))
	        return minSpan;
	    if (hit->wordIx > endWord)
	        break;
	    }
	twr->iHit = hit;
	endWord = hit->wordIx;
	}
    span = endWord - startWord;
    if (span < minSpan)
        minSpan = span;

    /* Advance to next occurence of first word. */
    hit = twrList->iHit = twrList->iHit->next;
    if (hit == NULL || hit->docIx != docIx || !sameString(hit->itemId, itemId))
	return minSpan+1;
    }
}


static void fillInDocAndSpans(struct trixSearchResult *ts, 
	struct trixWordResult *twrList, char *itemId)
/* Fill in the inSameDoc, orderedSpan, and unorderedSpan fields of ts. */
{
for (;;)
    {
    int docIx = highestDoc(twrList);
    if (seekAllToDoc(twrList, itemId, docIx))
        {
	ts->inSameDoc = TRUE;
	ts->unorderedSpan = findUnorderedSpan(twrList, itemId, docIx);
	ts->orderedSpan = findOrderedSpan(twrList, itemId, docIx);
	}
    seekAllPastDoc(twrList, itemId, docIx);
    if (anyPastItem(twrList, itemId))
        break;
    }
}

static struct trixSearchResult *findMultipleWordHits(struct trixWordResult *twrList)
/* Return list of items that are hit by all words. */
{
struct trixWordResult *twr;
struct trixSearchResult *tsList = NULL, *ts;

/* Initially set hit position to start on all words. */
for (twr = twrList; twr != NULL; twr = twr->next)
    twr->hit = twr->hitList;

for (;;)
    {
    char *itemId = highestId(twrList);
    if (seekAllToId(twrList, itemId))
        {
	AllocVar(ts);
	ts->itemId = cloneString(itemId);
	fillInDocAndSpans(ts, twrList, itemId);
	slAddHead(&tsList, ts);
	}
    seekAllPastId(twrList, itemId);
    if (anyTwrDone(twrList))
        break;
    }
slReverse(&tsList);
return tsList;
}

int trixSearchResultCmp(const void *va, const void *vb)
/* Compare two trixSearchResult by itemId. */
{
const struct trixSearchResult *a = *((struct trixSearchResult **)va);
const struct trixSearchResult *b = *((struct trixSearchResult **)vb);
int dif;
dif = b->inSameDoc - a->inSameDoc;
if (dif == 0)
   {
   dif = a->unorderedSpan - b->unorderedSpan;
   if (dif == 0)
       dif = a->orderedSpan - b->orderedSpan;
   }
return dif;
}

struct trixSearchResult *trixSearch(struct trix *trix, int wordCount, char **words)
/* Return a list of items that match all words. */
{
struct trixWordResult *twr, *twrList = NULL;
struct trixSearchResult *ts, *tsList = NULL;
int wordIx;
boolean gotMiss = FALSE;

if (wordCount == 1)
    {
    struct trixHitPos *hit;
    twr = trixSearchWordResults(trix, words[0]);
    if (twr == NULL)
        return NULL;
    for (hit = twr->hitList; hit != NULL; hit = hit->next)
        {
	AllocVar(ts);
	ts->itemId = hit->itemId;	/* Transfer itemId */
	hit->itemId = NULL;
	ts->inSameDoc = TRUE;
	ts->orderedSpan = 1;
	ts->unorderedSpan = 1;
	slAddHead(&tsList, ts);
	}
    }
else
    {
    for (wordIx=0; wordIx<wordCount; ++wordIx)
	{
	char *searchWord = words[wordIx];
	twr = trixSearchWordResults(trix, searchWord);
	if (twr == NULL)
	    {
	    gotMiss = TRUE;
	    break;
	    }
	slAddHead(&twrList, twr);
	trwDump(twr);
	}
    if (!gotMiss)
	{
	slReverse(&twrList);
	tsList = findMultipleWordHits(twrList);
	}
    }
slSort(&tsList, trixSearchResultCmp);
return tsList;
}

void dumpTsList(struct trixSearchResult *tsList)
/* Dump out contents of tsList to stdout. */
{
struct trixSearchResult *ts;
int maxIx=40, ix=0;
printf("%d match:", slCount(tsList));
for (ts = tsList, ix=0; ts != NULL && ix<maxIx; ts = ts->next, ++ix)
    printf(" %s,%d,%d,%d", ts->itemId, ts->inSameDoc, 
    	ts->unorderedSpan, ts->orderedSpan);
if (ts != NULL)
    printf(" ...");
printf("\n");
}

void testSearch(char *inFile, int wordCount, char *words[])
/* testSearch - Set up a search program that does free text indexing and retrieval.. */
{
struct trix *trix;
char ixFile[PATH_LEN];
struct trixWordResult *twr;
struct trixHitPos *hit;
struct trixWordResult *twrList = NULL;
struct trixSearchResult *tsList, *ts;
int i;

safef(ixFile, sizeof(ixFile), "%s.ix", inFile);
for (i=0; i<wordCount; ++i)
    tolowers(words[i]);
trix = trixOpen(ixFile);
tsList = trixSearch(trix, wordCount, words);
dumpTsList(tsList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
testSearch(argv[1], argc-2, argv+2);
return 0;
}
