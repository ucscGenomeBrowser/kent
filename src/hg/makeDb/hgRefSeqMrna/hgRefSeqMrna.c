/* hgRefSeqMrna - Load refSeq mRNA alignments and other info into genieKnown table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgRefSeqMrna - Load refSeq mRNA alignments and other info into genieKnown table\n"
  "usage:\n"
  "   hgRefSeqMrna XXX\n"
  "options:\n"
  "   -xxx=XXX\n");
}

void hgRefSeqMrna(char *XXX)
/* hgRefSeqMrna - Load refSeq mRNA alignments and other info into genieKnown table. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgRefSeqMrna(argv[1]);
return 0;
}
