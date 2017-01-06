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


void freen(char *tagFile, char *fieldList, char *whereClause)
/* Test something */
{
struct tagStorm *tagStorm = tagStormFromFile(tagFile);
uglyf("Got %d in %s\n", slCount(tagStorm->forest), tagFile);
struct tagStanza *result, *resultList = tagStormQuery(tagStorm, fieldList, whereClause);
for (result = resultList; result != NULL; result = result->next)
    {
    struct slPair *tag;
    for (tag = result->tagList; tag != NULL; tag = tag->next)
	printf("%s %s\n", tag->name, (char *)tag->val);
    printf("\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
freen(argv[1], argv[2], argv[3]);
return 0;
}

