/* paraToken - reads a line file and produces paraTokens from
 * it. */

#ifndef PFTOKEN_H
#define PFTOKEN_H

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef LOCALMEM_H
#include "localmem.h"
#endif

enum pfTokType
    {
    pftEnd = 256,
    pftWhitespace,
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

    /* - reserved words - */
    pftClass,
    pftTo,
    pftPara,
    pftFlow,
    pftInto,
    pftFor,
    pftForeach,
    pftWhile,
    pftOf,
    pftIf,
    pftElse,
    };

struct pfSource
/* A paraFlow source file. */
    {
    struct pfSource *next;	/* Next in list. */
    char *name;			/* File name. */
    char *contents;		/* File contents as one long string */
    size_t contentSize;		/* Size of contents. */
    };

struct pfTokenizer
/* Tokenizing structure. */
    {
    struct pfTokenizer *next;	/* Next if any in list */
    struct lm *lm;		/* Local memory pool for tokens etc. */
    struct pfSource *source;	/* Current source file. */
    char *pos;			/* Current position within source->contents. */
    char *endPos;		/* Last position within source->contents. */
    struct hash *reserved;	/* Hash of built-in reserved words. */
    struct hash *symbols;	/* Hash containing all symbols. */
    struct hash *strings;	/* Hash containing all strings. */
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
    struct pfScope *scope;	/* Scope start for {'s  */
    };

struct pfToken
/* ParaFlow lexical token. */
    {
    struct pfToken *next;	/* Next token in list. */
    enum pfTokType type;	/* Token type. */
    union pfTokVal val;		/* Type-dependent value. */
    struct pfSource *source;	/* Source file this is in. */
    char *text;			/* Pointer to start of text in source file. */
    int textSize;		/* Size of text. */
    };


struct pfTokenizer *pfTokenizerNew(char *fileName, struct hash *reservedWords);
/* Create tokenizing structure on file.  Reserved words is an int valued hash
 * that may be NULL. */

struct pfToken *pfTokenizerNext(struct pfTokenizer *tkz);
/* Puts next token in token.  Returns token type. For single
 * character tokens, token type is just the char value,
 * other token types start at 256. */

void pfTokenDump(struct pfToken *tok, FILE *f, boolean doLong);
/* Print info on token to file. */

void errAt(struct pfToken *tok, char *format, ...);
/* Warn about error at given token. */

void expectingGot(char *expecting, struct pfToken *got);
/* Complain about unexpected stuff and quit. */

#endif /* PFTOKEN_H */
