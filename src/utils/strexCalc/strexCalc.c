/* strexCalc - String expression calculator, mostly to test strex expression evaluator.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "verbose.h"
#include "strex.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "strexCalc - String expression calculator, mostly to test strex expression evaluator.\n"
  "usage:\n"
  "   strexCalc [variable assignments] expression\n"
  "command options in strexCalc are used to seed variables so for instance the command\n"
  "   strexCalc a=12 b=13 c=xyz 'a + b + c'\n"
  "ends up returning 1213xyz\n"
  );
}

char *symLookup(void *symbols, char *key)
{
return optionVal(key, NULL);
}

void warnHandler(void *symbols, char *warning)
/* Print warning message */
{
fprintf(stderr, "%s\n", warning);
}

void abortHandler(void *symbols)
/* Abort */
{
errAbort("Aborting");
}

void strexCalc(char *expression)
/* strexCalc - String expression calculator, mostly to test strex expression evaluator.. */
{
struct strexParse *parsed = strexParseString(expression, expression, 0, "options", symLookup);
if (verboseLevel() > 1)
    strexParseDump(parsed, 0, stderr);
char *result = strexEvalAsString(parsed, "options", symLookup, warnHandler, abortHandler);
printf("%s\n", result);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, NULL);
if (argc != 2)
    usage();
strexCalc(argv[1]);
return 0;
}
