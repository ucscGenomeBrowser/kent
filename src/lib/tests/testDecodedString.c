/* testQuotedString - test the parseQuotedString function */

/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "obscure.h"
#include "dystring.h"
#include "htmshell.h"

void usage()
/* Explain usage and exit */
{
errAbort(
"testDecodedString - test the htmshell decode functions\n"
"usage:\n"
"   testDecodedString type <some quote char>[any characters]<some quote char>\n"
"   whatever is given on the command line: [any characters] will be parsed\n"
"   the first character encountered will be the quote character\n"
"   the string must end with that quote character\n"
"   beware of your shell consuming your quote characters if you want\n"
"   to use \" or \'\n"
"   type can be any of these: attr css js url\n"
"   -verbose=2 to mirror [any characters]\n"
"  HH stands for 2 hex digits 0-9 A-F a-f\n"
"attr \"&#xHH;\"\n"
"css \"\\HH \" (trailing space critical)\n" 
"js \"\\xHH\"\n"
"url \"%%HH\"\n"
);
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
int i;
char *type = argv[1];
struct dyString *cmdArgs = newDyString(0);
for (i = 2; i < argc; ++i)
    {
    if (i > 2)
	dyStringPrintf(cmdArgs, " %s", argv[i]);
    else
	dyStringPrintf(cmdArgs, "%s", argv[i]);
    }

char *dupe = cloneString(cmdArgs->string);
verbose(2, "command line: '%s'\n", cmdArgs->string);
if (sameString(type,"attr"))
    {
    verbose(1,"============  attributeDecode\n");
    attributeDecode(dupe);
    verbose(1, "attr decoded to: '%s'\n", dupe);
    }
else if (sameString(type,"css"))
    {
    verbose(1,"============  cssDecode\n");
    cssDecode(dupe);
    verbose(1, "css decoded to: '%s'\n", dupe);
    }
else if (sameString(type,"js"))
    {
    verbose(1,"============  jsDecode\n");
    jsDecode(dupe);
    verbose(1, "js decoded to: '%s'\n", dupe);
    }
else if (sameString(type,"url"))
    {
    verbose(1,"============  urlDecode\n");
    urlDecode(dupe);
    verbose(1, "url decoded to: '%s'\n", dupe);
    }
freeDyString(&cmdArgs);
return 0;
}
