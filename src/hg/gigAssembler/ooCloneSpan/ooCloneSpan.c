/* cloneSpan - List clones and the amount the span by looking at .gl file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cloneSpan - List clones and the amount the span by looking at .gl file\n"
  "usage:\n"
  "   cloneSpan file.gl\n");
}

struct clone
/* Info on one clone. */
   {
   struct clone *next;      /* Next in list. */
   char *name;	            /* Name allocate in hash. */
   int start, end;          /* Start/end position in golden path. */
   int baseCount;           /* Number of bases. */
   };

void cloneSpan(char *fileName)
/* cloneSpan - List clones and the amount the span by looking at .gl file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount, lineSize;
char *words[16], *line;
struct hash *hash = newHash(0);
struct hashEl *hel;
char *cloneName;
int start, end;
struct clone *cloneList = NULL, *clone;
int totalSpan = 0, totalBases = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount < 3)
       lineFileExpectWords(lf, 3, wordCount);
    cloneName = words[0];
    chopSuffix(cloneName);
    start = sqlUnsigned(words[1]);
    end = sqlUnsigned(words[2]);
    clone = hashFindVal(hash, cloneName);
    if (clone == NULL)
        {
	AllocVar(clone);
	hel = hashAdd(hash, cloneName, clone);
	clone->name = hel->name;
	clone->start = start;
	clone->end = end;
	slAddHead(&cloneList, clone);
	}
    else
        {
	if (clone->start > start)
	    clone->start = start;
	if (clone->end < end)
	    clone->end = end;
	}
    clone->baseCount += end-start;
    }
lineFileClose(&lf);
slReverse(&cloneList);

for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    int span = clone->end - clone->start;
#ifdef SOMETIMES
    printf("clone %s, bases %d, spans %d, density %4.2f%%\n",
        clone->name, clone->baseCount, span,
	100.0 * (double)clone->baseCount/(double)span);
#endif
    totalSpan += span;
    totalBases += clone->baseCount;
    }
printf("%s bases %d, spans %d, density %4.2f%%\n",
    fileName, totalBases, totalSpan,
    100.0 * (double)totalBases/(double)totalSpan);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
if (argc < 2)
    usage();
for (i=1; i<argc; ++i)
    cloneSpan(argv[i]);
return 0;
}
