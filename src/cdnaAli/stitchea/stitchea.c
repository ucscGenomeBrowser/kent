#include "common.h"
#include "hash.h"

struct textLine
    {
    struct textLine *next;
    char line[1];   /* This is allocated to be bigger. */
    };


struct textLine *loadLines(char *fileName, struct textLine *list)
/* Load in each line of file into a textLine structure. */
{
char buf[512];
struct textLine *tl;
int textSize;
FILE *f = mustOpen(fileName, "r");

while (fgets(buf, sizeof(buf), f) != NULL)
    {
    textSize = strlen(buf);
    tl = needMem(sizeof(*tl) + textSize);
    strcpy(tl->line, buf);
    tl->next = list;
    list = tl;
    }
fclose(f);
return list;
}

struct hitBag
    {
    struct hitBag *next;
    char *name;
    int ix;
    struct textLine *lines;
    };

int cmpIx(const void *va, const void *vb)
/* Compare two rough alis by score. */
{
struct hitBag **pA = (struct hitBag **)va;
struct hitBag **pB = (struct hitBag **)vb;
struct hitBag *a = *pA, *b = *pB;

return a->ix - b->ix;
}


int main(int argc, char *argv[])
{
int i;
struct textLine *lineList = NULL;
struct textLine *tl;
int textSize;
struct hitBag *hitList = NULL;
struct hitBag *hb;
char *words[4];
int wordCount;
int lastIx, ix;
struct hashEl *hel;
char buf[512];
char *pt;
FILE *in, *out;
struct hash *hash;
char *outFile;

if (argc < 3)
    {
    errAbort("stitchea - joins together EA files into one big one, throwing out overlaps.\n"
             "Will complain if there's any missing data.\n"
             "Usage:\n"
             "    stitchea outFile inFile(s)");
    }
outFile = argv[1];

/* Build up list of cdna hitbags. */
hash = newHash(12);
for (i=2; i<argc; ++i)
    {
    char *fileName = argv[i];
    int lineCount = 0;
    in = mustOpen(fileName, "r");
    printf("processing %s\n", fileName);
    while (fgets(buf, sizeof(buf), in) != NULL)
        {
        ++lineCount;
        /* Skip empty lines and those starting with # */
        pt = skipLeadingSpaces(buf);
        if (pt[0] == 0 || pt[0] == '#')
            continue;

        /* Clone line into something that hangs onto a list. */
        textSize = strlen(buf);
        tl = needMem(sizeof(*tl) + textSize);
        strcpy(tl->line, buf);
        
        /* Chop up temp copy of line. */
        wordCount = chopString(buf, whiteSpaceChopper, words, ArraySize(words));
        if (wordCount < 4)
            errAbort("short line %d of %s:\n%s", lineCount, fileName, tl->line);
    
        if (strcmp(words[1], "Blasting") == 0)
            {
            AllocVar(hb);
            hb->ix = atoi(words[0]);
            hb->name = cloneString(words[2]);
            hb->lines = tl;
            }
        else if (strcmp(words[1], "alignments") == 0)
            {
            /* Finish up hitbag. */
            tl->next = hb->lines;
            hb->lines = tl;
            slReverse(&hb->lines);

            /* If it's a new one add it to lists. */
            if ((hel = hashLookup(hash, hb->name)) == NULL)
                {
                hashAdd(hash, hb->name, hb);
                hb->next = hitList;
                hitList = hb;
                }
            /* If it's a new one update everything but index. */
            else
                {
                struct hitBag *oldBag = hel->val;
                oldBag->lines = hb->lines;
                }
            hb = NULL;
            }
        else
            {
            tl->next = hb->lines;
            hb->lines = tl;
            }
        }
    fclose(in);
    }
slReverse(hitList);

/* Sort by hitIx and make sure that they are all there. */
slSort(&hitList, cmpIx);
lastIx = 0;
for (hb = hitList; hb != NULL; hb = hb->next)
    {
    ix = hb->ix;
    if (ix != lastIx+1)
        warn("Gap from %d to %d", lastIx, ix);
    lastIx = ix;
    }

/* Write it out to file. */
out = mustOpen(outFile, "w");
for (hb = hitList; hb != NULL; hb = hb->next)
    {
    for (tl = hb->lines; tl != NULL; tl = tl->next)
        {
        fputs(tl->line, out);
        }
    fputs("\n", out);
    }
fclose(out);
return 0;
}
