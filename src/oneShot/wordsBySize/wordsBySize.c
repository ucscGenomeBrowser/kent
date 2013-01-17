/* wordsBySize - Print all words in a file sorted by size. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/** Some utility code.  In a larger program this would be part of a library module **/

void die()
/* Abort program with exit code that indicates it failed. */
{
exit(-1);
}

FILE *mustOpen(char *fileName, char *mode)
/* Open a file - or squawk and die. */
{
FILE *f;

if ((f = fopen(fileName, mode)) == NULL)
    {
    fprintf(stderr, "Can't open %s: %s\n", fileName, strerror(errno));
    die();
    }
return f;
}

int nextWord(FILE *f, char *buf, int bufSize)
/* Read next space-delimited word from file and put it into a fixed size buffer.
 * Check and complain about words that would be too big for buffer. 
 * Returns size of word, or 0 at end of file. Punctuation marks are
 * returned as single character strings. */
{
int maxStringSize = bufSize-1;   /* Leave room for terminating zero. */

/* Skip over leading spaces.  Return 0 if at end of file. */
int c;
for (;;)
    {
    c = getc(f);
    if (c <= 0)   /* EOF or some other error */
	{
	if (c == EOF)
	    return 0;
	else
	    {
	    fprintf(stderr, "Read error %s\n", strerror(errno));
	    die();
	    }
	}
    if (!isspace(c))
        break;
    }

/* C now has first non-space character. If it's non-alphanumeric we'll just return it. */
if (!isalnum(c))
    {
    buf[0] = c;
    buf[1] = 0;
    return 1;
    }

/* Loop until we get a non-alphanumeric character or we run out of space */
int size = 0;
for (;;)
    {
    /* Make sure we have enough room and add character to buffer. */
    if (size >= maxStringSize)
        {
        fprintf(stderr, "word size is bigger than buffer size\n");
        die();
        }
    buf[size++] = c;

    /* Get next character from file.  If it's not alphanumeric (or EOF or error) then push it 
     * back for next call to this routine and break. */
    c = getc(f);
    if (!isalnum(c))
       {
       ungetc(c, f);
       break;
       }
    }

/* Add string termination and return string size. */
buf[size] = 0;
return size;
}

void *needMem(size_t size)
/* Allocate memory for a string or die trying.  Result is cleared to zero. */
{
void *pt = calloc(size, 1);
if (pt == NULL)
    {
    fprintf(stderr, "Memory allocation of %llu bytes failed\n", (unsigned long long)size);
    die();
    }
return pt;
}

char *cloneString(char *string)
/* Return a copy of zero terminated string allocated in dynamic memory. */
{
int size = strlen(string);
char *clone = needMem(size+1);	/* Allow extra byte for zero tag at end. */
strcpy(clone, string);
return clone;
}


/** Stuff for a hash table. In a larger program this would be in a separate hash module. **/

struct keyVal
/* An string key associated with a generic value. */
    {
    struct keyVal *next;
    char *key;	    /* String we use to look up things in hash */
    void *val;	    /* Value associated with this key */
    };

struct keyVal *keyValNew(char *key, void *val)
/* Create a new initialized keyVal structure. */
{
struct keyVal *kv = needMem(sizeof(struct keyVal));
kv->key = cloneString(key);
kv->val = val;
return kv;
}

struct hash
/* A container that lets you find values associate with a particular string key quickly.
 * This isn't the best hash in the world, but it's simple. */
    {
    struct keyVal **table;	/* Hash buckets. */
    int size;			/* Size of table. */
    };

unsigned int hashFunction(char *string)
/* A simple but generally effective hash function.  Turns a string into a number that
 * tends to be different for different strings */
{
unsigned int result = 0;
unsigned int c;
while ((c = *string++) != '\0')
    result += (result<<3) + c;
return result;
}

