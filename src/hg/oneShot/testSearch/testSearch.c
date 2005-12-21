/* testSearch - Set up a search program that does free text indexing and retrieval.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: testSearch.c,v 1.1 2005/12/21 02:28:25 kent Exp $";

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

int prefixSize = 5;

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

void testSearch(char *inFile, char *searchWord)
/* testSearch - Set up a search program that does free text indexing and retrieval.. */
{
char ixxFile[PATH_LEN], ixFile[PATH_LEN];
off_t ixPos;
struct lineFile *ixLf;
char *line, *word;

safef(ixxFile, sizeof(ixxFile), "%s.ixx", inFile);
safef(ixFile, sizeof(ixFile), "%s.ix", inFile);
uglyf("Looking up %s in %s\n", searchWord, ixxFile);
ixPos = findIndexStartPos(ixxFile, searchWord);
uglyf("Index start position is %lld (%llx)\n", ixPos, ixPos);
ixLf = lineFileOpen(ixFile, TRUE);
lineFileSeek(ixLf, ixPos, SEEK_SET);
while (lineFileNext(ixLf, &line, NULL))
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
        printf("NOT FOUND %s\n", word);
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
