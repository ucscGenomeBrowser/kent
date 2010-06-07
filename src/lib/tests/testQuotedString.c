/* testQuotedString - test the parseQuotedString function */
#include "common.h"
#include "options.h"
#include "obscure.h"
#include "dystring.h"

void usage()
/* Explain usage and exit */
{
errAbort(
"testQuotedString - test the parseQuotedString() function\n"
"usage:\n"
"   testQuotedString <some quote char>[any characters]<some quote char>\n"
"   whatever is given on the command line: [any characters] will be parsed\n"
"   the first character encountered will be the quote character\n"
"   the string must end with that quote character\n"
"   beware of your shell consuming your quote characters if you want\n"
"   to use \" or \'\n"
"   -verbose=2 to mirror [any characters]\n"
);
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
int i;
struct dyString *cmdArgs = newDyString(0);
for (i = 1; i < argc; ++i)
    {
    if (i > 1)
	dyStringPrintf(cmdArgs, " %s", argv[i]);
    else
	dyStringPrintf(cmdArgs, "%s", argv[i]);
    }

char *dupe = cloneString(cmdArgs->string);
verbose(2, "command line: '%s'\n", cmdArgs->string);
verbose(1,"============  parseQuotedStringNoEscapes\n");
if (!parseQuotedStringNoEscapes(cmdArgs->string, dupe, NULL))
    verbose(1, "can not parse: '%s'\n", cmdArgs->string);
else
    verbose(1, "parsed to: '%s'\n", dupe);
verbose(1,"============  parseQuotedString\n");
if (!parseQuotedString(cmdArgs->string, dupe, NULL))
    errAbort("can not parse: '%s'", cmdArgs->string);
else
    verbose(1, "parsed to: '%s'\n", dupe);
freeDyString(&cmdArgs);
return 0;
}
