/* faToTwoBit - Convert DNA from fasta to 2bit format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "fa.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: faToTwoBit.c,v 1.3 2004/02/23 06:49:10 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faToTwoBit - Convert DNA from fasta to 2bit format\n"
  "usage:\n"
  "   faToTwoBit in.fa [in2.fa in3.fa ...] out.2bit\n"
  "options:\n"
  "   -noMask - Ignore lower-case masking in fa file.\n"
  );
}

boolean noMask = FALSE;

static struct optionSpec options[] = {
   {"noMask", OPTION_BOOLEAN},
   {NULL, 0},
};

static void unknownToN(char *s, int size)
/* Convert non ACGT characters to N. */
{
char c;
int i;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (ntChars[c] == 0)
        {
	if (isupper(c))
	    s[i] = 'N';
	else
	    s[i] = 'n';
	}
    }
}

	    
void faToTwoBit(char *inFiles[], int inFileCount, char *outFile)
/* Convert inFiles in fasta format to outfile in 2 bit 
 * format. */
{
struct twoBit *twoBitList = NULL, *twoBit;
int i;
struct hash *uniqHash = newHash(18);
FILE *f;

for (i=0; i<inFileCount; ++i)
    {
    char *fileName = inFiles[i];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct dnaSeq seq;
    ZeroVar(&seq);
    while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
        {
	if (hashLookup(uniqHash, seq.name))
	    errAbort("Duplicate sequence name %s", seq.name);
	hashAdd(uniqHash, seq.name, NULL);
	if (noMask)
	    faToDna(seq.dna, seq.size);
	else
	    unknownToN(seq.dna, seq.size);
	twoBit = twoBitFromDnaSeq(&seq, !noMask);
	slAddHead(&twoBitList, twoBit);
	}
    }
slReverse(&twoBitList);
f = mustOpen(outFile, "wb");
twoBitWriteHeader(twoBitList, f);
for (twoBit = twoBitList; twoBit != NULL; twoBit = twoBit->next)
    {
    twoBitWriteOne(twoBit, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
noMask = optionExists("noMask");
dnaUtilOpen();
faToTwoBit(argv+1, argc-2, argv[argc-1]);
return 0;
}
