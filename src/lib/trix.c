/* trix - text retrieval index.  Stuff for fast two level index
 * of text for fast word searches. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "trix.h"
#include "sqlNum.h"
#include "udc.h"
#include "net.h"
#include "rangeTree.h"
#include "portable.h"
#include "errCatch.h"

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

int trixPrefixSize = 5;
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
AllocVar(ixx->prefix);
memcpy(ixx->prefix, prefix, trixPrefixSize);
trix->ixxSize += 1;
}

// wrappers around the udc or lineFile routines
static void *ourOpen(struct trix *trix, char *fileName)
{
if (trix->useUdc)
    return (void *)udcFileOpen(fileName, NULL);
return (void *)lineFileOpen(fileName, TRUE);
}

static boolean ourReadLine(struct trix *trix, void *lf, char **line)
{
if (trix->useUdc)
    {
    *line = udcReadLine((struct udcFile *)lf);
    return *line != NULL;
    }
return lineFileNext((struct lineFile *)lf, line, NULL);
}

static void ourClose(struct trix *trix, void **lf)
{
if (trix->useUdc)
    udcFileClose((struct udcFile **)lf);
else
    lineFileClose((struct lineFile **)lf);
}

void ourSeek(struct trix *trix, struct lineFile *lf, off_t ixPos)
{
if (trix->useUdc)
    udcSeek((struct udcFile *)lf, ixPos);
else
    lineFileSeek((struct lineFile *)lf, ixPos, SEEK_SET);
}

struct trix *trixOpen(char *ixFile)
/* Open up index.  Load second level index in memory. */
{
struct trix *trix = trixNew();
trix->useUdc = FALSE;
if (hasProtocol(ixFile))
    trix->useUdc = TRUE;

char ixxFile[PATH_LEN];
void *lf;
char *line;

initUnhexTable();
safef(ixxFile, sizeof(ixxFile), "%sx", ixFile);
lf = ourOpen(trix, ixxFile);
while (ourReadLine(trix, lf, &line) )
    {
    off_t pos = unhex(line+trixPrefixSize);
    trixAddToIxx(trix, pos, line);
    }
ourClose(trix, &lf);
trix->lf = ourOpen(trix, ixFile);
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

static off_t trixFindIndexStartLine(struct trixIxx *trixIxx, int ixxSize, char *word)
/* Find start position of line we want to start at in the first level
 * index. */
{
char wordPrefix[trixPrefixSize];
int i;
off_t pos = 0;

trixCopyToPrefix(word, wordPrefix);
toLowerN(wordPrefix, trixPrefixSize);
for (i=0; i < ixxSize; ++i)
    {
    struct trixIxx *ixx = trixIxx + i;
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

int trixHitPosCmp(const void *a, const void *b)
/* Compare function to sort trixHitPos. */
{
struct trixHitPos *aHitPos = *((struct trixHitPos **)a);
struct trixHitPos *bHitPos = *((struct trixHitPos **)b);
int diff = strcmp(aHitPos->itemId, bHitPos->itemId);
if (diff == 0)
    {
    diff = aHitPos->wordIx - bHitPos->wordIx;
    if (diff == 0)
        diff = aHitPos->leftoverLetters - bHitPos->leftoverLetters;
    }
return diff;
}

static int reasonablePrefix(char *prefix, char *word, enum trixSearchMode mode)
/* Return non-negative if prefix is reasonable for word.
 * Returns number of letters left in word not matched by
 * prefix. */
{
int prefixLen = strlen(prefix);
int wordLen = strlen(word);
int suffixLen = wordLen - prefixLen;
if (suffixLen == 0)
   return 0;
else if ((mode == tsmExpand && prefixLen >= 3) ||
         (mode == tsmFirstFive && prefixLen >= 5))
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
    if (mode == tsmFirstFive && prefixLen >= 5)
        return wordEnd;
    }
return -1;
}


struct trixWordResult *trixSearchWordResults(struct trix *trix, 
	char *searchWord, enum trixSearchMode mode)
/* Get results for single word from index.  Returns NULL if no matches. */
{
char *line, *word;
struct trixWordResult *twr = NULL;
struct trixHitPos *hitList = hashFindVal(trix->wordHitHash, searchWord);

if (hitList == NULL)
    {
    struct trixHitPos *oneHitList;
    off_t ixPos = trixFindIndexStartLine(trix->ixx, trix->ixxSize, searchWord);
    ourSeek(trix, trix->lf, ixPos);
    while (ourReadLine(trix, trix->lf, &line))
        {
        word = nextWord(&line);
        if (startsWith(searchWord, word))
            {
            int leftoverLetters = reasonablePrefix(searchWord, word, mode);
            /* uglyf("reasonablePrefix(%s,%s)=%d<BR>\n", searchWord, word, leftoverLetters); */
            if (leftoverLetters >= 0)
                {
                oneHitList = trixParseHitList(searchWord, line, leftoverLetters);
                hitList = slCat(oneHitList, hitList);
                }
            }
        else if (strcmp(searchWord, word) < 0)
            break;
        }
    slSort(&hitList, trixHitPosCmp);
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

static int findUnorderedSpan(struct trixWordResult *twrList,
	char *itemId, int *retLeftoverLetters, int **retWordPos)
/* Find out smallest number of words in doc that will cover
 * all words in search. */
{
int minSpan = BIGNUM;
int leftoverLetters = 0;
int curLeftover = 0;
int minPossibleSpan;
int i;
struct trixWordResult *twr;

/* Set up iHit pointers we use to keep track of our 
 * search.  Don't want to mess with hit pointers as they
 * will be used later. */
for (twr = twrList; twr != NULL; twr = twr->next)
    twr->iHit = twr->hit;
int numWords = minPossibleSpan = slCount(twrList);
int *unorderedWordPositions;
AllocArray(unorderedWordPositions, numWords);
for (;;)
    {
    int minWord = BIGNUM, maxWord=0, span;

    /* Figure out current span and save as min if it's smallest so far. */
    for (twr = twrList, i = 0; twr != NULL && i < numWords; twr = twr->next, i++)
        {
        unorderedWordPositions[i] = twr->iHit->wordIx-1;
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
            if (hit == NULL || !sameString(hit->itemId, itemId) || minSpan == minPossibleSpan)
                {
                *retLeftoverLetters = leftoverLetters;
                *retWordPos = unorderedWordPositions;
                return span + 1;
                }
            }
        }
    }
*retWordPos = unorderedWordPositions;
} 

int findWordPos(struct trixWordResult *twrList, char *itemId)
/* Figure out the first word position.  For multiple words, this
 * will be the maximimum first word position of all words. 
 * This assumes that the hits are sorted by word position
 * within a document. NOTE: this does not take into account
 * that the span may not actually start at that word */
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

static int findOrderedSpan(struct trixWordResult *twrList, char *itemId, int **retWordPos)
/* Find out smallest number of words in doc that will cover
 * all words in search. */
{
int minSpan = BIGNUM - 1;  // subtract 1 to accomodate adding 1 below
struct trixWordResult *twr;

/* Set up iHit pointers we use to keep track of our 
 * search.  Don't want to mess with hit pointers as they
 * will be used later. */
for (twr = twrList; twr != NULL; twr = twr->next)
    twr->iHit = twr->hit;

int numWords = slCount(twrList);
int *orderedWordPositions;
AllocArray(orderedWordPositions, numWords);
for (;;)
    {
    int startWord = twrList->iHit->wordIx-1;
    int endWord = startWord;
    int span, i;
    struct trixHitPos *hit;
    // we haven't found a span yet so initialize to the first word start position
    if (minSpan == BIGNUM - 1)
        orderedWordPositions[0] = startWord;

    /* Set up twr->iHit to be closest one past hit of previous twr. */
    for (twr = twrList->next, i = 1; twr != NULL && i < numWords; twr = twr->next, i++)
        {
        for (hit = twr->iHit; ; hit = hit->next)
            {
            if (hit == NULL || !sameString(hit->itemId, itemId))
                return minSpan + 1;
            if (hit->wordIx > endWord)
                break;
            }
        twr->iHit = hit;
        endWord = hit->wordIx;
        orderedWordPositions[i] = hit->wordIx-1;
        }
    span = endWord - startWord;
    if (span < minSpan)
        {
        orderedWordPositions[0] = startWord;
        CopyArray(orderedWordPositions, *retWordPos, numWords);
        minSpan = span;
        }

    /* Advance to next occurence of first word. */
    hit = twrList->iHit = twrList->iHit->next;
    if (hit == NULL || !sameString(hit->itemId, itemId))
        {
        return minSpan+1;
        }
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
        // NOTE: tsr->wordPos first gets filled in by findUnorderedSpan.
        // If findOrderedSpan also finds the search words in order, tsr->wordPos
        // becomes that order instead of the unordered positions. Ideally I suppose
        // it would be best that there were two word position arrays saved for each
        // type
        ts->unorderedSpan = findUnorderedSpan(twrList, itemId, &ts->leftoverLetters, &ts->wordPos);
        ts->orderedSpan = findOrderedSpan(twrList, itemId, &ts->wordPos);
        ts->wordPosSize = slCount(twrList);
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
/* Compare two trixSearchResult in such a way that most relevant searches tend to be first. */
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
            {
            int i;
            for (i = 0; i < a->wordPosSize; i++)
                {
                dif = a->wordPos[i] - b->wordPos[i];
                if (dif != 0)
                    break;
                }
            }
        }
    }
return dif;
}

struct trixSearchResult *trixSearch(struct trix *trix, int wordCount, char **words,
                                    enum trixSearchMode mode)
/* Return a list of items that match all words.  This will be sorted so that
 * multiple-word matches where the words are closer to each other and in the
 * right order will be first.  Single word matches will be prioritized so that those
 * closer to the start of the search text will appear before those later.
 * Do a trixSearchResultFreeList when done.  If mode is tsmExpand or tsmFirstFive then
 * this will match not only the input words, but also additional words that start with
 * the input words. If addSnippets is true, go back to the original text and include
 * the text around the matching word as a "word1 word2 ... matchedWord word3 word4 ..."
 * style snippet. The snippet will have '...' in between multi-word matches if the
 * span between matched words is 'too' long. */
{
struct trixWordResult *twr, *twrList = NULL;
struct trixSearchResult *ts, *tsList = NULL;
int wordIx;
boolean gotMiss = FALSE;

if (wordCount == 1)
    {
    struct trixHitPos *hit;
    char *lastId = "";
    twr = twrList = trixSearchWordResults(trix, words[0], mode);
    if (twr == NULL)
        return NULL;
    for (hit = twr->hitList; hit != NULL; hit = hit->next)
        {
        if (!sameString(lastId, hit->itemId))
            {
            lastId = hit->itemId;
            AllocVar(ts);
            ts->itemId = hit->itemId;        /* Transfer itemId */
            hit->itemId = NULL;
            ts->orderedSpan = 1;
            ts->unorderedSpan = 1;
            AllocArray(ts->wordPos, 1);
            ts->wordPos[0] = hit->wordIx - 1;
            ts->wordPosSize = 1;
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
        twr = trixSearchWordResults(trix, searchWord, mode);
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

void snippetIndexAddToIxx(struct snippetIndex *snippetIndex, off_t pos, char *prefix)
/* Add to trix->ixx. */
{
struct trixIxx *ixx;
if (snippetIndex->ixxSize >= snippetIndex->ixxAlloc)
     {
     snippetIndex->ixxAlloc += snippetIndex->ixxAlloc;	/* Double allocation. */
     ExpandArray(snippetIndex->ixx, snippetIndex->ixxSize, snippetIndex->ixxAlloc);
     }
ixx = snippetIndex->ixx + snippetIndex->ixxSize;
ixx->pos = pos;
AllocVar(ixx->prefix);
toLowerN(prefix, trixPrefixSize);
memcpy(ixx->prefix, prefix, trixPrefixSize);
snippetIndex->ixxSize += 1;
}

static void openSnippetIxx(struct snippetIndex *snippetIndex, char *ixxFile, struct trix *trix)
{
int prefixSize = 15;
snippetIndex->ixxAlloc = 8*1024;
AllocArray(snippetIndex->ixx, snippetIndex->ixxAlloc);
void *lf = ourOpen(trix, ixxFile);
char *line;
while (ourReadLine(trix, lf, &line) )
    {
    off_t pos = unhex(line+prefixSize);
    snippetIndexAddToIxx(snippetIndex, pos, line);
    }
ourClose(trix, &lf);
}

static void openSnippetIndex(struct trix *trix)
/* Fill out a struct snippetIndex on a trix index */
{
char *baseName = cloneString(trix->lf->fileName);
chopSuffix(baseName);
char origFile[PATH_LEN];
safef(origFile, sizeof(origFile), "%s.txt", baseName);
if (!fileExists(origFile))
    safef(origFile, sizeof(origFile), "%s.tab", baseName);
if (!fileExists(origFile))
    errAbort("Can't find original file used for indexing, base name: '%s', %s.txt or %s.tab should exist",
            baseName, baseName, baseName);
char offsetFile[PATH_LEN];
char ixxFile[PATH_LEN];
safef(offsetFile, sizeof(origFile), "%s.offsets", baseName);
if (!fileExists(offsetFile))
    errAbort("Can't find offset file '%s.offsets'", baseName);
safef(ixxFile, sizeof(origFile), "%s.offsets.ixx", baseName);
if (!fileExists(ixxFile))
    errAbort("Can't find offsets.ixx file '%s.offsets.ixx'", baseName);
struct snippetIndex *snippetIndex = NULL;
AllocVar(snippetIndex);
snippetIndex->origFile = lineFileOpen(origFile, TRUE);
snippetIndex->textIndex = lineFileOpen(offsetFile, TRUE);
openSnippetIxx(snippetIndex, ixxFile, trix);
trix->snippetIndex = snippetIndex;
}

bool wordMiddleChars[256];  /* Characters that may be part of a word. */
bool wordBeginChars[256];

void initCharTables()
/* Initialize tables that describe characters. */
{
int c;
for (c=0; c<256; ++c)
    if (isalnum(c))
       wordBeginChars[c] = wordMiddleChars[c] = TRUE;
wordBeginChars['_'] = wordMiddleChars['_'] = TRUE;
wordMiddleChars['.'] = TRUE;
wordMiddleChars['-'] = TRUE;
}

char *skipToWord(char *s)
/* Skip to next word character.  Return NULL at end of string. */
{
unsigned char c;
while ((c = *s) != 0)
    {
    if (wordBeginChars[c])
        return s;
    s += 1;
    }
return NULL;
}

char *skipOutWord(char *start)
/* Skip to next non-word character.  Returns empty string at end. */
{
char *s = start;
unsigned char c;
while ((c = *s) != 0)
    {
    if (!wordMiddleChars[c])
        break;
    s += 1;
    }
while (s > start && !wordBeginChars[(int)(s[-1])])
    s -= 1;
return s;
}

static struct rbTree *wordPosToRangeTree(int *intList, int len)
/* Convert an array of integers to a range tree with 10 context around each int */
{
struct rbTree *rt = rangeTreeNew();
int i;
for (i = 0; i < len; i++)
    {
    int temp = intList[i] - 10;
    if (temp < 0) temp = 0;
    rangeTreeAdd(rt, temp, intList[i]+10);
    }
return rt;
}

void addSnippetForResult(struct trixSearchResult *tsr, struct trix *trix)
/* Find the snippet for a search result */
{
struct snippetIndex *snippetIndex = trix->snippetIndex;
char *ourDocId = tsr->itemId;
static struct dyString *snippet = NULL;
if (snippet == NULL)
    snippet = dyStringNew(0);
else
    dyStringClear(snippet);
off_t origFileOffset = 0, offsetFilePos = trixFindIndexStartLine(snippetIndex->ixx, snippetIndex->ixxSize, ourDocId);
ourSeek(trix, snippetIndex->textIndex, offsetFilePos);
char *snippetIxLine, *origTextLine;
char *thisDocId;
while (ourReadLine(trix, snippetIndex->textIndex, &snippetIxLine))
    {
    thisDocId = nextWord(&snippetIxLine);
    if (sameString(thisDocId, ourDocId))
        {
        origFileOffset = sqlUnsignedLong(nextWord(&snippetIxLine));
        ourSeek(trix, snippetIndex->origFile, origFileOffset);
        ourReadLine(trix, snippetIndex->origFile, &origTextLine);
        nextWord(&origTextLine);
        char *origText = skipLeadingSpaces(origTextLine);
        // make a rangeTree out of wordPos
        intSort(tsr->wordPosSize, tsr->wordPos);
        struct rbTree *rt = wordPosToRangeTree(tsr->wordPos, tsr->wordPosSize);
        int i, end = tsr->wordPos[tsr->wordPosSize-1] + 10;
        char word[128];
        char *s, *e = origText;
        boolean didEllipse = FALSE;
        for (i = 0 ; i < end; i++)
            {
            s = skipToWord(e);
            if (s == NULL)
                break;
            e = skipOutWord(s);
            if (rangeTreeOverlaps(rt, i, i+1))
                {
                int len = e - s;
                safencpy(word, sizeof(word), s, len);
                if (isNotEmpty(word))
                    {
                    int j;
                    boolean bold = FALSE;
                    for (j = 0; j < tsr->wordPosSize; j++)
                        if (i == tsr->wordPos[j])
                            bold = TRUE;
                    if (bold)
                        dyStringPrintf(snippet, "<b>%s</b>", word);
                    else
                        dyStringPrintf(snippet, "%s", word);
                    if (i < end)
                        dyStringPrintf(snippet, " ");
                    didEllipse = FALSE;
                    }
                else
                    break;
                }
            else if (!didEllipse)
                {
                didEllipse = TRUE;
                dyStringPrintf(snippet, " ... ");
                }
            }
        tsr->snippet = dyStringCannibalize(&snippet);
        break;
        }
    }
}

void resetPrefixSize()
{
trixPrefixSize = 5;
}

void initSnippetIndex(struct trix *trix)
/* Setup what we need to obtain snippets */
{
trixPrefixSize = 15;
initCharTables();
openSnippetIndex(trix);
}

void addSnippetsToSearchResults(struct trixSearchResult *tsrList, struct trix *trix)
/* Add snippets to each search result in tsrList. Wrap in an errCatch
 * in case the supporting files don't exist. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    initSnippetIndex(trix);
    struct trixSearchResult *tsr;
    for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
        addSnippetForResult(tsr, trix);
    }
if (errCatch->gotError || errCatch->gotWarning)
    {
    warn("error adding snippets to search results: %s", errCatch->message->string);
    }
errCatchEnd(errCatch);
resetPrefixSize();
}
