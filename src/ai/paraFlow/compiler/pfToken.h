/* pfToken - turns files into lexical tokens. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

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
    pftString,		/* Single quoted string */
    pftSubstitute,	/* Double quoted string */
    pftInt,
    pftLong,
    pftFloat,
    pftDotDotDot,	/* ... */
    pftPlusPlus,        /* ++ */
    pftMinusMinus,      /* -- */
    pftPlusEquals,      /* += */
    pftMinusEquals,     /* -= */
    pftDivEquals,       /* /= */
    pftMulEquals,       /* *= */
    pftEqualsEquals,    /* == */
    pftNotEquals,       /* != */
    pftGreaterOrEquals, /* >= */
    pftLessOrEquals,    /* <= */
    pftLogAnd,          /* && */
    pftLogOr,           /* || */
    pftShiftLeft,       /* << */
    pftShiftRight,      /* >> */

    /* - reserved words - */
    pftReservedWordStart,
    pftBreak,
    pftCase,
    pftCatch,
    pftContinue,
    pftConst,
    pftClass,
    pftElse,
    pftExtends,
    pftFlow,
    pftFor,
    pftGlobal,
    pftIf,
    pftImport,
    pftIn,
    pftInclude,
    pftInterface,
    pftInto,
    pftLocal,
    pftNew,
    pftNil,
    pftOf,
    pftOperator,
    pftPara,
    pftPolymorphic,
    pftPrivate,
    pftProtected,
    pftReadable,
    pftReturn,
    pftStatic,
    pftTil,
    pftTo,
    pftTry,
    pftWhile,
    pftWritable,
    };

char *pfTokTypeAsString(enum pfTokType type);
/* Return string corresponding to pfTokType */

struct pfSource
/* A paraFlow source file. */
    {
    struct pfSource *next;	/* Next in list. */
    char *name;			/* File name. */
    char *contents;		/* File contents as one long string */
    size_t contentSize;		/* Size of contents. */
    };

struct pfSource *pfSourceNew(char *name, char *contents);
/* Create new pfSource. */

struct pfSource *pfSourceOnFile(char *fileName);
/* Create new pfSource based around file */

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
    int tokenCount;		/* Count of calls to tokenNext. */
    };

union pfTokVal
/* The value field of a token. */
    {
    char *s;		/* A string (allocated in string table) 
                         * This covers strings, names, and glyphs. */
    long long l;	/* A 64-bit integer type */
    int i;		/* 32-bit integer type. */
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


struct pfTokenizer *pfTokenizerNew(struct hash *reservedWords);
/* Create tokenizer with given reserved words.  You'll need to
 * do a pfTokenizerNewSource() before using it much. */

void pfTokenizerNewSource(struct pfTokenizer *tkz, struct pfSource *source);
/*  Attatch new source to tokenizer. */

struct pfToken *pfTokenizerNext(struct pfTokenizer *tkz);
/* Puts next token in token.  Returns token type. For single
 * character tokens, token type is just the char value,
 * other token types start at 256. */

void pfTokenDump(struct pfToken *tok, FILE *f, boolean doLong);
/* Print info on token to file. */

void pfSourcePos(struct pfSource *source, char *pos, 
    char **retFileName, int *retLine, int *retCol);

/* Return file name, line, and column (zero based) of pos. */
void errAt(struct pfToken *tok, char *format, ...);
/* Warn about error at given token. */

void expectingGot(char *expecting, struct pfToken *got);
/* Complain about unexpected stuff and quit. */

char *pfTokenAsString(struct pfToken *tok);
/* Return string representation of token.  FreeMem the string
 * when done. */

struct pfToken *pfTokenGetClosingTok(struct pfToken *openTok,
	enum pfTokType openType, enum pfTokType closeType);
/* Given a token that represents an open glyph such as (, [ or {,
 * find token that represents the closing glyph ) ] or } .
 * This does handle nesting. */

struct pfToken *pfTokenFirstBetween(struct pfToken *start,
	struct pfToken *end, enum pfTokType type);
/* Return first token of given type between start and end.
 * End is not inclusive. */

#define internalErrAt(tok)  errAt(tok, "paraFlow compiler error at %s %d", __FILE__, __LINE__)
/* Generic internal error message */

#endif /* PFTOKEN_H */
