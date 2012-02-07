/* preProcGgfAndy - Print CpG locs/stats to stdout for Andy Law's cpg-island
 * script (modified to use G-G&F obs/exp calc). */
#include "common.h"
#include "dnaseq.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "preProcGgfAndy - Print CpG locs/stats to stdout for Andy Law's cpg-island\n"
  "   script (modified to use G-G&F obs/exp calc)\n"
  "usage:\n"
  "   preProcGgfAndy file.fa\n");
}

void preProcGgfAndy(char *faName)
/* preProcGgfAndy - Print CpG locs/stats to stdout for Andy Law's cpg-island 
 * script (modified to use G-G&F obs/exp calc). */
{
struct dnaSeq *seqList, *seq;
seqList = faReadAllDna(faName);
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    int i = 0;
    int numC=0, numG=0;
    int size = 0;
    boolean wasC = FALSE;
    for (i=0;  i < seq->size;  i++)
	{
	DNA base = seq->dna[i];
	boolean isC = (base == 'C') || (base == 'c');
	boolean isG = (base == 'G') || (base == 'g');
	if (isC)
	    numC++;
	if (isG)
	    numG++;
	size++;
	if (isG && wasC)
	    {
	    /* i is 1-based offset for previous base (CpG start). */
	    printf("%s\t%d\t%d\t%d\t%d\n", seq->name, i, numC, numG, size);
	    numC = numG = size = 0;
	    }
	wasC = isC;
	}    
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
preProcGgfAndy(argv[1]);
return 0;
}
