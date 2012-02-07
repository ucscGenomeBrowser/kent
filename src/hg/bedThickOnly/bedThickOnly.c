/* bedThickOnly - Reduce bed to just thick parts.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedThickOnly - Reduce bed to just thick parts.\n"
  "usage:\n"
  "   bedThickOnly in.bed out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void thickOnly(char *inBed, char *outBed)
/* thickOnly - Reduce bed to just thick parts.. */
{
struct lineFile *lf = lineFileOpen(inBed, TRUE);
FILE *f = mustOpen(outBed, "w");
char *row[bedKnownFields+1];
int firstSize = 0;
int size;
while ((size = lineFileChop(lf, row)) != 0)
    {
    if (firstSize == 0)
	{
	if (size > bedKnownFields)
	    errAbort("Too many fields (%d) in bed", size);
	else if (size < 3)
	    errAbort("Not enough fields (%d) in bed", size);
        firstSize = size;
	}
    else
        {
	if (size != firstSize)
	    lineFileExpectWords(lf, firstSize, size);
	}
    struct bed *in = bedLoadN(row, size);
    struct bed *out = bedThickOnly(in);
    if (out != NULL)
	{
	bedTabOutN(out, size, f);
	bedFree(&out);
	}
    bedFree(&in);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
thickOnly(argv[1], argv[2]);
return 0;
}
