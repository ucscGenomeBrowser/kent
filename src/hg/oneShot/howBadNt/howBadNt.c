/* howBadNt - Figure out how bad NT reversal screwup is. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "howBadNt - Figure out how bad NT reversal screwup is\n"
  "usage:\n"
  "   howBadNt ctg_coords\n");
}

void howBadNt(char *ntFile)
/* howBadNt - Figure out how bad NT reversal screwup is. */
{
char lastName[256];
char lastOrientation[16];
struct lineFile *lf = lineFileOpen(ntFile, TRUE);
char *row[8];
int pairCount = 0;
int revCount = 0;
int cloneCount = 0;

strcpy(lastName, "");
while (lineFileRow(lf, row))
    {
    ++cloneCount;
    if (sameString(row[0], lastName))
        {
	++pairCount;
	if (!sameString(row[4], lastOrientation))
	    {
	    ++revCount;
	    }
	}
    else
        {
	strcpy(lastName, row[0]);
	}
    strcpy(lastOrientation, row[4]);
    }
lineFileClose(&lf);
printf("%d clones, %d adjacencies, %d reversed (%4.3f%%)\n",
	cloneCount, pairCount, revCount, 100.0*revCount/(double)pairCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
howBadNt(argv[1]);
return 0;
}
