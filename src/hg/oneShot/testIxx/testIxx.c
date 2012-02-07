/* testIxx - Test index of index. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "testIxx - Test index of index (ixx0.  The ixx will have fixed width entries)\n"
  "usage:\n"
  "   testIxx in.ix out.ixx\n"
  "options:\n"
  "   -prefixSize=N Size of prefix to index on.  Default is 5.\n"
  "   -binSize=N Size of bins.  Default is 64k.\n"
  );
}

static struct optionSpec options[] = {
   {"prefixSize", OPTION_INT},
   {"binSize", OPTION_INT},
   {NULL, 0},
};

int prefixSize = 5;
int binSize = 64*1024;

void setPrefix(char *word, char *prefix)
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

void writeIndexEntry(FILE *f, char *prefix, unsigned long long pos)
/* Write out one index entry to file. */
{
fprintf(f, "%s%010llX\n", prefix, pos);
}

void testIxx(char *inIx, char *outIxx)
/* testIxx - Test index of index. */
{
struct lineFile *lf = lineFileOpen(inIx, TRUE);
FILE *f = mustOpen(outIxx, "w");
char *curPrefix = needMem(prefixSize+1);
char *lastPrefix = needMem(prefixSize+1);
char *writtenPrefix = needMem(prefixSize+1);
off_t startPrefixPos = 0, writtenPos = 0, curPos;
char *line, *word;

/* Read first line and index it. */
if (!lineFileNextReal(lf, &line))
    errAbort("%s is empty", inIx);
word = nextWord(&line);
setPrefix(word, writtenPrefix);
setPrefix(lastPrefix, writtenPrefix);
writtenPos = lineFileTell(lf);
writeIndexEntry(f, writtenPrefix, writtenPos);

/* Loop around adding to index as need be */
while (lineFileNextReal(lf, &line))
    {
    int diff;
    curPos = lineFileTell(lf);
    word = nextWord(&line);
    setPrefix(word, curPrefix);
    if (!sameString(curPrefix, lastPrefix))
        startPrefixPos = curPos;
    diff = curPos - writtenPos;
    if (diff >= binSize)
        {
	if (!sameString(curPrefix, writtenPrefix))
	    {
	    writeIndexEntry(f, curPrefix, startPrefixPos);
	    writtenPos = curPos;
	    strcpy(writtenPrefix, curPrefix);
	    }
	}
    strcpy(lastPrefix, curPrefix);
    }
carefulClose(&f);
lineFileClose(&lf);
freeMem(curPrefix);
freeMem(lastPrefix);
freeMem(writtenPrefix);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
prefixSize = optionInt("prefixSize", prefixSize);
binSize = optionInt("binSize", binSize);
if (argc != 3)
    usage();
testIxx(argv[1], argv[2]);
return 0;
}
