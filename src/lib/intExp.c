/* Below is the worlds sleaziest little numerical expression
 * evaluator. Used to do only ints, now does doubles as well. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

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


static double expression();
/* Forward declaration of main expression handler. */

static double number()
/* Return number. */
{
double val;
if (tok == NULL)
    errAbort("Parse error in numerical expression");
if (!isdigit(tok->string[0]))
    errAbort("Expecting number, got %s", tok->string);
val = atof(tok->string);
nextTok();
return val;
}

static double atom()
/* Return parenthetical expression or number. */
{
double val;
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


static double uMinus()
/* Unary minus. */
{
double val;
if (tok->type == kxtSub)
    {
    nextTok();
    val = -atom();
    return val;
    }
else
    return atom();
}

static double mulDiv()
/* Multiplication or division. */
{
double val = uMinus();
for (;;)
    {
    if (tok->type == kxtMul)
	{
	nextTok();
	val *= uMinus();
	}
    else if (tok->type == kxtDiv)
	{
	nextTok();
	val /= uMinus();
	}
    else
        break;
    }
return val;
}

static double addSub()
/* Addition or subtraction. */
{
double val;
val = mulDiv();
for (;;)
    {
    if (tok->type == kxtAdd)
	{
	nextTok();
	val += mulDiv();
	}
    else if (tok->type == kxtSub)
	{
	nextTok();
	val -= mulDiv();
	}
    else
        break;
    }
return val;
}

static double expression()
/* Wraps around lowest level of expression. */
{
return addSub();
}

double doubleExp(char *text)
/* Convert text to double expression and evaluate. */
{
double val;
struct kxTok *tokList = tok = kxTokenize(text, FALSE);
val = expression();
slFreeList(&tokList);
return val;
}

int intExp(char *text)
/* Convert text to int expression and evaluate. */
{
return round(doubleExp(text));
}
