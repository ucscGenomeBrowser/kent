/* SortFoo - sort a text file .... somehow. */
#include "common.h"

struct lineData
/* Struct that holds data to sort. */
    {
    struct lineData *next;
    char *line;
    char *key1;
    int key2;
    };

int cmpLines(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct lineData *a = *((struct lineData **)va);
const struct lineData *b = *((struct lineData **)vb);
int dif = strcmp(a->key1, b->key1);
if (dif != 0)
    return dif;
return a->key2 - b->key2;
}




int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in, *out;
char line[1024];
int lineCount; 
struct lineData *lineList = NULL;
struct lineData *ld;
char *words[256];
int wordCount;

if (argc != 3)
    errAbort("usage:\n    sortFoo in out");
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "r");
out = mustOpen(outName, "w");

while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    AllocVar(ld);
    ld->line = cloneString(line);
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount < 11)
        errAbort("Short line %d of %s\n", lineCount, inName);
    ld->key1 = cloneString(words[2]);
    if (!isdigit(words[4][0]))
        errAbort("Expecting number in word 5 of line %d of %s\n", lineCount, inName);
    ld->key2 = atoi(words[4]);
    slAddHead(&lineList, ld);
    }

slSort(&lineList, cmpLines);

for (ld = lineList; ld != NULL; ld = ld->next)
    {
    fputs(ld->line, out);
    }
return 0;
}