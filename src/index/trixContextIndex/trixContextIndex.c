/* trixContextIndex - Index in.txt file used with ixIxx to produce a two column file with symbol name 
 * and file offset for that line.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static int prefixSize = 15;
int binSize = 64*1024;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trixContextIndex - Index in.txt file used with ixIxx to produce two files:\n"
  "    - a two column file with symbol name and file offset for that line\n"
  "    - an index into the file with the offsets for efficient retrieval\n"
  "Note that outBaseName.offsets and outBaseName.offsets.ixx will be created\n"
  "usage:\n"
  "   trixContextIndex in.txt outBaseName\n"
  "options:\n"
  "    -prefixSize=N length of prefix to use for second level index\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"prefixSize", OPTION_INT},
   {NULL, 0},
};

void writeIxxEntry(FILE *f, char *prefix, unsigned long long pos)
/* Write out one index entry to file. */
{
fprintf(f, "%s%010llX\n", prefix, pos);
}

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

void makeIxx(char *inIx)
{
char infile[PATH_LEN];
safef(infile, sizeof(infile), "%s.offsets", inIx);
struct lineFile *lf = lineFileOpen(infile, TRUE);
char outIxx[PATH_LEN];
safef(outIxx, sizeof(outIxx), "%s.offsets.ixx", inIx);
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
//tolowers(word);
setPrefix(word, writtenPrefix);
strcpy(lastPrefix, writtenPrefix);
writtenPos = lineFileTell(lf);
writeIxxEntry(f, writtenPrefix, writtenPos);

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
	    writeIxxEntry(f, curPrefix, startPrefixPos);
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

struct wordPos
{
    char *word;
    long long pos;
};

void makeIx(FILE *f, struct hash *itemIdHash)
{
struct hashEl *hel, *helList = hashElListHash(itemIdHash);
slSort(&helList, hashElCmp);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    char *word = hel->name;
    struct wordPos *wordPos = (struct wordPos *)hel->val;
    fprintf(f, "%s\t%lld\n", word, wordPos->pos);
    }
}

void trixContextIndex(char *input, char *output)
/* trixContextIndex - Index in.txt file used with ixIxx to produce a two column file with symbol name 
 * and file offset for that line.. */
{
char outputFile[PATH_LEN];
safef(outputFile, sizeof(outputFile), "%s.offsets", output);
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(outputFile, "w");
char *line;
struct hash *itemIdHash = hashNew(20);
while (lineFileNextReal(lf, &line))
    {
    long long pos = lineFileTell(lf);
    char *word = nextWord(&line);
    if (word == NULL)
        errAbort("Short line %d of %s", lf->lineIx, input);
    struct wordPos *wordPos = NULL;
    AllocVar(wordPos);
    wordPos->word = word;
    wordPos->pos = pos;
    hashAdd(itemIdHash, word, wordPos);
    }
makeIx(f, itemIdHash);
carefulClose(&f);
makeIxx(output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
prefixSize = optionInt("prefixSize", prefixSize);
if (argc != 3)
    usage();
trixContextIndex(argv[1], argv[2]);
return 0;
}
