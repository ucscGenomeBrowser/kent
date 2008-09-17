/* trix - text retrieval index.  Stuff for fast two level index
 * of text for fast word searches. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "trix.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: trix.c,v 1.6 2008/09/17 17:56:38 kent Exp $";

/* Some local structures for the search. */
struct trixHitPos 
/* A hit to the index. */
    {
    struct trixHitPos *next;	/* Next in list */
    char *itemId;		/* Associated itemId */
    int wordIx;			/* Which word this is part of. */
    int leftoverLetters;	/* Number of letters at end of word not matched */
    };

struct trixWordResult
/* Results of a search on one word. */
    {
    struct trixWordResult *next;
    char *word;			/* Word name. */
    struct trixHitPos *hitList;	/* Hit list.  May be shared (not allocated here). */
    struct trixHitPos *hit;	/* Current position while iterating through hit list. */
    struct trixHitPos *iHit;	/* Current position during an inner iteration. */
    };

struct trixIxx
/* A prefix and */
    {
    off_t pos;	   /* Position where prefix first occurs in file. */
    char prefix[trixPrefixSize];/* Space padded first five letters of what we're indexing. */
    };

/* Some cleanup code. */

static void trixHitPosFree(struct trixHitPos **pPos)
/* Free up trixHitPos. */
{
struct trixHitPos *pos = *pPos;
if (pos != NULL)
    {
    freeMem(pos->itemId);
    freez(pPos);
    }
}

static void trixHitPosFreeList(struct trixHitPos **pList)
/* Free up a list of trixHitPoss. */
{
struct trixHitPos *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    trixHitPosFree(&el);
    }
*pList = NULL;
}

static void trixWordResultFree(struct trixWordResult **pTwr)
/* Free up a trixWordResult */
{
struct trixWordResult *twr = *pTwr;
if (twr != NULL)
    {
    freeMem(twr->word);
    freez(pTwr);
    }
}

static void trixWordResultFreeList(struct trixWordResult **pList)
/* Free up a list of trixWordResults. */
{
struct trixWordResult *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    trixWordResultFree(&el);
    }
*pList = NULL;
}

static void freeHitCallback(void *val)
/* Val is actually list trixHitPos.  This called to free stuff in hash. */
{
struct trixHitPos *posList = val;
trixHitPosFreeList(&posList);
}

void trixClose(struct trix **pTrix)
/* Close up index and free up associated resources. */
{
struct trix *trix = *pTrix;
if (trix != NULL)
    {
    freeMem(trix->ixx);
    hashTraverseVals(trix->wordHitHash, freeHitCallback);
    hashFree(&trix->wordHitHash);	/* Need to free items? */
    freez(pTrix);
    }
}

void trixSearchResultFree(struct trixSearchResult **pTsr)
/* Free up data associated with trixSearchResult. */
{
struct trixSearchResult *tsr = *pTsr;
if (tsr != NULL)
    {
    freeMem(tsr->itemId);
    freez(pTsr);
    }
}

void trixSearchResultFreeList(struct trixSearchResult **pList)
/* Free up a list of trixSearchResults. */
{
struct trixSearchResult *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    trixSearchResultFree(&el);
    }
*pList = NULL;
}


/* Code that makes, rather than cleans up. */

static char unhexTable[128];	/* Lookup table to help with hex conversion. */

static void initUnhexTable()
/* Initialize a table for fast unhexing of numbers. */
{
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

for (i=0; i<10; ++i)
   {
   x <<= 4;
   x += unhexTable[(unsigned)hex[i]];
   }
return x;
}

struct trix *trixNew()
/* Create a new empty trix index. */
{
struct trix *trix;
AllocVar(trix);
trix->ixxAlloc = 8*1024;
AllocArray(trix->ixx, trix->ixxAlloc);
trix->wordHitHash = newHash(8);
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
    off_t pos = unhex(line+trixPrefixSize);
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
if (len >= trixPrefixSize)
    memcpy(prefix, word, trixPrefixSize);
else
    {
    memset(prefix, ' ', trixPrefixSize);
    memcpy(prefix, word, len);
    }
}

static off_t trixFindIndexStartLine(struct trix *trix, char *word)
/* Find start position of line we want to start at in the first level
 * index. */
{
char wordPrefix[trixPrefixSize];
int i;
off_t pos = 0;

trixCopyToPrefix(word, wordPrefix);
toLowerN(wordPrefix, trixPrefixSize);
for (i=0; i<trix->ixxSize; ++i)
    {
    struct trixIxx *ixx = trix->ixx + i;
    if (memcmp(wordPrefix, ixx->prefix, trixPrefixSize) < 0)
       break;
    pos = ixx->pos;
    }
return pos;
}

static struct trixHitPos *trixParseHitList(char *hitWord, char *hitString,
	int leftoverLetters)
