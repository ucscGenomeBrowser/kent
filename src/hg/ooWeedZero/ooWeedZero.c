/* ooWeedZero - Weed out phase zero clones from geno.lst. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "ooUtils.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooWeedZero - Weed out phase zero clones from geno.lst\n"
  "usage:\n"
  "   ooWeedZero sequence.inf ooDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct weed
/* Something that needs weeding. */
    {
    struct weed *next;
    char *cloneName;	/* Name of clone. */
    char *ctgDir;	/* Name of file that needs weeding. */
    };
struct weed *weedList;

struct clone
/* Info on a clone */
    {
    struct clone *next;
    char *name;
    int phase;
    };
struct clone *cloneList = NULL;
struct hash *cloneHash;

struct clone *readSeqInfo(char *fileName)
/* Read sequence.info file into clone list. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct clone *cloneList = NULL, *clone;
char *row[10];

while (lineFileRow(lf, row))
    {
    chopSuffix(row[0]);
    AllocVar(clone);
    clone->name = cloneString(row[0]);
    clone->phase = atoi(row[3]);
    slAddHead(&cloneList, clone);
    }
lineFileClose(&lf);
slReverse(&cloneList);
return cloneList;
}

void oneContig(char *dir, char *chrom, char *contig)
/* Build weed list one contig at a time. */
{
char fileName[512];
struct weed *weed;
boolean gotWeed = FALSE;
struct lineFile *lf;
char *row[1];
char root[128];
struct clone *clone;

sprintf(fileName, "%s/geno.lst", dir);
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    splitPath(trimSpaces(row[0]), NULL, root, NULL);
    if ((clone = hashFindVal(cloneHash, root)) != NULL)
        {
	if (clone->phase == 0)
	    {
	    AllocVar(weed);
	    weed->cloneName = cloneString(clone->name);
	    weed->ctgDir = cloneString(dir);
	    slAddHead(&weedList, weed);
	    }
	}
    }
lineFileClose(&lf);
}

void weedFile(char *fileName, char *pattern)
/* Read file/weed file/write file. */
{
struct slName *lineList = readAllLines(fileName), *line;
FILE *f = mustOpen(fileName, "w");
for (line = lineList; line != NULL; line = line->next)
    {
    if (!stringIn(pattern, line->name))
        fprintf(f, "%s\n", line->name);
    }
slFreeList(&lineList);
fclose(f);
}

void weedDir(struct weed *weed)
/* Weed files in dir. */
{
char fileName[512];
static char *gardens[] = {"geno.lst", "self.psl", "pairedReads.psl", "mrna.psl", "est.psl", "bacEnd.psl"};
int i;

for (i=0; i<ArraySize(gardens); ++i)
    {
    sprintf(fileName, "%s/%s", weed->ctgDir, gardens[i]);
    weedFile(fileName, weed->cloneName);
    }
}

void ooWeedZero(char *seqInfo, char *ooDir)
/* ooWeedZero - Weed out phase zero clones from geno.lst. */
{
struct clone *clone;
struct weed *weed;

cloneHash = newHash(0);
cloneList = readSeqInfo(seqInfo);
for (clone = cloneList; clone != NULL; clone = clone->next)
    hashAdd(cloneHash, clone->name, clone);
ooToAllContigs(ooDir, oneContig);
slReverse(&weedList);
for (weed = weedList; weed != NULL; weed = weed->next)
    {
    weedDir(weed);
    uglyf("Got weed %s in %s\n", weed->cloneName, weed->ctgDir);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
ooWeedZero(argv[1], argv[2]);
return 0;
}
