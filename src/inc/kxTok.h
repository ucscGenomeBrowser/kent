/* kxTok - quick little tokenizer for stuff first
 * loaded into memory.  Originally developed for
 * "Key eXpression" evaluator. */

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
    };

struct kxTok
/* A key expression token.   Input text is tokenized
 * into a list of these. */
    {
    struct kxTok *next;
    enum kxTokType type;
    char string[1];  /* Allocated at run time */
    };

struct kxTok *kxTokenize(char *text);
/* Convert text to stream of tokens. */

#endif /* KXTOK_K */
