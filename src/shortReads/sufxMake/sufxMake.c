/* sufxMake - Create an extended suffix tree.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sufa.h"

static char const rcsid[] = "$Id: sufxMake.c,v 1.1 2008/10/26 09:34:37 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufxMake - Create an extended suffix tree - one that can be searched without\n"
  "resorting to a binary search so much.\n"
  "usage:\n"
  "   sufxMake input.sufa output.sufx\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

bits32 *sufxCalcSearchExtensions(char *dna, bits32 *array, int arraySize)
/* Fill in the bits that will help us traverse the array as if it were a tree... */
{
bits32 *xArray;
AllocArray(xArray, arraySize);
int depth = 0;
int stackSize = 128;
int *stack;
AllocArray(stack, stackSize);
int i;
for (i=0; i<arraySize; ++i)
    {
    char *curDna = dna + array[i];
    int d;
    for (d = 0; d<depth; ++d)
        {
	int prevIx = stack[d];
	char *prevDna = dna + array[prevIx];
	if (curDna[d] != prevDna[d])
	    {
	    xArray[prevIx] = i - prevIx;
	    depth = d;
	    break;
	    }
	}
    if (depth >= stackSize)
        errAbort("Stack overflow");
    stack[depth] = i;
    depth += 1;
    }
return xArray;
}

void sufxMake(char *input, char *output)
/* sufxMake - Create an extended suffix tree.. */
{
struct sufa *sufa = sufaRead(input, FALSE);
uglyTime("Read %s", input);
FILE *f = mustOpen(output, "w");
int arraySize = sufa->header->basesIndexed;
char *allDna = sufa->allDna;
bits32 *array = sufa->array;
bits32 *xArray = sufxCalcSearchExtensions(allDna, array, arraySize);
uglyTime("Calculated extensions", input);

int i;
for (i=0; i<arraySize; ++i)
    {
    int offset = array[i];
    fprintf(f, "%d %d ", i, offset);
    mustWrite(f, allDna+offset, 50);
    fprintf(f, " %d\n", xArray[i]);
    }
carefulClose(&f);
uglyTime("wrote %s", output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
uglyTime(NULL);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
sufxMake(argv[1], argv[2]);
return 0;
}
