/* jccTest - This implements the 'phone book' problem that Lutz Prechelt
 * used in An empirical comparison of C, C++, Java, Perl, Python, Rexx, and Tcl. 
 * The problem roughly corresponds with trying to find mnemonic words to
 * help remember phone numbers.  Each number is assigned a letter or two or
 * three.  You're given a dictionary.  You try to turn the phone number
 * into words in the dictionary. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"


#define MAXWORDSIZE 256

void usage()
/* Explain usage and exit. */
{
errAbort(
  "jccTest - 'phone book' test program used to compare various languages.\n"
  "usage:\n"
  "   jccTest dictionary phonebook output\n"
  );
}

/* These specify conversion from number to letter. */
static char *letterCode = "ejnqrwxdsyftamcivbkulopghz";
static char *numberCode = "01112223334455666777888999";
static char trTable[256];

/* Here's where we store "0", "1", and so forth in case we can't
 * find a word. */
static char *literal[256];

static void toNum(char *in, char *outBuf, int outBufSize)
/* Convert letters to numerics */
{
char c;
int i;
for (i=0; i<outBufSize; )
    {
    c = *in++;
    if (c == 0)
        {
	*outBuf++ = 0;
	return;
	}
    c = trTable[c];
    if (c != 0)
        {
	*outBuf++ = c;
	++i;
	if (i >= outBufSize)
	    errAbort("outBufSize too small");
	}
    }
}

static void initLiteral()
/* Init literal table */
{
int i;
for (i='0'; i<= '9'; ++i)
    {
    char buf[2];
    buf[0] = i;
    buf[1] = 0;
    literal[i] = cloneString(buf);
    }
}

static void initTrTable()
/* Initialize translation table to get from letters to number. */
{
int i;
for (i=0; i<26; ++i)
    {
    char l = letterCode[i];
    char n = numberCode[i];
    trTable[l] = n;
    trTable[toupper(l)] = n;
    }
}

static struct hash *readDictionary(char *fileName, int *retMaxSize)
/* Read in dictionary table and put it in hash that is
 * keyed by numeric version of string. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
struct hash *hash = hashNew(18);
int maxSize = 0, size;
while (lineFileNext(lf, &line, NULL))
    {
    while ((word = nextWord(&line)) != NULL)
         {
	 char numBuf[MAXWORDSIZE];
	 toNum(word, numBuf, sizeof(numBuf));
	 hashAdd(hash, numBuf, cloneString(word));
	 size = strlen(numBuf);
	 if (size > maxSize)
	     maxSize = size;
	 }
    }
lineFileClose(&lf);
*retMaxSize = maxSize;
return hash;
}

void stripNonNum(char *s)
/* Remove non-numerical input from s. */
{
char *in = s, *out = s, c;
while ((c = *in++) != 0)
    {
    if (isdigit(c))
        *out++ = c;
    }
*out++ = 0;
}

void rConvert(FILE *f, struct hash *hash, int maxSize, char *s)
/* Print out all possible word representations of s.  This does
 * the job recursively, finding a word in the dictionary that
 * matches the first part of s, and then calling itself on the
 * remainder of s.  It keeps a local stack of words it's seen.
 * When there is nothing left in s, it prints out the stack.*/
{
static char *wordStack[128];
static int wordStackIx = 0;
int len = strlen(s);
int i;
if (len > 0)
    {
    char numBuf[MAXWORDSIZE];
    boolean gotAny = FALSE;
    numBuf[0] = s[0];
    if (maxSize > len)
        maxSize = len;
    for (i=1; i<=maxSize; ++i)
        {
	struct hashEl *hel;
	numBuf[i] = 0;
	hel = hashLookup(hash, numBuf);
	while (hel != NULL)
	    {
	    char *mnemonic = hel->val;
	    wordStack[wordStackIx++] = mnemonic;
	    rConvert(f, hash, maxSize, s+i);
	    --wordStackIx;
	    gotAny = TRUE;
	    hel = hashLookupNext(hel);
	    }
	numBuf[i] = s[i];
	}
    if (!gotAny)
        {
	/* We can't match any words in the dictionary.
	 * Just output one literal number then. */
	wordStack[wordStackIx++] = literal[s[0]];
	rConvert(f, hash, maxSize, s+1);
	--wordStackIx;
	}
    }
else
    {
    /* We've come to the end of the string, print out words we've composed. */
    if (wordStackIx > 0)
        {
	for (i=0; i < wordStackIx; ++i)
	    fprintf(f, "%s ", wordStack[i]);
	fprintf(f, "\n");
        }
    }
}

void processPhoneBook(struct hash *hash, int maxSize, char *phoneNumbers, char *output)
/* Read in phone book and construct numbers for it. */
{
struct lineFile *lf = lineFileOpen(phoneNumbers, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    fprintf(f, "Number: %s\n", line);
    stripNonNum(line);
    rConvert(f, hash, maxSize, line);
    fprintf(f, "\n");
    }
}

void jccTest(char *dictionary, char *phoneBook, char *output)
/* jccTest - Test program.. */
{
int maxSize;
struct hash *hash = readDictionary(dictionary, &maxSize);
processPhoneBook(hash, maxSize, phoneBook, output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
pushCarefulMemHandler(100000000);
initTrTable();
initLiteral();
jccTest(argv[1], argv[2], argv[3]);
printf("total mem used %ld\n", carefulTotalAllocated());
return 0;
}
