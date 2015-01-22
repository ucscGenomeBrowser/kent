/* metaReformat - Do basic reformating of meta ra format file.  Add or subtract parents, 
 * change indenting. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "meta.h"

int indent = META_DEFAULT_INDENT;
boolean withParent = FALSE;
int depth = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaReformat - Do basic reformating of meta ra format file.  Add or subtract parents, change\n"
  "indenting.\n"
  "usage:\n"
  "   metaReformat input output\n"
  "options:\n"
  "   -withParent - if set include parent tag in output\n"
  "   -indent=N - sets indentation level\n"
  "   -depth=N - if set restricts output to top N levels (root is level 1)\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"withParent", OPTION_BOOLEAN},
   {"indent", OPTION_INT},
   {"depth", OPTION_INT},
   {NULL, 0},
};

void metaReformat(char *input, char *output)
/* metaReformat - Do basic reformating of meta ra format file.  Add or subtract parents, change 
 * indenting. */
{
struct meta *metaList = metaLoadAll(input, "meta", "parent", FALSE, FALSE);
metaWriteAll(metaList, output, indent, withParent, depth);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
indent = optionInt("indent", indent);
depth = optionInt("depth", depth);
withParent = optionExists("withParent");
metaReformat(argv[1], argv[2]);
return 0;
}
