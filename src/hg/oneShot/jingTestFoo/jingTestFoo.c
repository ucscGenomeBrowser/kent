/* jingTestFoo - This is a program just to test newProg, and to do little non-repeating tests for Jing.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: jingTestFoo.c,v 1.1 2007/08/01 19:23:11 jzhu Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "jingTestFoo - This is a program just to test newProg, and to do little non-repeating tests for Jing.\n"
  "usage:\n"
  "   jingTestFoo input output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void jingTestFoo(char *input, char *output)
/* jingTestFoo - This is a program just to test newProg, and to do little non-repeating tests for Jing.. */
{
printf("Input is %s\n", input);
printf("Output is %s\n", output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
jingTestFoo(argv[1], argv[2]);
return 0;
}
