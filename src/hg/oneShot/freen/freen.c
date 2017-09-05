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

static boolean matchAndExtractIndexes(char *dottedName, struct slName *objArrayPieces,
    struct dyString *scratch, char **retIndexes, boolean *retMatchEnd)
/* Dotted name is something like this.that.12.more.2.ok
 * The objArrayPieces is something like "this.that." ".more." ".notOk"
 * Crucially it holds at least two elements, some of which may be ""
 * This function will return TRUE if all but maybe the last of the objArrayPieces is
 * found in the dottedName.  If this is the case it will put something like .12.2 in
 * scratch and *retIndexes.  If the last one matches it will set *retMatchEnd.
 * Sort of a complex routine but it plays a key piece in checking required array
 * elements */
{
dyStringClear(scratch);
struct slName *piece = objArrayPieces;
char *pos = dottedName;
for (;;)
    {
    /* Check to see if we match next piece, and return FALSE if not */
    char *pieceString = piece->name;
    int pieceLen = strlen(pieceString);
    if (pieceLen != 0 && memcmp(pieceString, pos, pieceLen) != 0)
        return FALSE;
    pos += pieceLen;

    /* Put the number into scratch with a leading dot separator. */
    int digits = tagSchemaDigitsUpToDot(pos);
    assert(digits >= 0);
    dyStringAppendC(scratch, '.');
    dyStringAppendN(scratch, pos, digits);
    pos += digits;

    /* Go to next piece,saving last piece for outside of the loop. */
    piece = piece->next;
    if (piece->next == NULL)
        break;
    }

/* One more special case, where last piece needs to agree on emptiness at least in
 * terms of matching */
if (isEmpty(piece->name) != isEmpty(pos))
    return FALSE;

/* Otherwise both have something.  We return true/false depending on whether it matches */
*retMatchEnd = (strcmp(piece->name, pos) == 0);
*retIndexes = scratch->string;
return TRUE;
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

void checkIt(char *bracketed, char *numbered)
/* Test something */
{
struct dyString *scratch = dyStringNew(0);

printf("%s    %s :\n", bracketed, numbered);
struct slName *el, *list = makeObjArrayPieces(bracketed);
for (el = list; el != NULL; el = el->next)
    printf("\t'%s'\n", el->name);
char *index;
boolean fullMatch = FALSE;
if (matchAndExtractIndexes(numbered, list, scratch, &index, &fullMatch))
    {
    printf("index %s, fullMatch %d\n", index, fullMatch);
    }
else
    printf("No match\n");
printf("--------------\n");
}

void freen(char *input)
/* Test something */
{
checkIt("this.[].that.[].more.so", "this.1.that.2.more.so");
checkIt("this.[].that.[].more.so", "this.3.that.1.more.notSo");
checkIt("this.[].that.[].more.so", "other.3.that.1.more.notSo");
checkIt("[].name", "3.name");
checkIt("[].name", "3.fancy");
checkIt("name.[]", "3");
checkIt("[].name", "3");
// checkIt("[]", "abba");
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

