/* This compares two refineAli bad.txt files and prints similarities and differences. */
#include "common.h"
#include "hash.h"

struct hash *hashFirstWord(char *fileName)
/* Make up a hash table out of the first word of each line */
{
FILE *f = mustOpen(fileName, "r");
struct hash *h = newHash(12);
char lineBuf[1024];
char *words[1];

while (fgets(lineBuf, sizeof(lineBuf), f))
    {
    if (chopLine(lineBuf, words))
        hashAdd(h,words[0], NULL);
    }
fclose(f);
return h;
}

struct slName *onlyA(char *a, char *b)
{
FILE *f = mustOpen(a, "r");
struct hash *h = hashFirstWord(b);
int sameCount = 0;
int totalCount = 0;
char lineBuf[1024];
char *words[1];
struct slName *list = NULL, *el;

while (fgets(lineBuf, sizeof(lineBuf), f))
    {
    if (chopLine(lineBuf, words))
        {
        ++totalCount;
        if (hashLookup(h, words[0]) )
            {
            ++sameCount;
            }
        else
            {
            el = newSlName(words[0]);
            slAddHead(&list, el);
            }
        }
    }
printf("%d words in %s, %d unique\n", totalCount, a, totalCount-sameCount);
fclose(f);
freeHash(&h);
slReverse(&list);
return list;
}

void printColumns(struct slName *list, int colWidth, int numCol)
{
int colIx = 0;
struct slName *el;

slNameSort(&list);
for (el = list; el != NULL; el = el->next)
    {
    if (colIx == 0)
        printf("\t");
    printf("%*s ", colWidth, el->name);
    if (++colIx == numCol)
        {
        colIx = 0;
        printf("\n");
        }
    }
if (colIx != 0)
    printf("\n");
}

int main(int argc, char *argv[])
{
char *bad1, *bad2;
struct slName *onlyBad1, *onlyBad2;

if (argc != 3)
    {
    errAbort("badbad compares two bad.txt files and summarizes similarities and differences.\n"
             "usage:  badbad bad1.txt bad2.txt");
    }
bad1 = argv[1];
bad2 = argv[2];

onlyBad1 = onlyA(bad1, bad2);
printColumns(onlyBad1, 16, 4);
onlyBad2 = onlyA(bad2, bad1);
printColumns(onlyBad2, 16, 4);
return 0;
}