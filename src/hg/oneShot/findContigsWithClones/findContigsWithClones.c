/* findContigsWithClones - find contigs that contain clones. */
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
  "findContigsWithClones - find contigs that contain clones\n"
  "usage:\n"
  "   findContigsWithClones cloneList ooDir fileName\n"
  "example:\n"
  "   findContigsWithClones chr22.lst oo.29 gold.99\n"
  );
}

boolean snoopInFile(char *fileName, struct slName *wordList)
/* Return TRUE if any word in list occurs in file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct slName *el;
boolean foundIt = FALSE;

while (lineFileNext(lf, &line, NULL))
    {
    for (el = wordList; el != NULL; el = el->next)
	{
	if (strstr(line, el->name))
	    {
	    foundIt = TRUE;
	    break;
	    }
	}
    if (foundIt)
        break;
    }
lineFileClose(&lf);
return foundIt;
}

struct slName *cloneList;
char *snoopFileName;

void doContig(char *dir, char *chrom, char *contig)
/* Report contigs that have clone in them. */
{
char fileName[512];
sprintf(fileName, "%s/%s", dir, snoopFileName);
if (snoopInFile(fileName, cloneList))
    printf("%s/%s\n", chrom, contig);
}

void findContigsWithClones(char *cloneFile, char *ooDir, char *snoopIn)
/* findContigsWithClones - find contigs that contain clones. */
{
struct slName *el;
cloneList = readAllLines(cloneFile);
for (el = cloneList; el != NULL; el = el->next)
    chopSuffix(el->name);
snoopFileName = snoopIn;
ooToAllContigs(ooDir, doContig);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
findContigsWithClones(argv[1], argv[2], argv[3]);
return 0;
}
