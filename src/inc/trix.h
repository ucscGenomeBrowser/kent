/* trix - text retrieval index.  Stuff for fast two level index
 * of text for fast word searches. */

struct trix
/* A two level index */
    {
    struct lineFile *lf;	/* Open file on first level index. */
    struct trixIxx *ixx;	/* Second level index in memory. */
    int ixxSize;		/* Size of second level index. */
    int ixxAlloc;	        /* Space allocated for index. */
    struct hash *wordHitHash;	/* Hash of word hitsLists, so search on "the the the" works fast. */
    };

struct trixSearchResult
/* Result of a trix search. */
    {
    struct trixSearchResult *next;
    char *itemId;               /* ID of matching item */
    int unorderedSpan;          /* Minimum span in single doc with words in any order. */
    int orderedSpan;            /* Minimum span in single doc with words in search order. */
    int wordPos;		/* Position of word in doc more or less. */
    int leftoverLetters;	/* Number of leftover letters in words. */
    };

#define trixPrefixSize 5	/* Size of prefix in second level index. */

struct trix *trixOpen(char *ixFile);
/* Open up index.  Load second level index in memory. */

void trixClose(struct trix **pTrix);
/* Close up index and free up associated resources. */

struct trixSearchResult *trixSearch(struct trix *trix, int wordCount, char **words,
	boolean expand);
/* Return a list of items that match all words.  This will be sorted so that
 * multiple-word matches where the words are closer to each other and in the
 * right order will be first.  Do a trixSearchResultFreeList when done. 
 * If expand is TRUE then this will match not only the input words, but also
 * additional words that start with the input words. */

void trixSearchResultFree(struct trixSearchResult **pTsr);
/* Free up data associated with trixSearchResult. */

void trixSearchResultFreeList(struct trixSearchResult **pList);
/* Free up a list of trixSearchResults. */
