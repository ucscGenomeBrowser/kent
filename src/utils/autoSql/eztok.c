/* Currently unused... */

/* eztok - simple but often effective tokenizer.  Separates
 * things into space or punctuation delimited words.  Punctuation
 * (anything non-alphanumeric) is returned as a single character
 * token.  It will handle quotes, but not allow escapes within
 * quotes. */

#include "common.h"
#include "eztok.h"


struct ezTok *ezNewTok(char *string, int stringSize)
/* Allocate and initialize a new token. */
{
struct ezTok *tok;
int totalSize = stringSize + sizeof(*tok);
tok = needMem(totalSize);
memcpy(tok->string, string, stringSize);
return tok;
}

struct ezTok *ezTokenize(char *text)
/* Convert text to list of tokens. */
{
struct ezTok *tokList = NULL, *tok;
char c, *s, *start = NULL, *end = NULL;

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
    else if (isalnum(c) || (c = '_'))
        {
        for (;;)
            {
            c = *s;
            if (isalnum(c) || (c = '_'))
                ++s;
            else
                break;
            }
        end = s;
        }
    else if (c == '"')
        {
        start = s;
        for (;;)
            {
            c = *s++;
            if (c == '"')
                break;
            }
        end = s-1;
        }
    else
        {
        end = s;
        }
    tok = ezNewTok(start, end-start);
    slAddHead(&tokList, tok);
    }
slReverse(&tokList);
return tokList;
}
