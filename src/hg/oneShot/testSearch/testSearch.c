/* testSearch - Set up a search program that does free text indexing and retrieval.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: testSearch.c,v 1.2 2005/12/21 04:18:46 kent Exp $";

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

char unhexTable[128];	/* Lookup table to help with hex conversion. */

void initUnhexTable()
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

off_t  unhex(char hex[10])
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

off_t findIndexStartPos(char *ixxFile, char *word)
{
struct lineFile *lf = lineFileOpen(ixxFile, TRUE);
char wordPrefix[8], linePrefix[8];
char *line;
off_t pos = 0;

wordPrefix[prefixSize] = 0;
linePrefix[prefixSize] = 0;
memcpy(wordPrefix, word, prefixSize);
tolowers(wordPrefix);
while (lineFileNext(lf, &line, NULL))
    {
    memcpy(linePrefix, line, prefixSize);
    if (strcmp(wordPrefix, linePrefix) < 0)
        break;
    pos = unhex(line+prefixSize);
    }
lineFileClose(&lf);
return pos;
}

struct hitPos 
/* A hit to the index. */
    {
    struct docPos *next;	/* Next in list */
    char *itemId;		/* Associated itemId */
    int docIx;			/* Which document this is part of. */
    int wordIx;			/* Which word this is part of. */
    };

struct wordResults
/* Results of a search on one word. */
    {
    struct wordResults *next;
    char *indexLine;		/* Copy of indexed line. */
    struct hitPos *hitList;	/* Hit list. */
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

void trixSetPrefix(char *word, char *prefix)
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

trixSetPrefix(word, wordPrefix);
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


void testSearch(char *inFile, char *searchWord)
/* testSearch - Set up a search program that does free text indexing and retrieval.. */
{
struct trix *trix;
char ixFile[PATH_LEN];
off_t ixPos;
char *line, *word;

safef(ixFile, sizeof(ixFile), "%s.ix", inFile);
trix = trixOpen(ixFile);
ixPos = trixFindIndexStartLine(trix, searchWord);
lineFileSeek(trix->lf, ixPos, SEEK_SET);
while (lineFileNext(trix->lf, &line, NULL))
    {
    int diff;
    word = nextWord(&line);
    diff = strcmp(searchWord, word);
    if (diff == 0)
	{
        printf("MATCH %s %s\n", word, line);
	break;
	}
    else if (diff < 0)
	{
        printf("NOT FOUND %s\n", searchWord);
	break;
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
initUnhexTable();
testSearch(argv[1], argv[2]);
return 0;
}
