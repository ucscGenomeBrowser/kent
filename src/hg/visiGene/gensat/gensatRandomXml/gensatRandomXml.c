/* gensatRandomXml - Get some random xml records from gensat. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "xp.h"
#include "xap.h"
#include "obscure.h"
#include "../lib/gs.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensatRandomXml - Get some random xml records from gensat\n"
  "usage:\n"
  "   gensatRandomXml input.xml count output.xml\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void gensatRandomXml(char *input, char *countString, char *output)
/* gensatRandomXml - Get some random xml records from gensat. */
{
struct gsGensatImageSet *imageSet;
int outCount = atoi(countString);
struct gsGensatImage *image;
FILE *f;

if (outCount < 1)
    usage();

/* Load and shuffle list. */
imageSet = gsGensatImageSetLoad(input);
shuffleList(&imageSet->gsGensatImage);

/* Truncate list. */
image = slElementFromIx(imageSet->gsGensatImage, outCount-1);
if (image != NULL)
    image->next = NULL;

/* Save truncated list. */
f = mustOpen(output, "w");
gsGensatImageSetSave(imageSet, 0, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
gensatRandomXml(argv[1], argv[2], argv[3]);
return 0;
}
