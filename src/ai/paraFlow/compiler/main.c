/* Command line main driver module for paraFlow compiler. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "localmem.h"
#include "tokenizer.h"

void usage()
/* Explain command line and exit. */
{
errAbort(
"This program compiles paraFlow code.  ParaFlow is a parallel programming\n"
"language designed by Jim Kent.\n"
"Usage:\n"
"    paraFlow input.pf\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

enum pfTokType
    {
    pftName = 256,
    pftString,
    pftNumber,
    };

char *pfTokTypeAsString(enum pfTokType type)
/* Return string corresponding to pfTokType */
{
static char buf[2];
switch (type)
    {
    case pftName: return "pftName";
    case pftString: return "pftString";
    case pftNumber: return "pftNumber";
    default:
        buf[0] = type;
	return buf;
    }
}

struct pfSource
/* A paraFlow source file. */
    {
    struct pfSource *next;	/* Next in list. */
    char *name;			/* File name. */
    struct tokenizer *tkz;	/* Tokenizer on this source. */
    };

struct pfTokenizer
/* Tokenizing structure. */
    {
    struct pfTokenizer *next;	/* Next if any in list */
    struct lm *lm;		/* Local memory pool for tokens etc. */
    struct pfSource *source;	/* Current source file. */
    struct pfSource *sourceList; /* List of all source files */
    struct hash *symbols;	/* Hash containing all symbols. */
    struct hash *strings;	/* Hash containing all strings. */
    };

union pfTokVal
/* The value field of a token. */
    {
    char *s;		/* A string (allocated in string table) 
                         * This covers strings, names, and glyphs. */
    long long i;	/* An integer type */
    double x;		/* Floating point */
    char c;		/* A single character symbol. */
    };

struct pfToken
/* ParaFlow lexical token. */
    {
    struct pfToken *next;	/* Next token in list. */
    enum pfTokType type;	/* Token type. */
    union pfTokVal val;		/* Type-dependent value. */
    struct pfSource *source;	/* Source file this is in. */
    int startLine,startCol;	/* Position of token start within source. */
    int endLine, endCol;	/* End of token within source. */
    };

struct pfSource *pfSourceNew(char *fileName)
/* Create new pfSource based around file */
{
struct pfSource *source;
AllocVar(source);
source->tkz = tokenizerNew(fileName);
source->tkz->leaveQuotes = TRUE;
source->tkz->uncommentC = TRUE;
source->tkz->uncommentShell = TRUE;
source->name = cloneString(fileName);
return source;
}

/* A paraFlow source file. */
struct pfTokenizer *pfTokenizerNew(char *fileName)
/* Create tokenizing structure on file. */
{
struct pfTokenizer *pfTkz;
AllocVar(pfTkz);
pfTkz->lm = lmInit(0);
pfTkz->source = pfSourceNew(fileName);
pfTkz->symbols = hashNew(16);
pfTkz->strings = hashNew(16);
return pfTkz;
}

struct pfToken *pfTokenizerNext(struct pfTokenizer *pfTkz)
/* Puts next token in token.  Returns token type. For single
 * character tokens, token type is just the char value,
 * other token types start at 256. */
{
struct pfToken *tok;
char *s, c;
struct tokenizer *tkz = pfTkz->source->tkz;
s = tokenizerNext(tkz);
if (s == NULL)
    return NULL;
lmAllocVar(pfTkz->lm, tok);
tok->source = pfTkz->source;
tok->startLine = tkz->lf->lineIx;
tok->startCol = tkz->linePt - tkz->curLine - strlen(s);
c = s[0];
if (c == '"' || c == '\'')
    {
    int len = strlen(s);
    s[len-1] = 0;
    tok->type = pftString;
    tok->val.s = hashStoreName(pfTkz->strings, s+1);
    }
else if (isdigit(c))
    {
    tok->type = pftNumber;
    tok->val.i = atoll(s);
    }
else if (c == '_' || isalpha(c))
    {
    tok->type = pftName;
    tok->val.s = hashStoreName(pfTkz->symbols, s);
    }
else
    {
    tok->type = c;
    tok->val.c = c;
    }
return tok;
}

void paraFlowOnFile(char *fileName)
/* Execute paraFlow on single file. */
{
struct pfTokenizer *pfTkz = pfTokenizerNew(fileName);
struct pfToken *tokList = NULL, *tok;
while ((tok = pfTokenizerNext(pfTkz)) != NULL)
    {
    slAddHead(&tokList, tok);
    }
slReverse(&tokList);

for (tok = tokList; tok != NULL; tok = tok->next)
    {
    printf("file %s, line %3d, column %2d, type %3d, value ",
    	tok->source->name, tok->startLine, tok->startCol, tok->type);
    switch (tok->type)
        {
	case pftName:
	    printf("%s", tok->val.s);
	    break;
	case pftString:
	    printf("\"%s\"", tok->val.s);
	    break;
	case pftNumber:
	    printf("%lld", tok->val.i);
	    break;
	default:
	    printf("%c", tok->val.c);
	    break;
        }
    printf("\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
paraFlowOnFile(argv[1]);
return 0;
}

