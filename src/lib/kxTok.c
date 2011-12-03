/* kxTok - quick little tokenizer for stuff first
 * loaded into memory.  Originally developed for
 * "Key eXpression" evaluator. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "kxTok.h"


boolean includeQuotes = FALSE;

static struct kxTok *kxTokNew(enum kxTokType type, char *string, int stringSize,
	boolean spaceBefore)
/* Allocate and initialize a new token. */
{
struct kxTok *tok;
int totalSize = stringSize + sizeof(*tok);
tok = needMem(totalSize);
tok->type = type;
tok->spaceBefore = spaceBefore;
memcpy(tok->string, string, stringSize);
return tok;
}

struct kxTok *kxTokenizeFancy(char *text, boolean wildAst,
			      boolean wildPercent, boolean includeHyphen)
/* Convert text to stream of tokens. If 'wildAst' is
 * TRUE then '*' character will be treated as wildcard
 * rather than multiplication sign.  
 * If wildPercent is TRUE then the '%' character will be treated as a 
 * wildcard (as in SQL) rather than a modulo (kxtMod) or percent sign.
 * If includeHyphen is TRUE then a '-' character in the middle of a String 
 * token will be treated as a hyphen (part of the String token) instead of 
 * a new kxtSub token. */
{
struct kxTok *tokList = NULL, *tok;
char c, *s, *start = NULL, *end = NULL;
enum kxTokType type = 0;
boolean spaceBefore = FALSE;

s = text;
for (;;)
    {
    if ((c = *s) == 0)
        break;
    start = s++;
    if (isspace(c))
        {
	spaceBefore = TRUE;
        continue;
        }
    else if (isalnum(c) || c == '?' || (wildAst && c == '*') ||
	     (wildPercent && c == '%'))
        {
        if (c == '?')
            type = kxtWildString;
	else if (wildAst && c == '*')
            type = kxtWildString;
	else if (wildPercent && c == '%')
            type = kxtWildString;
        else
            type = kxtString;
        for (;;)
            {
            c = *s;
            if (isalnum(c) || c == ':' || c == '_' || c == '.' ||
		(includeHyphen && c == '-'))
                ++s;
            else if (c == '?' || (wildAst && c == '*') ||
		     (wildPercent && c == '%'))
                {
                type = kxtWildString;
                ++s;
                }
            else
                break;
            }
        end = s;
        }
    else if (c == '"')
        {
        type = kxtString;
        if (! includeQuotes)
	    start = s;
        for (;;)
            {
            c = *s++;
            if (c == '"')
                break;
            if (c == '*' || c == '?' || (wildPercent && c == '%'))
                type = kxtWildString;
            }
	if (! includeQuotes)
	    end = s-1;
	else
	    end = s;
        }
    else if (c == '\'')
        {
        type = kxtString;
        if (! includeQuotes)
	    start = s;
        for (;;)
            {
            c = *s++;
            if (c == '\'')
                break;
            if (c == '*' || c == '?' || (wildPercent && c == '%'))
                type = kxtWildString;
            }
	if (! includeQuotes)
	    end = s-1;
	else
	    end = s;
        } 
    else if (c == '=')
        {
        type = kxtEquals;
        end = s;
        }
    else if (c == '&')
        {
        type = kxtAnd;
        end = s;
        }
    else if (c == '|')
        {
        type = kxtOr;
        end = s;
        }
    else if (c == '^')
        {
        type = kxtXor;
        end = s;
        }
    else if (c == '+')
        {
	type = kxtAdd;
	end = s;
	}
    else if (c == '-')
        {
	type = kxtSub;
	end = s;
	}
    else if (c == '*')
        {
	type = kxtMul;
	end = s;
	}
    else if (c == '/')
        {
	type = kxtDiv;
	end = s;
	}
    else if (c == '(')
        {
        type = kxtOpenParen;
        end = s;
        }
    else if (c == ')')
        {
        type = kxtCloseParen;
        end = s;
        }
    else if (c == '!')
        {
        type = kxtNot;
        end = s;
        }
    else if (c == '>')
        {
        if (*s == '=')
            {
            ++s;
            type = kxtGE;
            }
        else
            type = kxtGT;
        end = s;
        }
    else if (c == '<')
        {
        if (*s == '=')
            {
            ++s;
            type = kxtLE;
            }
        else
            type = kxtLT;
        end = s;
        }
    else if (c == '.')
        {
        type = kxtDot;
        end = s;
	}
    else if (c == '%')
        {
        type = kxtMod;
        end = s;
	}
    else if (ispunct(c))
        {
        type = kxtPunct;
        end = s;
	}
    else
        {
        errAbort("Unrecognized character %c", c);
        }
    tok = kxTokNew(type, start, end-start, spaceBefore);
    slAddHead(&tokList, tok);
    spaceBefore = FALSE;
    }
tok = kxTokNew(kxtEnd, "end", 3, spaceBefore);
slAddHead(&tokList, tok);
slReverse(&tokList);
return tokList;
}


struct kxTok *kxTokenize(char *text, boolean wildAst)
/* Convert text to stream of tokens. If 'wildAst' is
 * TRUE then '*' character will be treated as wildcard
 * rather than multiplication sign. */
{
return kxTokenizeFancy(text, wildAst, FALSE, FALSE);
}

void kxTokIncludeQuotes(boolean val)
/* Pass in TRUE if kxTok should include quote characters in string tokens. */
{
includeQuotes = val;
}


