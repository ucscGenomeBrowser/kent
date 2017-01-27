/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"
#include "tagStorm.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void rFix(struct tagStanza *list)
/* Fix up stanza list recursively */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	if (sameString(pair->name, "LibraryID"))
	    {
	    char *val = pair->val;
	    subChar(val, '-', '.');
	    }
	}
    rFix(stanza->children);
    }
}

void freen(char *inFile, char *outFile)
/* Test something */
{
struct tagStorm *tagStorm = tagStormFromFile(inFile);
rFix(tagStorm->forest);
tagStormWrite(tagStorm, outFile, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
freen(argv[1], argv[2]);
return 0;
}

