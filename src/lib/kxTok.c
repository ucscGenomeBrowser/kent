/* kxTok - quick little tokenizer for stuff first
 * loaded into memory.  Originally developed for
 * "Key eXpression" evaluator. */

#include "common.h"
#include "kxTok.h"

static struct kxTok *kxTokNew(enum kxTokType type, char *string, int stringSize)
/* Allocate and initialize a new token. */
{
struct kxTok *tok;
int totalSize = stringSize + sizeof(*tok);
tok = needMem(totalSize);
tok->type = type;
memcpy(tok->string, string, stringSize);
return tok;
}

struct kxTok *kxTokenize(char *text, boolean wildAst)
/* Convert text to stream of tokens. If 'wildAst' is
 * TRUE then '*' character will be treated as wildcard
 * rather than multiplication sign. */
{
struct kxTok *tokList = NULL, *tok;
char c, *s, *start = NULL, *end = NULL;
enum kxTokType type = 0;

s = text;
for (;;)
    {
    if ((c = *s) == 0)
        break;
    start = s++;
    if (isspace(c))
        {
        continue;
        }
    else if (isalnum(c) || c == '?' || (wildAst && c == '*'))
        {
        if (c == '?')
            type = kxtWildString;
	else if (wildAst && c == '*')
            type = kxtWildString;
        else
            type = kxtString;
        for (;;)
            {
            c = *s;
            if (isalnum(c))
                ++s;
            else if (c == '?' || (wildAst && c == '*'))
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
        start = s;
        for (;;)
            {
            c = *s++;
            if (c == '"')
                break;
            if (c == '*' || c == '?')
                type = kxtWildString;
            }
        end = s-1;
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
    else
        {
        errAbort("Unrecognized character %c", c);
        }
    tok = kxTokNew(type, start, end-start);
    slAddHead(&tokList, tok);
    }
tok = kxTokNew(kxtEnd, "end", 3);
slAddHead(&tokList, tok);
slReverse(&tokList);
return tokList;
}


