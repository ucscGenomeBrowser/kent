/* paraToken - reads a line file and produces paraTokens from
 * it. */

#ifndef PARATOKEN_H
#define PARATOKEN_H

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef LOCALMEM_H
#include "localmem.h"
#endif

enum pfTokType
    {
    pftWhitespace = 256,
    pftComment,
    pftName,
    pftString,
    pftInt,
    pftFloat,
    pftPlusPlus,
    pftMinusMinus,
    pftPlusEquals,
    pftMinusEquals,
    pftDivEquals,
    pftMulEquals,
    pftModEquals,
    pftEqualsEquals,
    pftEof,
    ptfEnd,
    };

struct pfSource
/* A paraFlow source file. */
    {
    struct pfSource *next;	/* Next in list. */
    char *name;			/* File name. */
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
    struct lineFile *lf;	/* Open linefile on current source. */
    char *line;			/* Points to start of current line. */
    int lineSize;		/* Size of current line including '\n'. */
    int posInLine;		/* Position within current line. */
    struct dyString *dy;	/* Dynamic string buffer - for symbols and
                                 * strings before they go in hash. */
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
    int startLine,startCol;	/* Start of token within source. One based. */
    };

struct pfTokenizer *pfTokenizerNew(char *fileName);
/* Create tokenizing structure on file. */

struct pfToken *pfTokenizerNext(struct pfTokenizer *tkz);
/* Puts next token in token.  Returns token type. For single
 * character tokens, token type is just the char value,
 * other token types start at 256. */

void pfTokenDump(struct pfToken *tok, FILE *f);
/* Print info on token to file. */

#endif /* PARATOKEN_H */
