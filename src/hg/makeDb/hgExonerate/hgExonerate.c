/* hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.\n"
  "usage:\n"
  "   hgExonerate XXX\n");
}

void hgExonerate(char *XXX)
/* hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
hgExonerate(argv[1]);
return 0;
}
