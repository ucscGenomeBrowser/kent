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
  );
}

void faRc(char *in, char *out)
/* faRc - Reverse complement a FA file. */
{
struct dnaSeq *seqList = NULL, *seq;
FILE *f;
char buf[512];
char *prefix = (cgiBoolean("keepName") ? "" : "RC_");

seqList = faReadAllDna(in);
f = mustOpen(out, "w");
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    reverseComplement(seq->dna, seq->size);
    sprintf(buf, "%s%s", prefix, seq->name);
    faWriteNext(f, buf,  seq->dna, seq->size);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
faRc(argv[1], argv[2]);
return 0;
}
