/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "dlist.h"
#include "fa.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen count");
}

void freen(char *s)
/* Print status code. */
{
struct lineFile *lf = lineFileOpen(s, TRUE);
static struct dnaSeq seq;
faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name);
printf("%s has %d bases in first seq\n", s, seq.size);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
