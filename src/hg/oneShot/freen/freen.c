/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "psl.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen in.psl out.psl");
}

void freen(char *in, char *out)
/* Print status code. */
{
struct psl *psl, *pslList = pslLoadAll(in);
FILE *f = mustOpen(out, "w");
char buf[256];

uglyf("Loaded %s\n", in);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    sprintf(buf, "mm2.%s", psl->qName);
    psl->qName = buf;
    pslTabOut(psl, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
}
