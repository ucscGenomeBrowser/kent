/* faRc - Reverse complement a FA file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
   "faRc - Reverse complement a FA file\n"
   "usage:\n"
   "   faRc in.fa out.fa\n"
   "In.fa and out.fa may be the same file.\n"
   "options:\n"
   "   -keepName - keep name identical (don't prepend RC)\n"
   "   -keepCase - works well for ACGTUN in either case. bizarre for other letters.\n"
   "               without it bases are turned to lower, all else to n's\n"
   "   -justReverse - prepends R unless asked to keep name\n"
   "   -justComplement - prepends C unless asked to keep name\n"
   "                     (cannot appear together with -justReverse)\n"
   );
}

void faRc(char *in, char *out, boolean revOnly, boolean compOnly)
/* faRc - Reverse complement a FA file. */
{
struct dnaSeq *seqList = NULL, *seq;
FILE *f;
char buf[512];
char *prefix = (cgiBoolean("keepName") ? "" : (revOnly ? "R_" : (compOnly ? "C_" : "RC_")));

if (cgiBoolean("keepCase"))
    seqList = faReadAllMixed(in);
else
    seqList = faReadAllDna(in);
f = mustOpen(out, "w");
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    if (revOnly)
	reverseBytes(seq->dna, seq->size);
    else if (compOnly)
	complement(seq->dna, seq->size);
    else
	reverseComplement(seq->dna, seq->size);
    sprintf(buf, "%s%s", prefix, seq->name);
    faWriteNext(f, buf,  seq->dna, seq->size);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean revOnly, compOnly;

cgiSpoof(&argc, argv);
revOnly  = cgiBoolean("justReverse");
compOnly = cgiBoolean("justComplement");
if (argc != 3 || (revOnly && compOnly))
usage();
faRc(argv[1], argv[2], revOnly, compOnly);
return 0;
}
