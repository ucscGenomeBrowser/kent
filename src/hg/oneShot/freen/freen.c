/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "axt.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen in.axt out.axt");
}

void freen(char *in, char *out)
/* Print status code. */
{
struct axt *axt;
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
char buf[256];

while ((axt = axtRead(lf)) != NULL)
    {
    if (!sameString(axt->qName, "chr2"))
       axtWrite(axt, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
}
