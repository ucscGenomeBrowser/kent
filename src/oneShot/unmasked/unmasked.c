/* List files that aren't repeat masked. */
#include "common.h"
#include "portable.h"

static char *seqDirs[] = 
    {
    "/projects/compbio/experiments/hg/gs.1/fin/fa",
    "/projects/compbio/experiments/hg/gs.1/unfin/fa",
    "/projects/compbio/experiments/hg/gs.1/predraft/fa",
    "/projects/compbio/experiments/hg/gs.1/missing/fa",
    };

void usage()
/* Explain usage and exit. */
{
errAbort("unmasked = List files that aren't repeat masked.\n"
	 "usage:\n"
	 "   unmasked outAcc outFa\n");
}

void unmasked(char *outAccName, char *outFaName)
/* Write out unmasked files. */
{
int i;
char *seqDir;
struct slName *dirList, *dirEl;
char faName[512], outName[512];
char acc[64];
char buf[32];
int missingCount = 0;
int emptyCount = 0;
int badCount = 0;
int totalCount = 0;
FILE *badAcc = mustOpen(outAccName, "w");
FILE *badFa = mustOpen(outFaName, "w");
FILE *log = mustOpen("unmasked.log", "w");

for (i=0; i < ArraySize(seqDirs); ++i)
    {
    seqDir = seqDirs[i];
    printf("Scanning %s", seqDir);
    fflush(stdout);
    dirList = listDir(seqDir, "*.fa");
    /* Open each .out file and check that it looks ok. */
    for (dirEl = dirList; dirEl != NULL; dirEl = dirEl->next)
	{
	FILE *f;
	boolean ok = FALSE;
	strcpy(acc, dirEl->name);
	chopSuffix(acc);
	sprintf(faName, "%s/%s", seqDir, dirEl->name);
	sprintf(outName, "%s.out", faName);
	++totalCount;
	if ((totalCount & 0xff) == 0)
	    {
	    printf(".");
	    fflush(stdout);
	    }
	if ((f = fopen(outName, "r")) == NULL)
	    {
	    ++missingCount;
	    fprintf(log, "missing %s\n", outName);
	    }
	else
	    {
	    if (fgets(buf, sizeof(buf), f) == NULL)
		{
		++emptyCount;
		fprintf(log, "empty %s\n", outName);
		}
	    else
		{
		if (startsWith("There were no", buf))
		    {
		    ok = TRUE;
		    fprintf(log, "unique %s\n", outName);
		    }
		else if (startsWith("   SW", buf))
		    {
		    ok = TRUE;
		    fprintf(log, "normal %s\n", outName);
		    }
		else
		    {
		    ++badCount;
		    fprintf(log, "bad %s\n", outName);
		    }
		}
	    fclose(f);
	    }
	if (!ok)
	    {
	    fprintf(badAcc, "%s\n", acc);
	    fprintf(badFa, "%s\n", faName);
	    }
	}
    printf("\n");
    }
printf("%d missing %d empty %d bad %d total\n",
    missingCount, emptyCount, badCount, totalCount);
}

int main(int argc, char *argv[])
{
if (argc != 3)
    usage();
unmasked(argv[1], argv[2]);
}
