/* Below is the worlds sleaziest little integer expression
 * evaluator. */
#include "common.h"
#include "kxTok.h"

static struct kxTok *tok;

#define nextTok() (tok = tok->next) 

#ifdef DEBUG
static void nextTok()
/* Advance to next token. */
{
if (tok == NULL)
    printf("(null)");
else
    {
    printf("'%s' -> ", tok->string);
    if (tok->next == NULL)
        printf("(null)\n");
    else
        printf("'%s'\n", tok->next->string);
    }
tok = tok->next;
}
#endif /* DEBUG */


static int expression();
/* Forward declaration of main expression handler. */

static int number()
/* Return number. */
{
int val;
if (tok == NULL)
    errAbort("Parse error in numerical expression");
if (!isdigit(tok->string[0]))
    errAbort("Expecting integer, got %s", tok->string);
val = atoi(tok->string);
nextTok();
return val;
}

static int atom()
/* Return parenthetical expression or number. */
{
int val;
if (tok->type == kxtOpenParen)
    {
    nextTok();
    val = expression();
    if (tok->type == kxtCloseParen)
	{
        nextTok();
	return val;
	}
    else
	{
        errAbort("Unmatched parenthesis");
	return 0;
	}
    }
else
    return number();
}


static int uMinus()
/* Unary minus. */
{
int val;
if (tok->type == kxtSub)
    {
    nextTok();
    val = -atom();
    return val;
    }
else
    return atom();
}

static int mulDiv()
/* Multiplication or division. */
{
int val = uMinus();
if (tok->type == kxtMul)
    {
    nextTok();
    val *= mulDiv();
    }
else if (tok->type == kxtDiv)
    {
    nextTok();
    val /= mulDiv();
    }
return val;
}

static int addSub()
/* Addition or subtraction. */
{
int val;
val = mulDiv();
if (tok->type == kxtAdd)
    {
    nextTok();
    val += addSub();
    }
else if (tok->type == kxtSub)
    {
    nextTok();
    val -= addSub();
    }
return val;
}

static int expression()
/* Wraps around lowest level of expression. */
{
return addSub();
}

int intExp(char *text)
/* Convert text to integer expression and evaluate. */
{
int val;
struct kxTok *tokList = tok = kxTokenize(text, FALSE);
val = expression();
slFreeList(&tokList);
return val;
}

