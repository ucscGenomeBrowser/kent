/* qaToQa - convert from compressed to uncompressed 
 * quality score format. */
#include "common.h"
#include "linefile.h"
#include "qaSeq.h"

static char const rcsid[] = "$Id: qacToQa.c,v 1.3 2003/10/10 16:59:22 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
"qacToQa - convert from compressed to uncompressed\n"
"quality score format.\n"
"usage:\n"
"   qacToQa in.qac out.qa");
}

void qacToQa(char *inName, char *outName)
/* qacToQa - convert from compressed to uncompressed 
 * quality score format. */
{
boolean isSwapped;
FILE *in = qacOpenVerify(inName, &isSwapped);
FILE *out = mustOpen(outName, "wb");
struct qaSeq *qa;

while ((qa = qacReadNext(in, isSwapped)) != NULL)
    {
    qaWriteNext(out, qa);
    qaSeqFree(&qa);
    }
fclose(in);
fclose(out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
qacToQa(argv[1], argv[2]);
return 0;
}
