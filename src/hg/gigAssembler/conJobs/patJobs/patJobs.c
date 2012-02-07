/* patJobs - reads an ls -l file of .fa files and turns
 * them into a list of aliGlue jobs. */

#include "common.h"


void usage()
{
errAbort("patJobs bacLsFile bacDir");
}

struct patJob
	{
	struct patJob *next;
	struct slName *bacs;
	int size;
	int jobIx;
	};

int jobCount = 0;

struct patJob *jobList = NULL;

void finishJob(struct slName **pBacs, int size)
/* Make job for a list of bacs. */
{
struct slName *bacs;
int count;
struct patJob *pj;

slReverse(pBacs);
bacs = *pBacs;
count = slCount(bacs);
printf("%d bacs totalling %d bases\n", count, size);
*pBacs = NULL;

AllocVar(pj);
pj->bacs = bacs;
pj->size = size;
pj->jobIx = jobCount++;
slAddHead(&jobList, pj);
}

#ifdef OLD
void writeJobs(char *jobDir, char *inDir, int machStart, int machEnd, char *machRoot)
{
int jobCount;
int machCount = machEnd - machStart;
int setCount;
int machIx;
struct patJob *pj;
int jobsAssigned = 0;

slReverse(&jobList);
jobCount = slCount(jobList);
setCount = (jobCount + machCount-1)/machCount;
uglyf("jobCount %d setCount %d machCOunt %d\n", jobCount, setCount, machCount);

pj = jobList;
for (machIx = machStart; machIx < machEnd; ++machIx)
    {
    char machName[32];
    char jobName[64];
    FILE *jobFile;
    int i;
    int mi = machIx-machStart;

    setCount = ((mi+1)*jobCount)/machCount - (mi*jobCount)/machCount;
    if (pj == NULL)
	break;
    sprintf(machName, "%s%02d", machRoot, machIx);
    sprintf(jobName, "%s/%s.sh", jobDir, machName);
    printf("Preparing jobs for %s\n", machName);
    jobFile = mustOpen(jobName, "w");
    fprintf(jobFile, "cd /var/tmp/jk\n");
    for (i=0; i<setCount && pj != NULL; ++i, pj = pj->next)
	{
	struct slName *bn;
	char geneListName[512];
	FILE *f;
	int jobIx = pj->jobIx;

	/* Create a file that is list of BACs for this job. */
	++jobsAssigned;
	sprintf(geneListName, "%s/%d.lst", inDir, jobIx);
	f = mustOpen(geneListName, "w");
	for (bn = pj->bacs; bn != NULL; bn = bn->next)
	    fprintf(f, "../h/c22chunks/%s\n", bn->name);
	fclose(f);
	}
    fclose(jobFile);
    }
assert(jobsAssigned == jobCount);
}
#endif /* OLD */

void writeInLists(char *listFileDir, char *targetSeqDir)
/* Write out a bunch of .in files. */
{
struct patJob *pj;
for (pj = jobList; pj != NULL; pj = pj->next)
    {
    struct slName *bn;
    int jobIx = pj->jobIx;
    char listFileName[512];
    FILE *f;

    sprintf(listFileName, "%s/%d.in", listFileDir, jobIx);
    f = mustOpen(listFileName, "w");
    for (bn = pj->bacs; bn != NULL; bn = bn->next)
	fprintf(f, "%s/%s\n", targetSeqDir, bn->name);
    fclose(f);
    }
}

int main(int argc, char *argv[])
{
char *inName, *name;
int chunkSize = 4048*1024;
FILE *in;
int accSize = 0;
int newAccSize;
int oneSize;
char line[512];
int lineCount;
char *words[16];
int wordCount;
struct slName *bacs = NULL, *bn;
char *dirName;
char *outDir;

if (argc != 4)
    usage();
inName = argv[1];
dirName = argv[2];
outDir = argv[3];


in = mustOpen(inName, "r");

while (fgets(line, sizeof(line), in))
    {
    char *sizeString;
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
	continue;
    if (wordCount != 9)
	errAbort("Line %d of %s doesn't look like an ls -l line", lineCount, inName);
    sizeString = words[4];
    if (!isdigit(sizeString[0]))
	errAbort("Line %d of %s doesn't look like an ls - l line", lineCount, inName);
    name = words[8];
    oneSize = atoi(sizeString);
    newAccSize = accSize + oneSize;
    if (newAccSize > chunkSize)
	{
	finishJob(&bacs, accSize);
	accSize = oneSize;
	if (oneSize > chunkSize)
	    warn("Size %d of %s exceed chunk size %d", oneSize, name, chunkSize);
	}
    else
	{
	accSize = newAccSize;
	}
    bn = newSlName(name);
    slAddHead(&bacs, bn);
    }
if (bacs != NULL)
    finishJob(&bacs, accSize);
printf("%d total jobs\n", jobCount);
writeInLists(outDir, dirName);
//writeJobs("job", "in", startMachine, stopMachine, "cc");
}
