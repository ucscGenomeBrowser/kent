/* callGraph - Make C function call graph. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tokenizer.h"
#include "localmem.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "callGraph - Make C function call graph\n"
  "usage:\n"
  "   callGraph file.c\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct token
/* A token - includes string, line No, and file */
    {
    struct token *next;	/* Next in list. */
    char *string;	/* Token's string (not allocated here). */
    int lineIx;		/* Line number. */
    char *fileName;	/* File name (not allocated here) */
    };

struct token *readAllTokens(char *fileName, struct lm *lm)
/* Get list of all tokens from file. */
{
struct tokenizer *tkz;
struct token *tokList = NULL, *tok;
char *s;

tkz = tokenizerNew(fileName);
tkz->uncommentC = tkz->uncommentShell = tkz->leaveQuotes = TRUE;
while ((s = tokenizerNext(tkz)) != NULL)
    {
    lmAllocVar(lm, tok);
    tok->string = lmCloneString(lm, s);
    tok->fileName = fileName;
    tok->lineIx = tkz->lf->lineIx;
    slAddHead(&tokList, tok);
    }
slReverse(&tokList);
return tokList;
}

struct token *token;

boolean isReservedWord(char *word)
/* Return TRUE if a reserved word. */
{
static struct hash *hash;
if (hash == NULL)
    {
    hash = hashNew(8);
    hashAdd(hash, "for", NULL);
    hashAdd(hash, "if", NULL);
    hashAdd(hash, "while", NULL);
    hashAdd(hash, "return", NULL);
    }
return hashLookup(hash, word) != NULL;
}

void nextToken()
/* Advance token, complain if at end. */
{
token = token->next;
if (token == NULL)
    errAbort("Unexpected end of file");
}

void parseFunction(struct token *functionName)
/* Starting with ( parse function and make call graph. */
{
char c;
struct hash *uniqHash = hashNew(0);
for (;;)
    {
    nextToken();
    if (token->string[0] == ')')
	 {
	 nextToken();
	 break;
	 }
    }
c = token->string[0];
if (c == '{')
    {
    int blockDepth = 1;
    struct token *lastName = NULL;
    for (;;)
	{
	char c;
	nextToken();
	c = token->string[0];
	if (c == '{')
	    {
	    lastName = NULL;
	    ++blockDepth;
	    }
	else if (c == '}')
	    {
	    lastName = NULL;
	    --blockDepth;
	    if (blockDepth == 0)
		break;
	    }
	else if (c == '_' || isalpha(c))
	    lastName = token;
	else if (c == '(')
	    {
	    if (lastName != NULL && !isReservedWord(lastName->string))
		{
		// if (!hashLookup(uniqHash, lastName->string))
		    {
		  //   hashAdd(uniqHash, lastName->string, NULL);
		    printf("%s -> %s\n", functionName->string, lastName->string);
		    }
		}
	    lastName = NULL;
	    }
	else
	    lastName = NULL;
	}
    }
hashFree(&uniqHash);
}

void parseFunctions()
/* Parse list of functions - skipping other declarations. */
{
struct token *lastName = NULL;
while (token != NULL)
    {
    char c = token->string[0];
    if (isalpha(c) || c == '_')
	{
	lastName = token;
	}
    else if (c == '(')
	{
	if (lastName != NULL)
	    parseFunction(lastName);
	lastName = NULL;
	}
    else
	{
	lastName = NULL;
	}
    token = token->next;
    }
}

void callGraph(char *fileName)
/* callGraph - Make C function call graph. */
{
struct lm *lm = lmInit(0);
struct token *tokenList;
tokenList = token = readAllTokens(fileName, lm);
printf("digraph g {\n");
parseFunctions();
printf("}\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
callGraph(argv[1]);
return 0;
}
