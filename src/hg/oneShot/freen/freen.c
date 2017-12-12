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

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static int nonDotSize(char *s)
/* Return number of chars up to next dot or end of string */
{
char c;
int size = 0;
for (;;)
    {
    c = *s++;
    if (c == '.' || c == 0)
       return size;
    ++size;
    }
}

int tagSchemaParseIndexes(char *input, int indexes[], int maxIndexCount)
/* This will parse out something that looks like:
 *     this.array.2.subfield.subarray.3.name
 * into
 *     2,3   
 * Where input is what we parse,   indexes is an array maxIndexCount long
 * where we put the numbers. */
{
char dot = '.';
char *s = input;
int indexCount = 0;
int maxMinusOne = maxIndexCount - 1;
for (;;)
    {
    /* Check for end of string */
    char firstChar = *s;
    if (firstChar == 0)
        break;

    /* If leading char is a dot and if so skip it. */
    boolean startsWithDot = (firstChar == dot);
    if (startsWithDot)
       ++s;

    int numSize = tagSchemaDigitsUpToDot(s);
    if (numSize > 0)
        {
	if (indexCount > maxMinusOne)
	    errAbort("Too many array indexes in %s, maxIndexCount is %d",
		input, maxIndexCount);
	indexes[indexCount] = atoi(s);
	indexCount += 1;
	s += numSize;
	}
    else
        {
	int partSize = nonDotSize(s);
	s += partSize;
	}
    }
return indexCount;
}

char *tagSchemaInsertIndexes(char *bracketed, int indexes[], int indexCount,
    struct dyString *scratch)
/* Convert something that looks like:
 *     this.array.[].subfield.subarray.[].name and 2,3
 * into
 *     this.array.2.subfield.subarray.3.name
 * The scratch string holds the return value. */
{
char *pos = bracketed;
int indexPos = 0;
dyStringClear(scratch);

/* Handle special case of leading "[]" */
if (startsWith("[]", pos))
     {
     dyStringPrintf(scratch, "%d", indexes[indexPos]);
     ++indexPos;
     pos += 2;
     }

char *aStart;
for (;;)
    {
    aStart = strchr(pos, '[');
    if (aStart == NULL)
        {
	dyStringAppend(scratch, pos);
	break;
	}
    else
        {
	if (indexPos >= indexCount)
	    errAbort("Expecting %d '[]' in %s, got more.", indexCount, bracketed);
	dyStringAppendN(scratch, pos, aStart-pos);
	dyStringPrintf(scratch, "%d", indexes[indexPos]);
	++indexPos;
	pos = aStart + 2;
	}
    }
return scratch->string;
}

void freen(char *input)
/* Test something */
{
struct dyString *withBrackets = dyStringNew(0);
tagSchemaFigureArrayName(input, withBrackets);
int maxIndexCount = 10;
int indexes[maxIndexCount];
int indexCount = tagSchemaParseIndexes(input, indexes, maxIndexCount);
{
uglyf("Got index count of %d, values:", indexCount);
int i;
for (i=0; i<indexCount; ++i)
    uglyf(" %d", indexes[i]);
uglyf("\n");
}

if (indexCount > 0)
    {
    int i;
    for (i=0; i<indexCount; ++i)
	indexes[i] += 1;
    struct dyString *withIncs = dyStringNew(0);
    char *incString = tagSchemaInsertIndexes(withBrackets->string, 
	indexes, indexCount, withIncs);
    printf("%s -> %s\n", input, incString);
    }
else
    printf("%s has no array index subfields\n", input);
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

