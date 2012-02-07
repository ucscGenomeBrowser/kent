/* testIndex - Create a word index. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "testIndex - Create a word index on file.   The word index is just a text file\n"
  "usage:\n"
  "   testIndex inFile outIndex\n"
  );
}

static struct optionSpec options[] = {
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
while (s > start && !wordBeginChars[s[-1]])
    s -= 1;
return s;
}


struct wordPos
/* Word position. */
    {
    struct wordPos *next;	/* Next wordPos in list. */
    char *itemId;	/* ID of associated item.  Not allocated here*/
    int docIx;		/* Document number. */
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
   {
   dif = a->docIx - b->docIx;
   if (dif == 0)
      dif = a->wordIx - b->wordIx;
   }
return dif;
}

void indexWords(struct hash *wordHash, int docIx, char *track, char *source, 
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
	pos->docIx = docIx;
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
	fprintf(f, " %s,%d,%d", pos->itemId, pos->docIx, pos->wordIx);
    fprintf(f, "\n");
    }
carefulClose(&f);
hashElFreeList(&els);
}

void trixIndex(char *inFile, char *outIndex)
/* Create an index file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct hash *wordHash = newHash(20), *itemIdHash = newHash(20);
char *line;
initCharTables();
while (lineFileNextReal(lf, &line))
     {
     char *track, *source, *id, *text;
     track = nextWord(&line);
     source = nextWord(&line);
     id = nextWord(&line);
     text = skipLeadingSpaces(line);
     indexWords(wordHash, lf->lineIx, track, source, id, text, itemIdHash);
     }
writeIndexHash(wordHash, outIndex);
}



int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
trixIndex(argv[1], argv[2]);
return 0;
}
