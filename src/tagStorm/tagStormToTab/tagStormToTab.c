/* tagStormToTab - Convert tagStorm to tab-separated-value file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormToTab - Convert tagStorm to tab-separated-value file.\n"
  "usage:\n"
  "   tagStormToTab tagStorm.txt output.tsv\n"
  "options:\n"
  "   -mid - include non-leaf middle and root nodes in output\n"
  "   -null=string - print string for empty cells, defaults to empty string\n"
  "   -noSharp - suppress # character at front of label line\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"mid", OPTION_BOOLEAN},
   {"null", OPTION_STRING},
   {"noSharp", OPTION_BOOLEAN},
   {NULL, 0},
};

void tagStormToTab(char *input, char *output, boolean mid, char *nullVal, boolean noSharp)
/* tagStormToTab - Convert tagStorm to tab-separated-value file.. */
{
struct tagStorm *tagStorm = tagStormFromFile(input);
verbose(2, "%d trees in %s\n", slCount(tagStorm->forest), tagStorm->fileName);
tagStormWriteAsFlatTab(tagStorm, output, NULL, FALSE, 0, !mid, nullVal, !noSharp);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tagStormToTab(argv[1], argv[2], optionExists("mid"), optionVal("null", ""), 
    optionExists("noSharp"));
return 0;
}
