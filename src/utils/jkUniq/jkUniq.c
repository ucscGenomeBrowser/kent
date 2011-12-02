/* jkUniq - pass input to output a line at a time, but only
 * pass through lines haven't seen before. */
#include "common.h"
#include "hash.h"
#include "linefile.h"


void usage()
/* Explain usage and exit */
{
errAbort(
"jkUniq - remove duplicate lines from file.  Lines need not\n"
"be next to each other (plain Unix uniq works for that)\n"
"\n"
"WARNING: this overwrites the input files\n"
"\n"
"usage:\n"
"    jkUniq file(s)");
}

void jkUniq(char *fileName)
/* Remove dupe lines from file. */
{
struct slName *lineList = NULL, *lineEl;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int lineSize;
struct hash *hash = newHash(0);
FILE *f;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (!hashLookup(hash, line))
	{
	hashAdd(hash, line, NULL);
	lineEl = newSlName(line);
	slAddHead(&lineList, lineEl);
	}
    }
slReverse(&lineList);
lineFileClose(&lf);
f = mustOpen(fileName, "w");
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    fputs(lineEl->name, f);
    fputc('\n', f);
    }
fclose(f);
slFreeList(&lineList);
freeHash(&hash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;

if (argc < 2)
    usage();
for (i=1; i<argc; ++i)
    {
    char *fileName = argv[i];
    printf("Uniqing %s (%d of %d)\n", fileName, i, argc-1);
    jkUniq(fileName);
    }
return 0;
}

