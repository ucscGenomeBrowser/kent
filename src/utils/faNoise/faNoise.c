/* faNoise - Add noise to .fa file. */
#include "common.h"
#include "dnautil.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faNoise - Add noise to .fa file\n"
  "usage:\n"
  "   faNoise inName outName misPairPpt insertPpt deletePpt chimeraPpt\n");
}

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

int linePos = 0;
int lineMaxSize = 50;

int charOut(FILE *f, char c)
/* Put out a char, wrapping to new line if necessary. */
{
fputc(c, f);
if (++linePos >= lineMaxSize)
   {
   fputc('\n', f);
   linePos = 0;
   }
}

void finishCharOut(FILE *f)
/* Write final newline and reset. */
{
if (linePos != 0)
    fputc('\n', f);
linePos = 0;
}

void faNoise(char *inName, char *outName, int misPairPpt, int insertPpt,
   int deletePpt, int chimeraPpt)
/* faNoise - Add noise to .fa file. */
{
FILE *in = mustOpen(inName, "r");
FILE *out = mustOpen(outName, "w");
int dnaSize;
int i;
DNA *dna;
char *name;

while (faFastReadNext(in, &dna, &dnaSize, &name))
    {
    fprintf(out, ">%s %4.1f mispair, %4.1f insert, %4.1f delete, %4.1f chimera\n",
        name, 0.1*misPairPpt, 0.1*insertPpt, 0.1*deletePpt, 0.1*chimeraPpt);
    if (randomPpt() < chimeraPpt)
        {
	int size = rand()%dnaSize;
	if (rand()&1)
	    reverseComplement(dna, size);
	else
	    reverseComplement(dna+dnaSize-size, size);
	}
    for (i = 0; i<dnaSize; ++i)
        {
	if (randomPpt() < insertPpt)
	    charOut(out, randomBase());
	if (randomPpt() >= deletePpt)
	    {
	    if (randomPpt() < misPairPpt)
	        charOut(out, randomBase());
	    else
	        charOut(out, dna[i]);
	    }
	}
    finishCharOut(out);
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
if (argc != 7)
    usage();
dnaUtilOpen();
faNoise(argv[1], argv[2], makePpt(argv[3]), makePpt(argv[4]),
	makePpt(argv[5]), makePpt(argv[6]));
return 0;
}