/* Parse out hit string, inserting zeroes in it during process.
 * Return result as list of trixHitPos. */
{
struct trixHitPos *hit, *hitList = NULL;
char *word;
while ((word = nextWord(&hitString)) != NULL)
    {
    char *parts[3];
    int partCount;
    partCount = chopByChar(word, ',', parts, ArraySize(parts));
    if (partCount != 2)
        errAbort("Error in index format at word %s", hitWord);
    AllocVar(hit);
    hit->itemId = cloneString(parts[0]);
    hit->wordIx = sqlUnsigned(parts[1]);
    hit->leftoverLetters = leftoverLetters;
    slAddHead(&hitList, hit);
    }
slReverse(&hitList);
return hitList;
}

int trixHitPosCmp(struct trixHitPos *a, struct trixHitPos *b)
/* Compare function to sort trixHitPos. */
{
int diff = strcmp(a->itemId, b->itemId);
if (diff == 0)
    {
    diff = a->wordIx - b->wordIx;
    if (diff == 0)
        diff = a->leftoverLetters - b->leftoverLetters;
    }
return diff;
}


struct trixHitPos *mergeHits(struct trixHitPos *aList, struct trixHitPos *bList)
/* Return hit list that merges aList and bList.  The input is sorted,
 * and so is the output. */
{
struct trixHitPos *a, *b, *aNext, *bNext, *newList = NULL;

a = aList;
b = bList;
for (;;)
    {
    if (a == NULL)
        {
	if (b == NULL)
	    break;
	bNext = b->next;
	slAddHead(&newList, b);
	b = bNext;
	}
    else if (b == NULL)
        {
	aNext = a->next;
	slAddHead(&newList, a);
	a = aNext;
	}
    else if (trixHitPosCmp(a, b) < 0)
        {
	aNext = a->next;
	slAddHead(&newList, a);
	a = aNext;
	}
    else
        {
	bNext = b->next;
	slAddHead(&newList, b);
	b = bNext;
	}
    }
slReverse(&newList);
return newList;
}

static int reasonablePrefix(char *prefix, char *word, boolean expand)
/* Return non-negative if prefix is reasonable for word.
 * Returns number of letters left in word not matched by
 * prefix. */
{
int prefixLen = strlen(prefix);
int wordLen = strlen(word);
int suffixLen = wordLen - prefixLen;
if (suffixLen == 0)
   return 0;
else if (expand && prefixLen >= 3)
    {
    int wordEnd;
    char *suffix = word + prefixLen;
    boolean prefixEndsInDigit = isdigit(word[prefixLen-1]);
    /* Find a word marker - either end of string, '-', '.', or '_'
     * or a number. */
    for (wordEnd=0; wordEnd < suffixLen; ++wordEnd)
        {
	char c = suffix[wordEnd];
	if (c == '-' || c == '.' || c == '_' || (!prefixEndsInDigit && isdigit(c)))
	    break;
	}
    if (wordEnd <= 2)
       return wordEnd;
    if (wordEnd == 3 && startsWith("ing", suffix))
       return wordEnd;
    }
return -1;
}


struct trixWordResult *trixSearchWordResults(struct trix *trix, 
	char *searchWord, boolean expand)
/* Get results for single word from index.  Returns NULL if no matches. */
{
char *line, *word;
struct trixWordResult *twr = NULL;
struct trixHitPos *hitList = hashFindVal(trix->wordHitHash, searchWord);

if (hitList == NULL)
    {
    struct trixHitPos *oneHitList;
    off_t ixPos = trixFindIndexStartLine(trix, searchWord);
    lineFileSeek(trix->lf, ixPos, SEEK_SET);
    while (lineFileNext(trix->lf, &line, NULL))
	{
	word = nextWord(&line);
	if (startsWith(searchWord, word))
	    {
	    int leftoverLetters = reasonablePrefix(searchWord, word, expand);
	    /* uglyf("reasonablePrefix(%s,%s)=%d<BR>\n", searchWord, word, leftoverLetters); */
	    if (leftoverLetters >= 0)
		{
		oneHitList = trixParseHitList(searchWord, line, 
			leftoverLetters);
		hitList = mergeHits(hitList, oneHitList);
		}
	    }
	else if (strcmp(searchWord, word) < 0)
	    break;
	}
    hashAdd(trix->wordHitHash, searchWord, hitList);
    }
if (hitList != NULL)
    {
    AllocVar(twr);
    twr->word = cloneString(searchWord);
    twr->hitList = hitList;
    }
return twr;
}


#ifdef DEBUG
void trwDump(struct trixWordResult *twr)
/* Dump out one trixWordResult to stdout. */
{
struct trixHitPos *hit;
int hitIx, maxHits = 8;

printf("%d matches to %s:", slCount(twr->hitList), twr->word);
for (hit=twr->hitList, hitIx=0; hit != NULL && hitIx < maxHits; hit=hit->next, hitIx+=1)
    printf(" %s@%d", hit->itemId, hit->wordIx);
if (hit != NULL)
    printf(" ...");
printf("<BR>\n");
}
#endif /* DEBUG */

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

static void findUnorderedSpan(struct trixWordResult *twrList,
	char *itemId, int *retSpan, int *retLeftoverLetters)
