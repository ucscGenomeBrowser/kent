/* testTagStorm - Test speed of loading tag storms.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "tagStorm.h"
#include "errAbort.h"

boolean withParent = FALSE;
boolean flatten = FALSE;
boolean tab = FALSE;
char *idTag = NULL;
int maxDepth = BIGNUM;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testTagStorm - Test speed of loading tag storms.\n"
  "usage:\n"
  "   testTagStorm input output\n"
  "options:\n"
  "   -withParent  If set a parent tag will be added. You'll need the idTag option too\n"
  "   -idTag=name Name of tag to used as primary identifier\n"
  "   -maxDepth=n Maximum depth of storm to write\n"
  "   -tab   If set will be output in tab-separated format\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"withParent", OPTION_BOOLEAN},
   {"idTag", OPTION_STRING},
   {"maxDepth", OPTION_INT},
   {"flatten", OPTION_BOOLEAN},
   {"tab", OPTION_BOOLEAN},
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
if (flatten)
    tagStormWriteAsFlatRa(tagStorm, output, idTag, withParent, maxDepth);
else if (tab)
    tagStormWriteAsFlatTab(tagStorm, output, idTag, withParent, maxDepth);
else
    tagStormWrite(tagStorm, output, maxDepth);
tagStormFree(&tagStorm);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
idTag = optionVal("idTag", idTag);
withParent = optionExists("withParent");
flatten = optionExists("flatten");
tab = optionExists("tab");
if (withParent && !idTag)
    errAbort("Please specify idTag option if using withParent option");
maxDepth = optionInt("maxDepth", maxDepth);
testTagStorm(argv[1], argv[2]);
return 0;
}
