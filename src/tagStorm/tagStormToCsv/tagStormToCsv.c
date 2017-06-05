/* tagStormToCsv - convert a tagStorm to a comma separated file properly escaped. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormToCsv - Convert tagStorm to comma-separated-value file.\n"
  "usage:\n"
  "   tagStormToCsv tagStorm.txt output.tsv\n"
  "options:\n"
  "   -mid - include non-leaf middle and root nodes in output\n"
  "   -null=string - print string for empty cells, defaults to empty string\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"mid", OPTION_BOOLEAN},
   {"null", OPTION_STRING},
   {NULL, 0},
};

void tagStormToCsv(char *input, char *output, boolean mid, char *nullVal)
/* tagStormToTab - Convert tagStorm to tab-separated-value file.. */
{
struct tagStorm *tagStorm = tagStormFromFile(input);
verbose(1, "%d trees in %s\n", slCount(tagStorm->forest), tagStorm->fileName);
tagStormWriteAsFlatCsv(tagStorm, output, NULL, FALSE, 0, !mid, nullVal);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tagStormToCsv(argv[1], argv[2], optionExists("mid"), optionVal("null", ""));
return 0;
}
