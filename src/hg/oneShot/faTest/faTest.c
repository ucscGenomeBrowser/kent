/* faTest - Test new fast fa routines. */
#include "common.h"
#include "dnaseq.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faTest - Test new fast fa routines\n"
  "usage:\n"
  "   faTest file.fa\n");
}

void faTest(char *faName)
/* faTest - Test new fast fa routines. */
{
struct dnaSeq *seqList, *seq;
seqList = faReadAllDna(faName);
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    faWriteNext(stdout, seq->name, seq->dna, seq->size);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
faTest(argv[1]);
return 0;
}
