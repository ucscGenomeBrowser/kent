/* patJobs - reads an ls -l file of .fa files and turns
 * them into a list of patSpace jobs. */

#include "common.h"


void usage()
{
errAbort("patJobs in serial.sh ");
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

void finishJob(struct slName **pBacs, int size, FILE *out)
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

void writeJobs(char *jobDir, char *inDir, FILE *serial, int machStart, int machEnd, char *machRoot)
{
int jobCount;
int machCount = machEnd - machStart;
int setCount;
int machIx;
struct patJob *pj;

slReverse(&jobList);
jobCount = slCount(jobList);
setCount = (jobCount + machCount-1)/machCount;

pj = jobList;
for (machIx = machStart; machIx < machEnd; ++machIx)
    {
    char machName[32];
    char jobName[64];
    FILE *jobFile;
    int i;

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
	sprintf(geneListName, "%s/%d.lst", inDir, jobIx);
	f = mustOpen(geneListName, "w");
	for (bn = pj->bacs; bn != NULL; bn = bn->next)
	    fprintf(f, "in/bac/%s\n", bn->name);
	fclose(f);

	/* Write out job as command for this processor. */
	fprintf(jobFile, 
	  "~kent/bin/i386/patSpace in/%d.lst in/mrna.lst in/htg.ooc out/%d.po out/%d.mo\n",
	  jobIx, jobIx, jobIx);

	/* Write out copy jobs to main serial batch file. */
	fprintf(serial, "echo copying job %d to %s\n", jobIx, machName);
	fprintf(serial, "rcp in/%d.lst %s:/var/tmp/jk/in/%d.lst\n",
		jobIx, machName, jobIx);
#ifdef OLD
	for (bn = pj->bacs; bn != NULL; bn = bn->next)
	    {
	    fprintf(serial, 
	       "rcp /projects/gene/htg/fa/%s %s:/var/tmp/jk/in/bac/%s\n",
	       bn->name, machName, bn->name);
	    }
#endif /* OLD */
	}
    fclose(jobFile);
    }
}

int main(int argc, char *argv[])
{
char *inName, *serialName, *sizeString, *name;
int chunkSize = 3000000;
int startMachine = 1, stopMachine = 18;
FILE *in, *out;
int accSize = 0;
int newAccSize;
int oneSize;
char line[512];
int lineCount;
char *words[16];
int wordCount;
struct slName *bacs = NULL, *bn;

if (argc != 3)
    usage();
inName = argv[1];
serialName = argv[2];

if (!isdigit(sizeString[0]))
    usage();
chunkSize = atoi(sizeString);

in = mustOpen(inName, "r");
out = mustOpen(serialName, "w");

while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
	continue;
    if (wordCount != 9)
	errAbort("Line %d of %s doesn't look like and ls -l line", lineCount, inName);
    sizeString = words[4];
    if (!isdigit(sizeString[0]))
	errAbort("Line %d of %s doesn't look like an ls - l line", lineCount, inName);
    name = words[8];
    oneSize = atoi(sizeString);
    newAccSize = accSize + oneSize;
    if (newAccSize > chunkSize)
	{
	finishJob(&bacs, accSize, out);
	accSize = oneSize;
	if (oneSize > chunkSize)
	    errAbort("Size %d of %s exceed chunk size %d", oneSize, name, chunkSize);
	}
    else
	{
	accSize = newAccSize;
	}
    bn = newSlName(name);
    slAddHead(&bacs, bn);
    }
if (bacs != NULL)
    finishJob(&bacs, accSize, out);
printf("%d total jobs\n", jobCount);
writeJobs("jobs", "in", out, startMachine, stopMachine, "cc");
}