/* Find out smallest number of words in doc that will cover
 * all words in search. */
{
int minSpan = BIGNUM;
int leftoverLetters = 0;
struct trixWordResult *twr;

/* Set up iHit pointers we use to keep track of our 
 * search.  Don't want to mess with hit pointers as they
 * will be used later. */
for (twr = twrList; twr != NULL; twr = twr->next)
    twr->iHit = twr->hit;

for (;;)
    {
    int minWord = BIGNUM, maxWord=0, span;
    int curLeftover = 0;

    /* Figure out current span and save as min if it's smallest so far. */
    for (twr = twrList; twr != NULL; twr = twr->next)
        {
	int curWord = twr->iHit->wordIx;
	if (curWord < minWord)
	    minWord = curWord;
	if (curWord > maxWord)
	    maxWord = curWord;
	curLeftover += twr->iHit->leftoverLetters;
	}
    span = maxWord - minWord;
    if (span < minSpan)
	{
        minSpan = span;
	leftoverLetters = curLeftover;
	}

    /* Advance iHit past minWord.  Break if we go outside of our doc or item. */
    for (twr = twrList; twr != NULL; twr = twr->next)
        {
	if (twr->iHit->wordIx == minWord)
	    {
	    struct trixHitPos *hit = twr->iHit = twr->iHit->next;
	    if (hit == NULL || !sameString(hit->itemId, itemId))
	        {
		*retSpan = minSpan+1;
		*retLeftoverLetters = leftoverLetters;
		return;
		}
	    }
	}
    }
} 

static int findWordPos(struct trixWordResult *twrList, char *itemId)
/* Figure out the first word position.  For multiple words, this
 * will be the maximimum first word position of all words. 
 * This assumes that the hits are sorted by word position
 * within a document. */
{
int firstWordPos = 0;
struct trixWordResult *twr;
for (twr = twrList; twr != NULL; twr = twr->next)
    {
    int pos = twr->hit->wordIx;
    if (firstWordPos < pos)
        firstWordPos = pos;
    }
return firstWordPos;
}

static int findOrderedSpan(struct trixWordResult *twrList,
	char *itemId)
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
	    if (hit == NULL || !sameString(hit->itemId, itemId))
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
    if (hit == NULL || !sameString(hit->itemId, itemId))
	return minSpan+1;
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
	findUnorderedSpan(twrList, itemId, 
		&ts->unorderedSpan, &ts->leftoverLetters);
	ts->orderedSpan = findOrderedSpan(twrList, itemId);
	ts->wordPos = findWordPos(twrList, itemId);
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
dif = a->unorderedSpan - b->unorderedSpan;
if (dif == 0)
   {
   dif = a->orderedSpan - b->orderedSpan;
   if (dif == 0)
       {
       dif = a->leftoverLetters - b->leftoverLetters;
       if (dif == 0)
	   dif = a->wordPos - b->wordPos;
       }
   }
       
return dif;
}

struct trixSearchResult *trixSearch(struct trix *trix, int wordCount, char **words,
	boolean expand)
/* Return a list of items that match all words.  This will be sorted so that
 * multiple-word matches where the words are closer to each other and in the
 * right order will be first.  Do a trixSearchResultFreeList when done. 
 * If expand is TRUE then this will match not only the input words, but also
 * additional words that start with the input words. */
{
struct trixWordResult *twr, *twrList = NULL;
struct trixSearchResult *ts, *tsList = NULL;
int wordIx;
boolean gotMiss = FALSE;

if (wordCount == 1)
    {
    struct trixHitPos *hit;
    char *lastId = "";
    twr = twrList = trixSearchWordResults(trix, words[0], expand);
    if (twr == NULL)
        return NULL;
    for (hit = twr->hitList; hit != NULL; hit = hit->next)
        {
	if (!sameString(lastId, hit->itemId))
	    {
	    lastId = hit->itemId;
	    AllocVar(ts);
	    ts->itemId = hit->itemId;	/* Transfer itemId */
	    hit->itemId = NULL;
	    ts->orderedSpan = 1;
	    ts->unorderedSpan = 1;
	    ts->wordPos = hit->wordIx;
	    ts->leftoverLetters = hit->leftoverLetters;
	    slAddHead(&tsList, ts);
	    }
	}
    }
else
    {
    for (wordIx=0; wordIx<wordCount; ++wordIx)
	{
	char *searchWord = words[wordIx];
	twr = trixSearchWordResults(trix, searchWord, expand);
	if (twr == NULL)
	    {
	    gotMiss = TRUE;
	    break;
	    }
	slAddHead(&twrList, twr);
#ifdef DEBUG
	trwDump(twr);
#endif /* DEBUG */
	}
    if (!gotMiss)
	{
	slReverse(&twrList);
	tsList = findMultipleWordHits(twrList);
	}
    }
trixWordResultFreeList(&twrList);
slSort(&tsList, trixSearchResultCmp);
return tsList;
}


