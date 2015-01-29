/* testTagStorm - Test speed of loading tag storms.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "tagStorm.h"
#include "errAbort.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testTagStorm - Test speed of loading tag storms.\n"
  "usage:\n"
  "   testTagStorm input.ra\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void testTagStorm(char *input, char *output)
{
struct tagStorm *tagStorm = tagStormFromFile(input);
uglyf("%d high level tags in %s\n", slCount(tagStorm->forest), tagStorm->fileName);
struct hash *fileHash = tagStormIndex(tagStorm, "file");
uglyf("Got %d stanzas with file\n", fileHash->elCount);
#ifdef SOON
struct hash *qualityHash = tagStormIndex(tagStorm, "lab_kent_quality");
uglyf("Got %d stanzas with quality\n", qualityHash->elCount);
struct hash *sexHash = tagStormIndex(tagStorm, "sex");
uglyf("Got %d stanzas with sex\n", sexHash->elCount);
struct hash *labHash = tagStormIndex(tagStorm, "lab");
uglyf("Got %d stanzas with lab\n", labHash->elCount);
#endif /* SOON */
tagStormWriteAll(tagStorm, output);
tagStormFree(&tagStorm);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
testTagStorm(argv[1], argv[2]);
return 0;
}
