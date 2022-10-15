/* trix - text retrieval index.  Stuff for fast two level index
 * of text for fast word searches.  Generally you use the ixIxx program
 * to make the indexes. */

#ifndef TRIX_H
#define TRIX_H

struct trix
/* A two level index */
    {
    struct lineFile *lf;	/* Open file on first level index. */
    struct trixIxx *ixx;	/* Second level index in memory. */
    int ixxSize;		/* Size of second level index. */
    int ixxAlloc;	        /* Space allocated for index. */
    struct hash *wordHitHash;	/* Hash of word hitsLists, so search on "the the the" works fast. */
    boolean useUdc;            /* are we using UDC or lineFile */
    struct snippetIndex *snippetIndex; /* A second index for retrieving snippets around word matches */ 
    };

struct trixIxx
/* A prefix and offset */
    {
    off_t pos;	   /* Position where prefix first occurs in file. */
    char *prefix;/* Space padded first five letters of what we're indexing. */
    };


struct snippetIndex
/* An index of the original file fed into ixIxx. Used for making snippets. Making snippets
 * requires 3 files:
 * 1. The original text file that we will seek into as necessary (original.txt)
 * 2. A file of ids and offsets of the lines in the original file (original.offsets)
 * 3. An ixx index of the offsets file (original.offsets.ixx) */
    {
    struct lineFile *origFile; /* Original text file */
    struct lineFile *textIndex; /* Open file of file offsets in textFile */
    struct trixIxx *ixx; /* Second level index of the offsets file */
    int ixxSize;		/* Size of second level index. */
    int ixxAlloc;	        /* Space allocated for index. */
    };

struct trixSearchResult
/* Result of a trix search. */
    {
    struct trixSearchResult *next;
    char *itemId;               /* ID of matching item */
    int unorderedSpan;          /* Minimum span in single doc with words in any order. */
    int orderedSpan;            /* Minimum span in single doc with words in search order. */
    int *wordPos;               /* Position(s) of word(s) in doc in search. */
    int leftoverLetters;        /* Number of leftover letters in words. */
    int wordPosSize;            /* Number of positions in wordPos  */
    char *snippet;              /* The original text surrounding a match */
    };

enum trixSearchMode
/* How stringent is the search? */
    {
    tsmExact,                   /* Require whole-word matches. */
    tsmExpand,                  /* Match words that differ from the search term only in the
                                 * last two letters stopping at a word boundary, or that are
                                 * the search word plus "ing". */
    tsmFirstFive                /* Like tsmExpand, but also match words that have the same
                                 * first 5 letters. */
    };

// Size of prefix in second level index. Default is 5 for ixIxx but trixContextIndex and snippet
// searching defaults to 15
extern int trixPrefixSize;

struct trix *trixOpen(char *ixFile);
/* Open up index.  Load second level index in memory. */

void trixClose(struct trix **pTrix);
/* Close up index and free up associated resources. */

struct trixSearchResult *trixSearch(struct trix *trix, int wordCount, char **words,
                                    enum trixSearchMode mode);
/* Return a list of items that match all words.  This will be sorted so that
 * multiple-word matches where the words are closer to each other and in the
 * right order will be first.  Single word matches will be prioritized so that those
 * closer to the start of the search text will appear before those later.
 * Do a trixSearchResultFreeList when done.  If mode is tsmExpand or tsmFirstFive then
 * this will match not only the input words, but also additional words that start with
 * the input words. */

void trixSearchResultFree(struct trixSearchResult **pTsr);
/* Free up data associated with trixSearchResult. */

void trixSearchResultFreeList(struct trixSearchResult **pList);
/* Free up a list of trixSearchResults. */

int trixSearchResultCmp(const void *va, const void *vb);
/* Compare two trixSearchResult in such a way that most relevant searches tend to be first. */

extern bool wordMiddleChars[];  /* Characters that may be part of a word. */
extern bool wordBeginChars[];

void initCharTables();
/* Initialize tables that describe characters. */

char *skipToWord(char *s);
/* Skip to next word character.  Return NULL at end of string. */

char *skipOutWord(char *start);
/* Skip to next non-word character.  Returns empty string at end. */

void addSnippetForResult(struct trixSearchResult *tsr, struct trix *trix);
/* Find the snippet for a search result */

void initSnippetIndex(struct trix *trix);
/* Setup what we need to obtain snippets */

void addSnippetsToSearchResults(struct trixSearchResult *tsrList, struct trix *trix);
#endif //ndef TRIX_H
