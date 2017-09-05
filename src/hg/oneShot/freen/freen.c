/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "jsonWrite.h"
#include "tagSchema.h"
#include "csv.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}


static struct slName *makeObjArrayPieces(char *name)
/* Given something like this.[].that return a list of "this." ".that".  That is
 * return a list of all strings before between and after the []'s Other
 * examples:
 *        [] returns "" ""
 *        this.[] return "this." ""
 *        [].that returns "" ".that"
 *        this.[].that.and.[].more returns "this." ".that.and." ".more" */
{
struct slName *list = NULL;	// Result list goes here
char *pos = name;

/* Handle special case of leading "[]" */
if (startsWith("[]", name))
     {
     slNameAddHead(&list, "");
     pos += 2;
     }

char *aStart;
for (;;)
    {
    aStart = strchr(pos, '[');
    if (aStart == NULL)
        {
	slNameAddHead(&list, pos);
	break;
	}
    else
        {
	struct slName *el = slNameNewN(pos, aStart-pos);
	slAddHead(&list, el);
	pos = aStart + 2;
	}
    }
slReverse(&list);
return list;
}

void freen(char *input)
/* Test something */
{
struct slName *el, *list = makeObjArrayPieces(input);
printf("%s\n", input);
for (el = list; el != NULL; el = el->next)
    printf("\t'%s'\n", el->name);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