struct hash *hashNew()
/* Create a new hash table. */
{
struct hash *hash = needMem(sizeof(struct hash));
hash->size = 1024-1;	/* Nearly a prime number, which helps a bit. */
hash->table = needMem(hash->size * sizeof(hash->table));  
return hash;
}

void *hashFind(struct hash *hash, char *key)
/* Finds value associated with key in hash.  Returns NULL if none exists. */
{
struct keyVal *kv;
int index = hashFunction(key) % hash->size;
for (kv = hash->table[index]; kv != NULL; kv = kv->next)
    if (strcmp(kv->key, key) == 0)
        return kv->val;
return NULL;
}

void hashAdd(struct hash *hash, char *key, void *val)
/* Add key/value pair to hash. */
{
struct keyVal *kv = keyValNew(key, val);
int index = hashFunction(key) % hash->size;
kv->next = hash->table[index];
hash->table[index] = kv;
}

/** A structure and associated routines to keep track of words. **/

struct word
/* A single word that may be hung on a list of words. */
    {
    struct word *next;	/* Next word in list. */
    char *string;	/* Zero terminated string. */
    };

struct word *wordNew(char *string)
/* Create a new word. */
{
struct word *word = needMem(sizeof(struct word));
word->string = cloneString(string);
return word;
}

int wordListCount(struct word *list)
/* Count the number of words in a NULL terminated list of words. */
{
int count = 0;
struct word *word;
for (word = list; word != NULL; word = word->next)
    ++count;
return count;
}

typedef int CmpFunction(const void *elem1, const void *elem2);  /* A comparing function for sort. */

struct word *sortWordList(struct word *list, CmpFunction *compare)
/* Sort list according to compare function and return sorted version. */
{
int count;
count = wordListCount(list);
if (count > 1)   /* Don't bother sorting small lists. */
    {
    /* Turn list into a temporary array. */
    struct word *el;
    struct word **array;
    int i;
    array = needMem(count * sizeof(*array));
    for (el = list, i=0; el != NULL; el = el->next, i++)
        array[i] = el;

    /* Call standard C library sort array using compare function. */
    qsort(array, count, sizeof(array[0]), compare);

    /* Turn array back into list.  Build list in reverse order so can add to head which is fast. */
    list = NULL;
    for (i=count-1; i>=0; --i)
        {
        array[i]->next = list;
        list = array[i];
        }

    /* Free up space used by array. */
    free(array);
    }
return list;
}

int wordCompareSize(const void *va, const void *vb)
/* Compare two words by size. */
{
const struct word *a = *((struct word **)va);	/* Convert from void to actual type */
const struct word *b = *((struct word **)vb);
return strlen(a->string) - strlen(b->string);
}

void wordsBySize(char *fileName)
/* Open file, read through it keeping track of each unique word,
 * and then print out words with longest word first. */
{
/* Stream through input file building up a hash and a list of all unique words. */
FILE *f = mustOpen(fileName, "r");
struct hash *hash = hashNew();
struct word *wordList = NULL;
//char wordBuffer[128];
char wordBuffer[12];
while (nextWord(f, wordBuffer, sizeof(wordBuffer)))
    {
    struct word *word = hashFind(hash, wordBuffer);
    if (word == NULL)
	{
	/* Add word to hash and to our list if it's not already in hash */
	word = wordNew(wordBuffer);
	word->next = wordList;
	wordList = word;
        hashAdd(hash, wordBuffer, word);
	}
    }
fclose(f);

/* Sort list and output results to standard output. */
wordList = sortWordList(wordList, wordCompareSize);
struct word *word;
for (word = wordList; word != NULL; word = word->next)
    puts(word->string);
}


int main(int argc, char *argv[])
/* Process command line and then call routine to do real work. */
{
if (argc != 2)
    {
    fprintf(stderr, 
      "wordsBySize - Print all words in a file sorted by size.\n"
      "usage:\n"
      "   wordsBySize textFile\n"
      );
    die();
    }
wordsBySize(argv[1]);
return 0;
}
