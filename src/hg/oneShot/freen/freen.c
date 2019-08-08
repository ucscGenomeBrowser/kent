/* freen - My Pet Freen.  A pet freen is actually more dangerous than a wild one. */

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
#include "localmem.h"
#include "csv.h"
#include "tokenizer.h"
#include "strex.h"
#include "hmac.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

/*  strex -String Expression - stuff to implement a relatively simple string
 *  processing expression parser. */

struct hash *hashFromFile(char *fileName)
/* Given a two column file (key, value) return a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(16);
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *word = nextWord(&line);
    line = trimSpaces(line);
    hashAdd(hash, word, lmCloneString(hash->lm, line));
    }
lineFileClose(&lf);
return hash;
}

static char *symLookup(void *record, char *key)
/* Lookup symbol in hash */
{
struct hash *hash = record;
return hashFindVal(hash, key);
}


void freen(char *symbols, char *expression)
/* Test something */
{
struct hash *symHash = hashFromFile(symbols);
verbose(1, "Got %d symbols from %s\n", symHash->elCount, symbols);
uglyf("Parsing...\n");
struct strexParse *parseTree = strexParseString(expression);
strexParseDump(parseTree, 0, uglyOut);
uglyf("Evaluating...\n");
uglyf("%s\n", strexEvalAsString(parseTree, symHash, symLookup));
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

