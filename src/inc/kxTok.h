/* kxTok - quick little tokenizer for stuff first
 * loaded into memory.  Originally developed for
 * "Key eXpression" evaluator. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef KXTOK_H
#define KXTOK_H

enum kxTokType
    {
    kxtEnd,
    kxtString,
    kxtWildString,
    kxtEquals,
    kxtGT,      /* Greater Than */
    kxtGE,      /* Greater Than or Equal */
    kxtLT,      /* Less Than */
    kxtLE,      /* Less Than or Equal */
    kxtAnd,
    kxtOr,
    kxtXor,
    kxtNot,
    kxtOpenParen,
    kxtCloseParen,
    kxtAdd,
    kxtSub,
    kxtDiv,
    kxtMul,
    kxtDot,
    kxtPunct,
    };

struct kxTok
/* A key expression token.   Input text is tokenized
 * into a list of these. */
    {
    struct kxTok *next;
    enum kxTokType type;
    char string[1];  /* Allocated at run time */
    };

struct kxTok *kxTokenize(char *text, boolean wildAst);
/* Convert text to stream of tokens. If 'wildAst' is
 * TRUE then '*' character will be treated as wildcard
 * rather than multiplication sign. */

void kxTokIncludeQuotes(boolean val);
/* Pass in TRUE if kxTok should include quote characters in string tokens. */

#endif /* KXTOK_K */
