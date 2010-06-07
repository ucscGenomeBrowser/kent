/* Figure out size of unique parts of genome. */
#include "common.h"
#include "portable.h"
#include "linefile.h"

void usage()
/* Print usage and exit. */
{
errAbort(
  "Figure out size of unique parts of genome.\n"
  "usage:\n"
  "   uniqSize ooDir version\n");
}

void getSizes(char *fileName, int *retU, int *retN)
/* Add together sizes in a gold  file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
int start,end,size;
int u = 0, n = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount < 8)
	errAbort("Short line %d of %s\n", lf->lineIx, lf->fileName);
    start = atoi(words[1]) - 1;
    end = atoi(words[2]);
    size = end-start;
    if (words[4][0] == 'N')
	n += size;
    else
	u += size;
    }
lineFileClose(&lf);
*retU = u;
*retN = n;
}

void uniqSize(char *ooDir, char *version)
/* Figure out unique parts of genome from all the
 * gold.22 files in ooDir */
{
struct slName *chromDirs, *chromEl;
struct slName *contigDirs, *contigEl;
char subDir[512];
unsigned long total = 0;
unsigned long totalN = 0;


chromDirs = listDir(ooDir, "*");
for (chromEl = chromDirs; chromEl != NULL; chromEl = chromEl->next)
    {
    char *chromName = chromEl->name;
    int dirNameLen = strlen(chromName);
    if (dirNameLen > 0 && dirNameLen <= 2)
	{
	sprintf(subDir, "%s/%s", ooDir, chromName);
	contigDirs = listDir(subDir, "ctg*");
	for (contigEl = contigDirs; contigEl != NULL; contigEl = contigEl->next)
	    {
	    int nSize, uSize;
	    char *contigName = contigEl->name;
	    char fileName[512];
	    sprintf(fileName, "%s/%s/gold.%s", subDir, contigName, version);
	    if (fileExists(fileName))
		{
		getSizes(fileName, &uSize, &nSize);
		printf("%s/%s has %d real %d N\n",  subDir, contigName, uSize, nSize);
		total += uSize;
		totalN += nSize;
		}
	    else
		{
		warn("No gold.36 in %s/%s", subDir, contigName);
		}
	    }
	slFreeList(&contigDirs);
	}
    }
printf("\n%lu unique %lu N's\n", total, totalN);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
uniqSize(argv[1], argv[2]);
return 0;
}
