/* fakeOut - fake a RepeatMasker .out file based on a N's in .fa file. */
#include "common.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fakeOut - fake a RepeatMasker .out file based on a N's in .fa file\n"
  "usage:\n"
  "   fakeOut x.fa.masked x.fa.out\n");
}

void fakeOut(char *inName,  char *outName)
/* fakeOut - fake a RepeatMasker .out file based on a N's in .fa file. */
{
FILE *out = mustOpen(outName, "w");
FILE *in = mustOpen(inName, "r");
DNA *dna;
int dnaSize;
char *name;

fprintf(out,
 "   SW  perc perc perc  query     position in query            matching       repeat         position in  repeat\n"
 "score  div. del. ins.  sequence    begin     end    (left)    repeat         class/family    begin   end (left)   ID\n"
 "\n");

while (faFastReadNext(in, &dna, &dnaSize, &name))
    {
    int start = 0, end = 0;
    int i;
    boolean n, lastN = TRUE;

    dna[dnaSize] = 'n';		/* Replace 0 with 'n' to make end condition not a special case. */
    for (i=0; i<=dnaSize; ++i)
        {
	n = (dna[i] == 'n');
	if (n != lastN)
	    {
	    if (n)
	        start = i;
	    else
	        {
		end = i;
		if (i != 0)
		    fprintf(out, " 1000  15.0  2.0  2.0 %-9s  %7d %7d (1234567) +  faked          fake              1     100      1\n",
			name, start+1, end);
		}
	    lastN = n;
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
fakeOut(argv[1], argv[2]);
return 0;
}
