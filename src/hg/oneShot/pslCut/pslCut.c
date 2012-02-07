/* pslCut - Remove a list of clones from psl file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "hCommon.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCut - Remove a list of clones from psl file.\n"
  "usage:\n"
  "   pslCut cut.lst in.psl out.psl\n");
}

void buildCutHash(char *file, struct hash *hash)
/* Each line of file contains a file name.  Put the
 * non-dir/non-suffix part onto hash. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char dir[256], name[128], ext[64];
int lineSize;
char *line;
int count = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    splitPath(line, dir, name, ext);
    hashStore(hash, name);
    ++count;
    }
lineFileClose(&lf);
printf("%d clones in cut list %s\n", count, file);
}

void pslCut(char *cutList, char *inPsl, char *outPsl)
/* pslCut - Remove a list of clones from psl file.. */
{
struct hash *cutHash = newHash(0);
struct lineFile *lf = pslFileOpen(inPsl);
FILE *f = mustOpen(outPsl, "w");
struct psl *psl;
char cloneName[128];
int total = 0, cut = 0;

buildCutHash(cutList, cutHash);
pslWriteHead(f);
while ((psl = pslNext(lf)) != NULL)
    {
    fragToCloneName(psl->tName, cloneName);
    if (!hashLookup(cutHash, cloneName))
	{
        pslTabOut(psl, f);
	}
    else
        ++cut;
    ++total;
    pslFree(&psl);
    }
printf("Cut %d of %d\n", cut, total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
pslCut(argv[1], argv[2], argv[3]);
return 0;
}
