/* pslPartition - split PSL files into non-overlapping sets */
#include "common.h"
#include "options.h"
#include "pipeline.h"
#include "psl.h"
#include "dystring.h"
#include "portable.h"

static char const rcsid[] = "$Id: pslPartition.c,v 1.5 2006/08/04 06:00:20 markd Exp $";

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"outLevels", OPTION_INT},
    {"partSize", OPTION_INT},
    {NULL, 0}
};
int gOutLevels = 0;
int gPartSize = 20000;

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("Error: %s\n"
  "pslPartition - split PSL files into non-overlapping sets\n"
  "usage:\n"
  "   pslPartition [options] pslFile outDir\n"
  "\n"
  "Split psl files into non-overlapping sets for use in cluster jobs,\n"
  "limiting memory usage, etc. Multiple levels of directories can be are\n"
  "created under outDir to prevent slow access to huge directories.\n"
  "The pslFile maybe compressed and no ordering is assumed.\n"
  "\n"
  "options:\n"
  "  -outLevels=0 - number of output subdirectory levels.  0 puts all files\n"
  "   directly in outDir, 2, will create files in the form outDir/0/0/00.psl\n"
  "  -partSize=20000 - will combine non-overlapping partitions, while attempting\n"
  "   to keep them under this number of PSLs.  This reduces the number of\n"
  "   files that are created while ensuring that there are no overlaps\n"
  "   between any two PSL files.  A value of 0 creates a PSL file per set of\n"
  "   overlapping PSLs.\n"
  "\n", msg);
}

struct pslInput
/* object to read a psl */
{
    struct pipeline *pl;     /* sorting pipeline */
    struct lineFile *lf;     /* lineFile to pipeline */
    struct psl *pending;     /* next psl to read, if not NULL */
};

struct pipeline *openPslSortPipe(char *pslFile)
/* open pipeline that sorts psl */
{
static char *zcatCmd[] = {"zcat", NULL};
static char *sortCmd[] = {"sort", "-k", "14,14", "-k", "16,16n", "-k", "17,17nr", NULL};
int iCmd = 0;
char **cmds[3];

if (endsWith(pslFile, ".gz") || endsWith(pslFile, ".Z"))
    cmds[iCmd++] = zcatCmd;
cmds[iCmd++] = sortCmd;
cmds[iCmd++] = NULL;

return pipelineOpen(cmds, pipelineRead, pslFile, NULL);
}

struct pslInput *pslInputNew(char *pslFile)
/* create object to read PSLs */
{
struct pslInput *pi;
AllocVar(pi);
pi->pl = openPslSortPipe(pslFile);
pi->lf = pipelineLineFile(pi->pl);
return pi;
}

void pslInputFree(struct pslInput **piPtr)
/* free pslInput object */
{
struct pslInput *pi = *piPtr;
if (pi != NULL)
    {
    assert(pi->pending == NULL);
    pipelineWait(pi->pl);
    pipelineFree(&pi->pl);
    freez(piPtr);
    }
}

struct psl *pslInputNext(struct pslInput *pi)
/* read next psl */
{
struct psl *psl = pi->pending;
if (psl != NULL)
    pi->pending = NULL;
else
    psl = pslNext(pi->lf);
return psl;
}

void pslInputPutBack(struct pslInput *pi, struct psl *psl)
/* save psl pending slot. */
{
if (pi->pending)
    errAbort("only one psl maybe pending");
pi->pending = psl;
}

boolean isOverlapped(struct psl *psl, struct psl *pslPart,
                     int minStart, int maxEnd)
/* determine if a psl is in the partation */
{
return (psl->tStart < maxEnd) && (psl->tEnd > minStart)
    && sameString(psl->tName, pslPart->tName);
}

struct psl *readPartition(struct pslInput *pi)
/* read next set of overlapping psls */
{
int minStart, maxEnd;
struct psl *pslPart = pslInputNext(pi);
struct psl *psl;

if (pslPart == NULL)
    return NULL;  /* no more */
minStart = pslPart->tStart;
maxEnd = pslPart->tEnd;

/* add more psls while they overlap, easy since input is sorted */
while ((psl = pslInputNext(pi)) != NULL)
    {
    if (isOverlapped(psl, pslPart, minStart, maxEnd))
        {
        minStart = min(minStart, psl->tStart);
        maxEnd = max(maxEnd, psl->tEnd);
        slAddHead(&pslPart, psl);
        }
    else
        {
        pslInputPutBack(pi, psl);
        break;
        }
    }
slReverse(&pslPart);
return pslPart;
}

char *getPartPslFile(char *outDir, int partNum)
/* compute the name for the partition psl file, creating directories.
 * freeMem result */
{
struct dyString *partPath = dyStringNew(128);
char partStr[64];
int i, partStrLen;

/* part number, with leading zeros */
safef(partStr, sizeof(partStr), "%0*d", gOutLevels, partNum);
partStrLen = strlen(partStr);

dyStringAppend(partPath, outDir);
makeDir(partPath->string);

/* create with multiple levels of directories, with least-signficant part of
 * number being lowest directory */
for (i = 0; i < gOutLevels; i++)
    {
    dyStringAppendC(partPath, '/');
    dyStringAppendC(partPath, partStr[(partStrLen-gOutLevels)+i]);
    makeDir(partPath->string);
    }
dyStringAppendC(partPath, '/');
dyStringAppend(partPath, partStr);
dyStringAppend(partPath, ".psl");

return dyStringCannibalize(&partPath);
}

struct pslParts
/* object for accumulating and combining partitions */
{
    struct psl *psls;   /* list of psls */
    int size;           /* number of psls */
    int partNum;        /* next partition number */
};


void pslPartsWrite(struct pslParts *parts, char *outDir)
/* write out a set of partitions and reset stated to empty. */
{
char *partPath = getPartPslFile(outDir, parts->partNum++);
pslWriteAll(parts->psls, partPath, FALSE);
freeMem(partPath);
pslFreeList(&parts->psls);
parts->size = 0;
}

void pslPartsAdd(struct pslParts *parts, struct psl* newPart, char *outDir)
/* add a new partition, writing out pending parts if max size reached,
 * and adding new ones. */
{
int newSize = slCount(newPart);
if (((parts->size + newSize) > gPartSize) && (parts->psls != NULL))
    pslPartsWrite(parts, outDir);
parts->psls = slCat(parts->psls, newPart);
parts->size += newSize;
}

void pslPartition(char *pslFile, char *outDir)
/* split PSL files into non-overlapping sets */
{
struct pslInput *pi = pslInputNew(pslFile);
struct pslParts parts;
struct psl *newPart;
ZeroVar(&parts);

while ((newPart = readPartition(pi)) != NULL)
    pslPartsAdd(&parts, newPart, outDir);
if (parts.psls != NULL)
    pslPartsWrite(&parts, outDir);
pslInputFree(&pi);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage("wrong # args");
gOutLevels = optionInt("outLevels", gOutLevels);
gPartSize = optionInt("partSize", gPartSize);
pslPartition(argv[1], argv[2]);
return 0;
}
