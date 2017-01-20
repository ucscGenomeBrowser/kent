/* tagStormHoist - Move tags that are shared by all siblings up a level. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormHoist - Move tags that are shared by all siblings up a level\n"
  "usage:\n"
  "   tagStormHoist input output\n"
  "options:\n"
  "   -one=tagName - only hoist tags of the given type\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"one", OPTION_STRING},
   {NULL, 0},
};

void hoistFile(char *input, char *output, char *selectedTag)
/* tagStormHoist - Move tags that are shared by all siblings up a level. */
{
struct tagStorm *tagStorm = tagStormFromFile(input);
tagStormHoist(tagStorm, selectedTag);
tagStormWrite(tagStorm, output, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hoistFile(argv[1], argv[2], optionVal("one", NULL));
return 0;
}
