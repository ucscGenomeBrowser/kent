/* dumpNib - Print a nib file. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"


void dumpNib(char *nibFile, char *outFile)
/* dumpNib - Print a nib file. */
{
struct dnaSeq *seq;

seq = nibLoadAll(nibFile);
faWrite(outFile, "test", seq->dna, seq->size);

}

int main(int argc, char *argv[])
/* Process command line. */
{
dumpNib(argv[1], argv[2]);
return 0;
}
