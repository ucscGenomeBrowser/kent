/* ixIxx - Create indices for simple line-oriented file of format 
 * <symbol> <free text>. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "trix.h"


/* Variables that can be set from command line. */
int prefixSize = trixPrefixSize;
int binSize = 64*1024;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ixIxx - Create indices for simple line-oriented file of format \n"
  "<symbol> <free text>\n"
  "usage:\n"
  "   ixIxx in.text out.ix out.ixx\n"
  "Where out.ix is a word index, and out.ixx is an index into the index.\n"
  "options:\n"
  "   -prefixSize=N Size of prefix to index on in ixx.  Default is 5.\n"
  "   -binSize=N Size of bins in ixx.  Default is 64k.\n"
  );
}

static struct optionSpec options[] = {
   {"prefixSize", OPTION_INT},
   {"binSize", OPTION_INT},
   {NULL, 0},
};

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


struct wordPos
/* Word position. */
    {
    struct wordPos *next;	/* Next wordPos in list. */
    char *itemId;	/* ID of associated item.  Not allocated here*/
    int wordIx;		/* Word number within doc. */
    };

int wordPosCmp(const void *va, const void *vb)
/* Compare two wordPos by itemId. */
{
const struct wordPos *a = *((struct wordPos **)va);
const struct wordPos *b = *((struct wordPos **)vb);
int dif;
dif = strcmp(a->itemId, b->itemId);
if (dif == 0)
    dif = a->wordIx - b->wordIx;
return dif;
}

void indexWords(struct hash *wordHash, 
	char *itemId, char *text, struct hash *itemIdHash)
/* Index words in text and store in hash. */
{
char *s, *e = text;
char word[32];
int len;
struct hashEl *hel;
struct wordPos *pos;
int wordIx;

tolowers(text);
itemId = hashStoreName(itemIdHash, itemId);
for (wordIx=1; ; ++wordIx)
    {
    s = skipToWord(e);
    if (s == NULL)
        break;
    e = skipOutWord(s);
    len = e - s;
    if (len < ArraySize(word))
        {
	memcpy(word, s, len);
	word[len] = 0;
	hel = hashLookup(wordHash, word);
	if (hel == NULL)
	    hel = hashAdd(wordHash, word, NULL);
	AllocVar(pos);
	pos->itemId = itemId;
	pos->wordIx = wordIx;
	pos->next = hel->val;
	hel->val = pos;
	}
    }
}

void writeIndexHash(struct hash *wordHash, char *fileName)
/* Write index to file.  This pretty much destroys the hash in the
 * process. */
{
struct hashEl *el, *els = hashElListHash(wordHash);
FILE *f = mustOpen(fileName, "w");
slSort(&els, hashElCmp);

for (el = els; el != NULL; el = el->next)
    {
    struct wordPos *pos;
    fprintf(f, "%s", el->name);
    slSort(&el->val, wordPosCmp);
    for (pos = el->val; pos != NULL; pos = pos->next)
	fprintf(f, " %s,%d", pos->itemId, pos->wordIx);
    fprintf(f, "\n");
    }
carefulClose(&f);
hashElFreeList(&els);
}

void makeIx(char *inFile, char *outIndex)
/* Create an index file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct hash *wordHash = newHash(20), *itemIdHash = newHash(20);
char *line;
initCharTables();
while (lineFileNextReal(lf, &line))
     {
     char *id, *text;
     id = nextWord(&line);
     text = skipLeadingSpaces(line);
     indexWords(wordHash, id, text, itemIdHash);
     }
writeIndexHash(wordHash, outIndex);
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

void writeIxxEntry(FILE *f, char *prefix, unsigned long long pos)
/* Write out one index entry to file. */
{
fprintf(f, "%s%010llX\n", prefix, pos);
}

void makeIxx(char *inIx, char *outIxx)
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


void ixIxx(char *inText, char *outIx, char *outIxx)
/* ixIxx - Create indices for simple line-oriented file of format 
 * <symbol> <free text>. */
{
makeIx(inText, outIx);
makeIxx(outIx, outIxx);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
prefixSize = optionInt("prefixSize", prefixSize);
binSize = optionInt("binSize", binSize);
if (argc != 4)
    usage();
ixIxx(argv[1], argv[2], argv[3]);
return 0;
}
