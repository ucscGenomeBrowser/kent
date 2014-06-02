/* splitSim - Simulate gapless distribution size. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "splitSim - Simulate gapless distribution size\n"
  "usage:\n"
  "   splitSim XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

#define countCount 300
int counts[countCount];
int simSize = 1000001;
double aveSize = 30;

void splitSim(char *XXX)
/* splitSim - Simulate gapless distribution size. */
{
int splitCount = simSize / aveSize;
bool *splitBuf = needMem(simSize+1);
int start = 0, size;
int i;
uglyf("split count %d\n", splitCount);
for (i=0; i<splitCount; ++i)
     {
     for (;;)
	 {
	 int rPos = random()%simSize;
	 if (splitBuf[rPos] == 0)
	     {
	     splitBuf[rPos] = 1;
	     break;
	     }
	 }
     }
splitBuf[simSize] = 1;
start = 0;
for (i=0; i<=simSize; ++i)
    {
    if (splitBuf[i])
        {
	size = i - start;
	if (size < countCount)
	     counts[size] += 1;
	start = i;
	}
    }
for (i=0; i<countCount; ++i)
    printf("%3d %6d\n", i, counts[i]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
splitSim(argv[1]);
return 0;
}
