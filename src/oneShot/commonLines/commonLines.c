/* commonLines - Print out lines in one file also found in another file using a minimum of 
 * library routines. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_LINE_SIZE (4*1024)
#define HASH_SIZE (64*1024-1)
#define TRUE 1
#define FALSE 0
#define boolean int

void usage()
/* Explain usage and exit. */
{
printf(
  "commonLines - Print out lines in file2 also found in file1 .\n"
  "usage:\n"
  "   commonLines file1 file2\n"
  );
exit(-1);
}

/** Memory file helper routines **/

void *needMemory(size_t size)
/* Return memory of given size or die trying. */
{
void *pt = malloc(size);
if (pt == NULL)
    {
    printf("Couldn't allocate %d bytes\n", (int)size);
    exit(-1);
    }
return pt;
}

void *needClearMemory(size_t size)
/* Return memory of given size cleared to zero or die trying. */
{
void *pt = needMemory(size);
memset(pt, 0, size);
return pt;
}

/** File helper routines. **/

FILE *mustOpenToRead(char *fileName)
/* Open a file to read or die trying. */
{
FILE *f = fopen(fileName, "r");
if (f == NULL)
    {
    printf("Can't open %s\n", fileName);
    exit(-1);
    }
return f;
}

boolean readLine(FILE *f, char line[MAX_LINE_SIZE])
/* Read a line from file.  Return number of characters in line (0 at end of file) */
{
int c, i;
for (i=0; i<MAX_LINE_SIZE-1; ++i)
    {
    c = getc(f);
    if (c < 0)
        {
	if (i == 0)
	    return FALSE;
	else
	    break;
	}
    if (c == '\n')
        break;
    line[i] = c;
    }
line[i] = 0;   /* Add zero tag at end. */
if (i == MAX_LINE_SIZE-1)	/* Warn and abort about long lines. */
    {
    printf("Long line starting \"%s\"\n", line);
    exit(-1);
    }
return TRUE;
}

/** String routines. */

int stringSize(char *s)
/* Return size of zero terminate string. */
{
int size = 0;
char c;
while ((c = *s++) != 0)
    ++size;
return size;
}

void stringCopy(char *source, char *dest)
/* Copy source string to dest including terminating zero.  Assume there is room
 * in dest. */
{
char c;
while ((c = *source++) != 0)
    *dest++ = c;
*dest++ = 0;
}

int sameString(char *a, char *b)
/* Return TRUE (1) if strings same, FALSE (0) otherwise. */
{
char c;
for (;;)
    {
    c = *a++;
    if (c != *b++)
        return FALSE;
    if (c == 0)
        return TRUE;
    }
}

char *cloneString(char *s)
/* Return copy of string in dynamic memory. */
{
int size = stringSize(s);
char *dupe = needMemory(size+1);
stringCopy(s, dupe);
return dupe;
}

/** Hash table structures and routines. */

struct hashElement
/* An element in a hash table. */
    {
    struct hashElement *next;
    char *string;
    void *value;
    };

struct hash
/* A hash table. */
    {
    struct hashElement *table[HASH_SIZE];
    };

struct hash *hashNew()
/* Return a new hash table. */
{
return needClearMemory(sizeof(struct hash));
}

unsigned int hashFunction(char *s)
/* Return an integer between 0 and HASH_SIZE-1 that is repeatably associated
 * with string s, and that tends to be different between different strings. */
{
unsigned val = 0;
unsigned char c;
while ((c = *s++) != 0)
    {
    val *= 5;
    val += c;
    }
return val % HASH_SIZE;
}

struct hashElement *hashAdd(struct hash *hash, char *s)
/* Add string to hash. Return associated hash element. */
{
int slot = hashFunction(s);
struct hashElement *el = needMemory(sizeof(struct hashElement));
el->string = cloneString(s);
el->next = hash->table[slot];
hash->table[slot] = el;
return el;
}

struct hashElement *hashFind(struct hash *hash, char *s)
/* Return hash element if any associated with string. */
{
struct hashElement *el;
for (el = hash->table[hashFunction(s)]; el != NULL; el = el->next)
    {
    if (sameString(el->string, s))
        return el;
    }
return NULL;
}

struct hashElement *hashAddUnique(struct hash *hash, char *s)
/* Add string to hash if it's not already there.  Return hash element associated with string. */
{
struct hashElement *el = hashFind(hash, s);
if (el != NULL)
    return el;
return hashAdd(hash, s);
}

/** Start of code that is specific to our task at hand. */

struct hash *hashLines(char *fileName)
/* Read file and stuff all lines of it into a hash table that we return. */
{
FILE *f = mustOpenToRead(fileName);
struct hash *hash = hashNew();
char line[MAX_LINE_SIZE];
while (readLine(f, line))
    hashAddUnique(hash, line);
fclose(f);
return hash;
}

void commonLines(char *file1, char *file2)
/* commonLines - Print out lines in one file also found in another file using a minimum of 
 * library routines.. */
{
struct hash *hash = hashLines(file1);
FILE *f = mustOpenToRead(file2);
char line[MAX_LINE_SIZE];
while (readLine(f, line))
    {
    if (hashFind(hash, line))
        printf("%s\n", line);
    }
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
commonLines(argv[1], argv[2]);
return 0;
}
