/* metaReformat - Do basic reformating of meta ra format file.  Add or subtract parents, 
 * change indenting. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "meta.h"

int indent = 3;
boolean withParent = FALSE;

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
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"withParent", OPTION_BOOLEAN},
   {"indent", OPTION_INT},
   {NULL, 0},
};

void metaReformat(char *input, char *output)
/* metaReformat - Do basic reformating of meta ra format file.  Add or subtract parents, change 
 * indenting. */
{
struct meta *metaList = metaLoadAll(input, "meta", "parent", FALSE, FALSE);
metaWriteAll(metaList, output, indent, withParent);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
indent = optionInt("indent", indent);
withParent = optionExists("withParent");
metaReformat(argv[1], argv[2]);
return 0;
}
