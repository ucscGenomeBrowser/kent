/* faNoise - Add noise to .fa file. */
#include "common.h"
#include "dnautil.h"
#include "options.h"
#include "fa.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faNoise - Add noise to .fa file\n"
  "usage:\n"
  "   faNoise inName outName transitionPpt transversionPpt insertPpt deletePpt chimeraPpt\n"
  "options:\n"
  "   -upper - output in upper case\n");
}

bool allUpper = FALSE;

char randomBase()
/* Return a random base. */
{
return valToNt[rand()&3];
}

int randomPpt()
/* Random number between 0 and 1000 */
{
return rand()%1001;
}

DNA transition(DNA base)
/* Swap a & g or c & t */
{
switch (base)
   {
   case 'a':
      return 'g';
   case 'c':
      return 't';
   case 'g':
      return 'a';
   case 't':
      return 'c';
   default:
      return 'n';
   }
}


DNA transversion(DNA base)
/* Swap a&g or c&t */
{
DNA newBase;
for (;;)
    {
    newBase = randomBase();
    if (newBase != base && newBase != transition(base))
        return newBase;
    }
}

void faNoise(char *inName, char *outName, int transitionPpt, 
	int transversionPpt, int insertPpt,
   	int deletePpt, int chimeraPpt)
/* faNoise - Add noise to .fa file. */
{
FILE *in = mustOpen(inName, "r");
FILE *out = mustOpen(outName, "w");
int dnaSize;
DNA *dna;
char *name;

srand(time(NULL));
while (faFastReadNext(in, &dna, &dnaSize, &name))
    {
    int tsCount = transitionPpt * dnaSize / 1000;
    int tvCount = transversionPpt * dnaSize / 1000;
    int delCount = deletePpt * dnaSize / 1000;
    int insCount = insertPpt * dnaSize / 1000;
    int pos;
    int newSize = dnaSize;
    int maxSize = dnaSize*10;
    DNA *newDna = needMem(maxSize+1);
    bool *randHit = needMem(dnaSize);
    memcpy(newDna, dna, dnaSize);

    /* Do transitions. */
    while (tsCount > 0)
        {
	pos = rand() % dnaSize;
	if (!randHit[pos])
	    {
	    randHit[pos] = TRUE;
	    newDna[pos] = transition(newDna[pos]);
	    --tsCount;
	    }
	}

    /* Do transversions */
    while (tvCount > 0)
        {
	pos = rand() % dnaSize;
	if (!randHit[pos])
	    {
	    randHit[pos] = TRUE;
	    newDna[pos] = transversion(newDna[pos]);
	    --tvCount;
	    }
	}


    /* Do deletions */
    while (--delCount >= 0)
        {
	pos = rand() % newSize;
	memcpy(newDna+pos, newDna+pos+1, newSize - pos);
	newSize -= 1;
	}

    /* Do insertions */
    while (--insCount >= 0)
        {
	if (newSize + 1 < maxSize)
	    {
	    pos = rand() % newSize;
	    memcpy(newDna+pos+1, newDna+pos, newSize - pos);
	    newDna[pos] = randomBase();
	    ++newSize;
	    }
	}
    if (allUpper)
        toUpperN(newDna, newSize);
    faWriteNext(out, name, newDna, newSize);

    freez(&newDna);
    freez(&randHit);
    }
}


int makePpt(char *s)
/* Convert ascii to part-per-thousand, checking range. */
{
int ppt;

if (!isdigit(s[0]))
    usage();
ppt = atoi(s);
if (ppt < 0 || ppt > 1000)
    errAbort("Expecting number between 0 and 1000 in ppt parameter");
return ppt;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 8)
    usage();
allUpper = optionExists("upper");
dnaUtilOpen();
faNoise(argv[1], argv[2], makePpt(argv[3]), makePpt(argv[4]),
	makePpt(argv[5]), makePpt(argv[6]), makePpt(argv[7]));
return 0;
}
