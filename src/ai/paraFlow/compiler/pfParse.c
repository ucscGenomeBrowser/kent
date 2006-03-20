/* pfParse build up parse tree from token stream.  */
/* Copyright 2005-6 Jim Kent.  All rights reserved. */
/* This is a pretty standard recursive descent parser.  The expression
 * parsing is handled with a function at each operator precedence level 
 * that handles all the operators at that level, and which calls the 
 * function that * handles operators at the next higher expression level 
 * for input.  The highest precedence level also looks for parenthesis,
 * and if it finds them, then calls back to the lowest level to handle
 * what's inside the parenthesis.
 *
 * At the statement level, all statements begin with a key word.
 * If no key word is found then the statement is passed off to the
 * expression level.  The "key word" '{' indicates the start of a
 * block of code, and works similarly to the parenthesis, but at
 * the statement rather than the expression level.
 *
 * The trickiest thing here is the handling of variable declarations.
 * Comma separated lists of variable declarations can occur in a
 * number of places - in the input and output declarations of a 
 * function, in declaring member variables of a class, and in
 * handling normal variable declarations. The grammar for a variable
 * declaration list should be:
 *     varDecList: typedList
 *          varDecList ',' typedList
 *     typedList: qualifiedType nameList
 *     qualifiedType: qualifier typeExpression
 *     qualifier: 'static' | 'local' | 'readable' | 'global'
 *     typeExpression: typeName
 *          array '[' expression ']' 'of' typeExpression
 *          array 'of' typeExpression
 *          dir 'of' typeExpression
 *     nameList: name
 *          nameList ',' name
 *     typeName: name
 *          module '.' name
 * Currently it's not exactly this, but it should be soon.
 *
 * A problem the compiler faces is how does it tell a type name
 * from any other name, particularly since variables and types
 * can both user the same name.  In particular if 'obj' is a type
 * name then you can declare a variable named obj of type obj as
 * so:
 *    obj obj;
 * The only hope of resolving this lies in the fact that a declaration
 * is the only place in the language where two names are allowed
 * together without an intervening operator.  Unfortunately
 * the more general form:
 *    <typeExpression> name, ..., name
 * the typeExpression could have generated quite an elaborate parse
 * tree before it figures out that it really wasn't a type expression
 * after all, but perhaps something like:
 *     obj = 10;
 * that was being parsed.   The current parse is allowed as much
 * lookahead into the token stream as it wants, but I'm reluctant
 * to let it back-track as well.  
 *
 * The current solution to this is to put variable declarations
 * at the level of precedence right under the = operations, which
 * in turn are right under tuple.  The tuple is then responsible
 * for seeing if the first item it sees is a variable declaration,
 * and if so, making sure all of the others are compatible with
 * being a var-decs too. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"

struct pfParse *pfParseStatement(struct pfCompile *pfc, struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope);

struct pfParse *pfParseExpression(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope);

static struct pfParse *parseOf(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope);
/* Parse out name of name . */

static void syntaxError(struct pfToken *tok, int code)
/* Complain about syntax error and quit. */
{
errAt(tok, "Syntax error #%d", code);
}

static void eatSemi(struct pfToken **pTokList)
/* Make sure there's a semicolon, and eat it. */
{
struct pfToken *tok = *pTokList;
if (tok->type != '}')
    {
    if (tok->type != ';')
	expectingGot(";", tok);
    tok = tok->next;
    *pTokList = tok;
    }
}
    	
struct pfParse *pfParseNew(enum pfParseType type,
	struct pfToken *tok, struct pfParse *parent, struct pfScope *scope)
/* Return new parse node.  It's up to caller to fill in
 * children later. */
{
struct pfParse *pp;
AllocVar(pp);
pp->type = type;
pp->tok = tok;
pp->parent = parent;
pp->scope = scope;
return pp;
}

boolean pfParseIsConst(struct pfParse *pp)
/* Return TRUE if pp is a simple constant. */
{
switch (pp->type)
    {
    case pptConstBit:
    case pptConstByte:
    case pptConstChar:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
    case pptConstString:
        return TRUE;
    default:
        return FALSE;
    }
}

void pfParsePutChildrenInPlaceOfSelf(struct pfParse **pPp)
/* Replace self with children. */
{
struct pfParse *pp = *pPp;
if (pp->children == NULL)
    *pPp = pp->next;
else
    {
    struct pfParse *lastChild = slLastEl(pp->children);
    lastChild->next = pp->next;
    *pPp = pp->children;
    }
}

struct pfParse *pfParseEnclosingFunction(struct pfParse *pp)
/* Find enclosing function if any.  */
{
for (pp = pp->parent; pp != NULL; pp = pp->parent)
    {
    if (pp->type == pptToDec || pp->type == pptFlowDec)
	{
        return pp;
	}
    }
return NULL;
}

struct pfParse *pfParseEnclosingClass(struct pfParse *pp)
/* Find enclosing class if any.  It treats self as enclosing self.  */
{
for (pp = pp->parent; pp != NULL; pp = pp->parent)
    {
    if (pp->type == pptClass)
	{
        return pp;
	}
    }
return NULL;
}

struct pfParse *emptyTuple(struct pfParse *parent, struct pfToken *tok,
	struct pfScope *scope)
/* Return empty tuple. */
{
return pfParseNew(pptTuple, tok, parent, scope);
}

struct pfParse *emptyStatement(struct pfParse *parent, struct pfToken *tok,
	struct pfScope *scope)
/* Return no-op statement. */
{
return pfParseNew(pptNop, tok, parent, scope);
}

struct pfParse *pfSingleTuple(struct pfParse *parent, struct pfToken *tok, 
	struct pfParse *single)
/* Wrap tuple around single and return it.  . */
{
struct pfParse *tuple = pfParseNew(pptTuple, tok, parent, parent->scope);
tuple->children = single;
tuple->next = single->next;
single->parent = tuple;
single->next = NULL;
return tuple;
}

static void skipRequiredWord(char *name, struct pfToken **pTokList)
/* Make sure next token matches name, and then skip it. */
{
struct pfToken *tok = *pTokList;
if (tok->type < pftReservedWordStart || !sameString(tok->val.s, name))
    expectingGot(name, tok);
*pTokList = tok->next;
}

static void skipRequiredCharType(enum pfTokType type, struct pfToken **pTokList)
/* Make sure next token is of given type simple char type, and then skip it. */
{
struct pfToken *tok = *pTokList;
if (tok->type != type)
    {
    char sym[2];
    sym[0] = type;
    sym[1] = 0;
    expectingGot(sym, tok);
    }
*pTokList = tok->next;
}

static struct pfParse *parseNameUse(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope)
/* Make sure have a name, and create a varUse type node
 * based on it. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptNameUse, tok, parent, scope);
if (tok->type != pftName)
    {
    errAt(tok, "Expecting variable name.");
    }
pp->name = tok->val.s;
*pTokList = tok->next;
return pp;
}

static struct pfParse *parseInputTuple(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope, struct pfToken **pCloseParenTok)
/* Parse input/output tuple.  Similar to a regular tuple, but
 * () can be an empty tuple, and (x) is interpreted as a tuple
 * of one rather than just x. */
{
struct pfToken *tok = *pTokList;
struct pfParse *tuple;

skipRequiredCharType('(', &tok);
if (tok->type == ')')
    {
    *pCloseParenTok = tok;
    tuple = emptyTuple(parent, tok, scope);
    tok = tok->next;
    }
else
    {
    tuple = pfParseExpression(pfc, parent, &tok, scope);
    if (tuple->type != pptTuple)
	tuple = pfSingleTuple(parent, tok, tuple);
    *pCloseParenTok = tok;
    skipRequiredCharType(')', &tok);
    }
tuple->type = pptTypeTuple;
*pTokList = tok;
return tuple;
}

static void parseFunctionIo(struct pfCompile *pfc,
    struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope, 
    struct pfParse **retInput, struct pfParse **retOutput)
/* Parse out something like (int x, int y) into (int z)
 * into input and output, both of which will be (possibly empty)
 * pptTuples. */
{
struct pfToken *tok = *pTokList;
struct pfParse *input, *output = NULL;
struct pfToken *outToken;
input = parseInputTuple(pfc, parent, &tok, scope, &outToken);
if (tok->type == pftInto)
    {
    boolean gotParen = FALSE;
    tok = tok->next;
    outToken = tok;
    /* To handle function pointer declarations need to parse
     * out parens here rather than leaving it to parse expression */
    if (tok->type == '(')
        {
	gotParen = TRUE;
	tok = tok->next;
	}
    output = pfParseExpression(pfc, parent, &tok, scope);
    if (gotParen)
	skipRequiredCharType(')', &tok);
    if (output->type != pptTuple)
	output = pfSingleTuple(parent, outToken, output);
    }
else
    {
    output = emptyTuple(parent, outToken, scope);
    }
output->type  = pptTypeTuple;

*pTokList = tok;
*retInput = input;
*retOutput = output;
}
	
static void decsToFormal(struct pfParse *pp)
/* Convert pptVarInits to pptFormalParameters on children */
{
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    if (pp->type != pptVarInit)
        errAt(pp->tok, "expecting formal parameter");
    pp->type = pptFormalParameter;
    }
}

static struct pfParse *parseShortFuncType(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Parse out something like:
 *      to (int x,y=2) into (int f) 
 * Into a parse tree that looks like
 *      pptTypeToPt	 (or pptTypeFlowPt)
 *         pptTypeTuple
 *            pptFormalParameter x
 *               pptTypeName int
 *               pptSymName x
 *            pptFormalParameter y
 *               pptTypeName int
 *               pptSymName y
 *               pptConstUse 2 
 *         pptTypeTuple
 *            pptFormalParameter f
 *               pptTypeName int
 *               pptSymName f
 */
{
struct pfToken *tok = *pTokList;
enum pfParseType ppt = (tok->type == pftTo ? pptTypeToPt : pptTypeFlowPt);
struct pfScope *funcScope = pfScopeNew(pfc, scope, 0, NULL);
struct pfParse *pp = pfParseNew(ppt, tok, parent, funcScope);
struct pfParse *input, *output;

tok = tok->next;	/* Skip over 'to' or 'flow' */
parseFunctionIo(pfc, pp, &tok, funcScope, &input, &output);
decsToFormal(input);
decsToFormal(output);
pp->children = input;
input->next = output;
*pTokList = tok;
return pp;
}

static struct pfParse *constUse(struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope)
/* Create constant use */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptConstUse, tok, parent, scope);
*pTokList = tok->next;
return pp;
}

static struct pfParse *parseAtom(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse atomic expression - const, variable, or parenthesized expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = NULL;
switch (tok->type)
    {
    case pftName:
	pp = parseNameUse(parent, &tok, scope);
        break;
    case pftString:
    case pftSubstitute:
    case pftInt:
    case pftLong:
    case pftFloat:
    case pftNil:
        pp = constUse(parent, &tok, scope);
	break;
    case '(':
        {
	struct pfToken *startTok = tok;
	tok = tok->next;
	if (tok->type == ')')
	    {
	    pp = emptyTuple(parent, tok, scope);
	    }
	else
	    {
	    pp = pfParseExpression(pfc, parent, &tok, scope);
	    if (tok->type != ')')
		errAt(tok, "Unclosed parenthesis.");
	    }
	tok = tok->next;
	break;
	}
    default:
        syntaxError(tok, 1);
	break;
    }
*pTokList = tok;
return pp;
}

static struct pfParse *parseArrayCallDot(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse out this.that()[].other[]().more and the like. */
{
struct pfParse *pp = parseAtom(pfc, parent, pTokList, scope);
struct pfToken *tok = *pTokList;
while (tok->type == '(' || tok->type == '[' || tok->type == '.')
    {
    enum pfTokType tokType = tok->type;
    struct pfParse *left = pp, *right;
    tok = tok->next;
    switch (tokType)
        {
	case '(':
	    pp = pfParseNew(pptCall, tok, parent, scope);
	    if (tok->type == ')')
	        right = emptyTuple(pp, tok, scope);
	    else
	        {
		right = pfParseExpression(pfc, pp, &tok, scope);
		if (right->type != pptTuple)
		    {
		    struct pfParse *tuple = pfSingleTuple(pp, tok, right);
		    right->parent = tuple;
		    right = tuple;
		    }
		}
	    skipRequiredCharType(')', &tok);
	    break;
	case '[':
	    pp = pfParseNew(pptIndex, tok, parent, scope);
	    right = pfParseExpression(pfc, pp, &tok, scope);
	    skipRequiredCharType(']', &tok);
	    break;
	case '.':
	    pp = pfParseNew(pptDot, tok, parent, scope);
	    right = parseAtom(pfc, pp, &tok, scope);
	    break;
	default:
	    internalErr();
	    right = NULL;
	    break;
	}
    left->parent = pp;
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}


static struct pfParse *parseParaInvoke(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope, boolean isStatement)
/* Parse para invokation.  This is of general form:
 *    'para' type var 'in' collection action expression
 * where action can be do, add, multiply or, and, min, max, top intExpression,
 * sample intExpression, filter, or get.  In the case where the action is 'do'
 * then the action is followed by a statement rather than an expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
struct pfParse *element;
struct pfParse *collection;
struct pfParse *body;
boolean actionNeedsStatement = FALSE;
char *action = NULL;
enum pfParseType paraType = pptNone;

/* Preliminaries, scope and as of yet untyped parse node for para. */
scope = pfScopeNew(pfc, scope, 1, NULL);
tok = tok->next;	/* Skip over para. */
skipRequiredCharType('(', &tok);
pp = pfParseNew(pptNone, tok, parent, scope);

/* Parse out the element in collection */
element = pfParseExpression(pfc, pp, &tok, scope);
skipRequiredWord("in", &tok);
collection = pfParseExpression(pfc, pp, &tok, scope);

skipRequiredCharType(')', &tok);

/* Parse out the action keyword, and figure out what to do depending on
 * action.  Determine parse node type from action. */
action = pfTokenAsString(tok);
if (sameString("do", action))
    {
    paraType = pptParaDo;
    actionNeedsStatement = TRUE;
    }
else if (sameString("+", action))
    paraType = pptParaAdd;
else if (sameString("*", action))
    paraType = pptParaMultiply;
else if (sameString("&&", action))
    paraType = pptParaAnd;
else if (sameString("||", action))
    paraType = pptParaOr;
else if (sameString("min", action))
    paraType = pptParaMin;
else if (sameString("max", action))
    paraType = pptParaMax;
else if (sameString("get", action))
    paraType = pptParaGet;
else if (sameString("argmax", action))
    paraType = pptParaArgMax;
else if (sameString("argmin", action))
    paraType = pptParaArgMin;
else if (sameString("filter", action))
    paraType = pptParaFilter;
else
    errAt(tok, "unrecognized para action");
if (isStatement != actionNeedsStatement)
    {
    if (isStatement)
        errAt(tok, "para action \"%s\" has a value that needs to be used", action);
    else
        errAt(tok, "para action \"%s\" has no value", action);
    }
freez(&action);
tok = tok->next;
pp->type = paraType;

/* Get expression or statement that gets executed in parallel. */
if (actionNeedsStatement)
    body = pfParseStatement(pfc, pp, &tok, scope);
else
    {
    body = pfParseExpression(pfc, pp, &tok, scope);
    if (isStatement)
	eatSemi(&tok);
    }

/* Hang various things off of para parse node. */
slAddHead(&pp->children, body);
slAddHead(&pp->children, element);
slAddHead(&pp->children, collection);
*pTokList = tok;
return pp;
}

static void markModuleDotType(struct pfCompile *pfc, struct pfParse *pp)
/* This type dot is in a context where it better be module.type.
 * We mark it as such.  We leave it to a later pass to make sure
 * that the module really is a module and the type really is a type
 * in that module. */
{
struct pfParse *left = pp->children;
struct pfParse *right = left->next;
if (left->type == pptNameUse && right->type == pptNameUse)
    {
    pp->type = pptModuleDotType;
    }
else
    {
    errAt(pp->tok, "misplaced dot in type expression");
    }
}

static void checkIsTypeExp(struct pfCompile *pfc, 
	struct pfParse *pp, struct pfScope *scope)
/* Make sure that subtree is all type expression. */
{
switch (pp->type)
    {
    case pptNameUse:
        if (!pfScopeFindType(scope, pp->name))
	     errAt(pp->tok, "%s is not a type name.", pp->name);
	pp->type = pptTypeName;
	break;
    case pptIndex:
        checkIsTypeExp(pfc, pp->children, scope);
	break;
    case pptOf:
        for (pp = pp->children; pp != NULL; pp = pp->next)
	    checkIsTypeExp(pfc, pp, scope);
	break;
    case pptTypeFlowPt:
    case pptTypeToPt:
        break;
    case pptDot:
	markModuleDotType(pfc, pp);
        break;
    default:
	{
	expectingGot("type expression", pp->tok);
	break;
	}
    }
}

static void submergeTypeIndex(struct pfParse **pPp)
/* Convert a subtree that looks like so:
 *        pptIndex
 *           pptName
 *           expressionTree
 * to something that looks like so:
 *       pptName
 *          expressionTree
 * Why to do this?  Well it's what the rest of the typing
 * system expects from an earlier design of the type expression
 * parse. */
{
struct pfParse *pp = *pPp;
if (pp->type == pptIndex)
    {
    struct pfParse *name = pp->children;
    struct pfParse *exp = name->next;
    name->next = pp->next;
    name->children = exp;
    *pPp = name;
    }
else
    {
    for (pPp = &pp->children; *pPp != NULL; pPp = &(*pPp)->next)
        submergeTypeIndex(pPp);
    }
}

static void massageIntoType(struct pfCompile *pfc, struct pfParse **pPp,
	struct pfScope *scope)
/* Given a parse tree that is just known to be some sort of expression,
 * make sure it's a type expression, and do any necessary reworking
 * of the subtree that that entails. */
{
checkIsTypeExp(pfc, *pPp, scope);
submergeTypeIndex(pPp);
}

static struct pfParse *parseNewObject(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse things of form  new type(init parameters) */
{
struct pfToken *tok = *pTokList, *newTok;
struct pfParse *newPp;
newTok = tok;
tok = tok->next;   /* We covered 'new' */
newPp = parseOf(pfc, parent, &tok, scope);
if (newPp->type != pptCall)
   errAt(newTok, "missing input tuple to new operator");
massageIntoType(pfc, &newPp->children, scope);
newPp->type = pptNew;
*pTokList = tok;
return newPp;
}


static struct pfParse *parseParaOrNew(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse expression that might include 'para' */
{
struct pfToken *tok = *pTokList;
switch (tok->type)
    {
    case pftPara:
	return parseParaInvoke(pfc, parent, pTokList, scope, FALSE);
    case pftNew:
        return parseNewObject(pfc, parent, pTokList, scope);
    default:
	return parseArrayCallDot(pfc, parent, pTokList, scope);
    }
}

static void makeIncrementNode(struct pfParse *pp,
	struct pfParse *exp, struct pfToken *incTok, struct pfScope *scope)
/* Add children to pp, which is a += node, so as to
 * make it into an increment/decrement operation. */
{
struct pfToken *constantTok;
struct pfParse *constant;

/* We have to fake a token because of the way the constant
 * handling code works. */
constantTok = CloneVar(incTok);
constantTok->type = pftInt;
constantTok->val.i = (incTok->type == pftPlusPlus ? 1 : -1);
constant = pfParseNew(pptConstUse, constantTok, pp, scope);

/* Hook everything onto pp. */
pp->children = exp;
exp->next = constant;
}

static struct pfParse *parseIncrement(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse ++exp or --exp */
{
struct pfToken *tok = *pTokList;
struct pfParse *exp, *pp;
struct pfToken *incTok;

switch (tok->type)
    {
    case pftPlusPlus:
    case pftMinusMinus:
	{
	incTok = tok;
	tok = tok->next;
	pp = pfParseNew(pptPlusEquals, tok, parent, scope);
	exp = parseParaOrNew(pfc, pp, &tok, scope);
	makeIncrementNode(pp, exp, incTok, scope);
	*pTokList = tok;
	return pp;
	}
    }
exp = parseParaOrNew(pfc, parent, pTokList, scope);
tok = *pTokList;
switch (tok->type)
    {
    case pftPlusPlus:
    case pftMinusMinus:
	{
	incTok = tok;
	tok = tok->next;
	pp = pfParseNew(pptPlusEquals, tok, parent, scope);
	makeIncrementNode(pp, exp, incTok, scope);
	*pTokList = tok;
	return pp;
	}
    }
return exp;
}

static struct pfParse *parseNegation(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse unary minus, and other things at that level. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
if (tok->type == '-')
    {
    pp = pfParseNew(pptNegate, tok, parent, scope);
    tok = tok->next;
    pp->children = parseNegation(pfc, pp, &tok, scope);
    }
else if (tok->type == '!')
    {
    pp = pfParseNew(pptNot, tok, parent, scope);
    tok = tok->next;
    pp->children = parseNegation(pfc, pp, &tok, scope);
    }
else if (tok->type == '~')
    {
    pp = pfParseNew(pptFlipBits, tok, parent, scope);
    tok = tok->next;
    pp->children = parseNegation(pfc, pp, &tok, scope);
    }
else
    {
    pp = parseIncrement(pfc, parent, &tok, scope);
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseProduct(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse things separated by '*' or '/' or '%'. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseNegation(pfc, parent, &tok, scope);
while (tok->type == '*' || tok->type == '/' || tok->type == '%'
	|| tok->type == pftShiftLeft || tok->type == pftShiftRight
	|| tok->type == '&' && tok->next->type != ')')
    {
    struct pfParse *left = pp, *right;
    enum pfTokType tt = pptNone;
    switch (tok->type)
        {
	case '*':
	   tt = pptMul;
	   break;
	case '/':
	   tt = pptDiv;
	   break;
	case '%':
	   tt = pptMod;
	   break;
	case pftShiftLeft:
	   tt = pptShiftLeft;
	   break;
	case pftShiftRight:
	   tt = pptShiftRight;
	   break;
	case '&':
	   tt = pptBitAnd;
	   break;
	}
    pp = pfParseNew(tt, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseNegation(pfc, pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseSum(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse + and - part of expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseProduct(pfc, parent, &tok, scope);
while (tok->type == '+' || tok->type == '-' || tok->type == '|' || tok->type == '^' && tok->next->type != ')')
    {
    struct pfParse *left = pp, *right;
    enum pfTokType tt = pptNone;
    switch (tok->type)
        {
	case '+':
	   tt = pptPlus;
	   break;
	case '-':
	   tt = pptMinus;
	   break;
	case '|':
	   tt = pptBitOr;
	   break;
	case '^':
	   tt = pptBitXor;
	   break;
	}
    pp = pfParseNew(tt, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseProduct(pfc, pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseCmp(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse == >=, <= > < and != part of expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseSum(pfc, parent, &tok, scope);
if (tok->type == pftEqualsEquals || tok->type == pftNotEquals 
	|| tok->type == pftGreaterOrEquals || tok->type == pftLessOrEquals
	|| tok->type == '<' || tok->type == '>')
    {
    struct pfParse *left = pp, *right;
    enum pfTokType tt = 0;
    switch (tok->type)
	{
	case pftEqualsEquals:
	    tt = pptSame;
	    break;
	case pftNotEquals:
	    tt = pptNotSame;
	    break;
	case pftGreaterOrEquals:
	    tt = pptGreaterOrEquals;
	    break;
	case pftLessOrEquals:
	    tt = pptLessOrEquals;
	    break;
	case '>':
	    tt = pptGreater;
	    break;
	case '<':
	    tt = pptLess;
	    break;
	}
    pp = pfParseNew(tt, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseSum(pfc, pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseLogAnd(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse logical and. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseCmp(pfc, parent, &tok, scope);
while (tok->type == pftLogAnd && tok->next->type != ')')
    {
    struct pfParse *left = pp, *right;
    pp = pfParseNew(pptLogAnd, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseCmp(pfc, pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseLogOr(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse logical and. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseLogAnd(pfc, parent, &tok, scope);
while (tok->type == pftLogOr && tok->next->type != ')')
    {
    struct pfParse *left = pp, *right;
    pp = pfParseNew(pptLogOr, tok, parent, scope);
    left->parent = pp;
    tok = tok->next;
    right = parseLogAnd(pfc, pp, &tok, scope);
    pp->children = left;
    left->next = right;
    }
*pTokList = tok;
return pp;
}

struct pfParse *parseIndexRange(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse range (X to Y) */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseLogOr(pfc, parent, &tok, scope);
if (tok->type == pftTil)
     {
     struct pfParse *left = pp, *right;
     pp = pfParseNew(pptIndexRange, tok, parent, scope);
     left->parent = pp;
     tok = tok->next;
     right = parseLogOr(pfc, parent, &tok, scope);
     pp->children = left;
     left->next = right;
     }
*pTokList = tok;
return pp;
}

struct pfParse *parseVarOfFunc(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* If it starts with 'var of to/flow' then parse out a function pointer
 * declaration.  Else use parseIndexRange. */
{
struct pfToken *tok = *pTokList;
if (tok->type == pftName && sameString(tok->val.s, "var"))
    {
    struct pfToken *ofTok = tok->next;
    if (ofTok->type == pftOf)
        {
	struct pfToken *funcTok = ofTok->next;
	if (funcTok->type == pftFlow || funcTok->type == pftTo)
	    {
	    *pTokList = funcTok;
	    return parseShortFuncType(pfc, parent, pTokList, scope);
	    }
	}
    }
return parseIndexRange(pfc, parent, pTokList, scope);
}


static struct pfParse *parseOf(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse out name of name . */
{
struct pfParse *pp = parseVarOfFunc(pfc, parent, pTokList, scope);
struct pfToken *tok = *pTokList;

if (tok->type == pftOf)
    {
    struct pfParse *of = pfParseNew(pptOf, tok, parent, scope);
    pp->parent = of;
    of->children = pp;
    while (tok->type == pftOf)
        {
	tok = tok->next;
	pp = parseVarOfFunc(pfc, of, &tok, scope);
	slAddHead(&of->children, pp);
	}
    slReverse(&of->children);
    *pTokList= tok;
    return of;
    }
else
    return pp;
}

static boolean inFunction(struct pfParse *pp)
/* Return TRUE if pp or one of it's parents is a function. */
{
while (pp != NULL)
    {
    if (pp->type == pptToDec || pp->type == pptFlowDec)
	return TRUE;
    pp = pp->parent;
    }
return FALSE;
}

static void rSetAccess(struct pfParse *pp, enum pfAccessType access, bool isConst)
/* Set access type for self and descendents. */
{
pp->access = access;
pp->isConst = isConst;
for (pp = pp->children; pp != NULL; pp = pp->next)
    rSetAccess(pp, access, isConst);
}

static struct pfParse *parseVarDec(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse something of the form [static] typeExp varName */
{
struct pfToken *tok = *pTokList, *constTok = NULL;
struct pfParse *pp;
bool inAccessSection = TRUE, isConst = FALSE;
enum pfAccessType access = paUsual;

while (inAccessSection)
    {
    enum pfTokType tokType = tok->type;
    switch (tokType)
        {
	case pftGlobal:
	case pftLocal:
	case pftReadable:
	case pftStatic:
	case pftWritable:
	    {
	    boolean inFunc = inFunction(parent);
	    if (access != paUsual)
	        errAt(tok, "Only one of global/readable/const/writable/local/static allowed.");
	    switch (tokType)
	        {
		case pftGlobal:
		    access = paGlobal;
		    if (inFunc)
			errAt(tok, "'global' inside function");
		    break;
		case pftLocal:
		    if (scope->module)
		        errAt(tok, "'local' unneeded here");
		    access = paLocal;
		    break;
		case pftReadable:
		    if (scope->module)
		        errAt(tok, "use global rather than readable here");
		    access = paReadable;
		    break;
		case pftStatic:
		    access = paStatic;
		    if (!inFunc)
			errAt(tok, "'static' outside of function");
		    break;
		case pftWritable:
		    access = paWritable;
		    break;
		}
	    tok = tok->next;
	    break;
	    }
	default:
	    inAccessSection = FALSE;
	    break;
	}
    }
if (tok->type == pftConst)
    {
    isConst = TRUE;
    constTok = tok;
    tok = tok->next;
    }
pp = parseOf(pfc, parent, &tok, scope);
if (tok->type == pftName)
    {
    struct pfParse *dec = pfParseNew(pptVarDec, tok, parent, scope);
    struct pfParse *type = pp;
    struct pfParse *name = parseNameUse(dec, &tok, scope);
    massageIntoType(pfc, &type, scope);
    pp->parent = dec;
    dec->children = type;
    type->next = name;
    dec->access = access;
    dec->isConst = isConst;
    rSetAccess(type, access, isConst);
    dec->name = name->name;
    pp = dec;
    }
else if (isConst)
    errAt(constTok, "Misplaced const");
*pTokList = tok;
return pp;
}

static void checkNoNestedAssigns(struct pfParse *pp)
/* Make sure that there are no assignments inside of subtree. */
{
switch (pp->type)
    {
    case pptAssign:
    case pptPlusEquals:
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
        errAt(pp->tok, "nested assignments not allowed in paraFlow");
	break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    checkNoNestedAssigns(pp);
}

struct pfParse *parseAssign(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse '=' separated expression */
{
struct pfToken *tok = *pTokList;
struct pfParse *assign = NULL;
enum pfParseType type = pptNone;
struct pfParse *pp = parseVarDec(pfc, parent, &tok, scope);
switch (tok->type)
    {
    case '=':
        type = pptAssign;
	break;
    case pftPlusEquals:
	type = pptPlusEquals;
	break;
    case pftMinusEquals:
	type = pptMinusEquals;
	break;
    case pftMulEquals:
	type = pptMulEquals;
	break;
    case pftDivEquals:
	type = pptDivEquals;
	break;
    case '@':
        type = pptKeyVal;
	break;
    }
if (type != pptNone)
    {
    if (pp->type == pptVarDec && type != pptAssign)
        errAt(tok, "You can only initialize a variable with a simple '='");
    assign = pfParseNew(type, tok, parent, scope);
    assign->children = pp;
    assign->access = pp->access;
    assign->isConst = pp->isConst;
    assign->name = pp->name;
    pp->parent = assign;
    for (;;)
	{
	tok = tok->next;
	pp = parseIndexRange(pfc, assign, &tok, scope);
	checkNoNestedAssigns(pp);
	slAddHead(&assign->children, pp);
	if (tok->type != '=' || type != pptAssign)
	    break;
	}
    }
*pTokList = tok;
if (assign != NULL)
    {
    slReverse(&assign->children);
    return assign;
    }
else
    return pp;
}

static void flipNameUseToVarDec(struct pfParse *pp, struct pfParse *type, 
	struct pfParse *parent)
{
struct pfParse *dupeType, *dupeName;

AllocVar(dupeName);
*dupeName = *pp;

AllocVar(dupeType);
*dupeType = *type;
pp->children = dupeType;
dupeType->next = dupeName;
dupeName->next = NULL;
pp->type = pptVarDec;
pp->access = type->access;
pp->isConst = type->isConst;
}

static boolean anyVarDecs(struct pfParse *pp)
/* Return TRUE if there are any varDecs in subtree */
{
if (pp->type == pptVarDec)
    return TRUE;
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    if (anyVarDecs(pp))
        return TRUE;
    }
return FALSE;
}

static boolean isVarDecOrVarDecInit(struct pfParse *pp)
/* Return true if pp is either varDec or assignment to varDec. */
{
return (pp->type == pptVarDec 
	|| (pp->type == pptAssign && pp->children->type == pptVarDec));
}

static void updateAccessAndConst(struct pfParse *pp, struct pfParse *type, 
	enum pfAccessType *pAccess, bool *pIsConst)
/* If pp has access or const modifiers then update pAccess and pConst
 * with them.  Otherwise use pAccess and pConst to set them. */
{
if (pp->access == paUsual)
    type->access = pp->access = *pAccess;
else
    *pAccess = pp->access;
if (pp->isConst)
    *pIsConst = TRUE;
else
    type->isConst = pp->isConst = *pIsConst;
}

static void addMissingTypesInDeclareTuple(struct pfParse *tuple)
/* If first element of tuple is typed, then make sure rest
 * is typed too. */
{
struct pfParse *vars = tuple->children, *next;
struct pfParse *type = NULL;
struct pfParse *pp;
enum pfAccessType access = vars->access; 
bool isConst = vars->isConst;

/* Make sure first item in tuple is a var dec. */
if (!isVarDecOrVarDecInit(vars))
    errAt(vars->tok, "Missing type information.");

/* Make sure everything is either a varDec or an assignment */
for (pp = vars; pp != NULL; pp = pp->next)
    {
    if (pp->type != pptVarDec && pp->type != pptAssign && 
    	pp->type != pptNameUse)
	{
        errAt(pp->tok, "Expecting variable declaration");
	}
    }

/* Fill in any missing types from type of previous one in list.
 * We've guaranteed now that first one will have a type. */
for (pp = vars; pp != NULL; pp = pp->next)
    {
    if (pp->type == pptVarDec)
	{
	type = pp->children;
	pp->name = type->next->name;
	updateAccessAndConst(pp, type, &access, &isConst);
	}
    else if (pp->type == pptNameUse)
	{
	flipNameUseToVarDec(pp, type, tuple);
	}
    else if (pp->type == pptAssign)
	{
	struct pfParse *sub = pp->children;
	if (sub->type == pptVarDec)
	    {
	    type = sub->children;
	    updateAccessAndConst(sub, type, &access, &isConst);
	    }
	else if (sub->type == pptNameUse)
	    {
	    flipNameUseToVarDec(sub, type, pp);
	    pp->access = sub->access;
	    pp->isConst = sub->isConst;
	    pp->ty = sub->ty;
	    }
	}
    }
}

static void varDecAndAssignToVarInit(struct pfParse *pp)
/* Convert pptVarDec and when appropriate pptAssign to pptVarInit. */
{
if (pp->type == pptVarDec)
    {
    pp->type = pptVarInit;
    if (pp->isConst)
	{
	boolean ok = FALSE;
	if (pp->parent->type == pptTuple || pp->parent->type == pptTypeTuple)
	    {
	    enum pfParseType grandparentType = pp->parent->parent->type;
	    if (grandparentType == pptToDec || grandparentType == pptFlowDec)
	        ok = TRUE;
	    }
	if (!ok)
	    errAt(pp->tok, "Uninitialized const.");
	}
    }
else if (pp->type == pptAssign)
    {
    struct pfParse *left = pp->children;
    if (left->type == pptVarDec)
        {
	struct pfParse *right = left->next;
	pp->type = pptVarInit;
	pp->access = left->access;
	pp->isConst = left->isConst;
	pp->children = left->children;
	slAddTail(&pp->children, right);
	}
    }
if (pp->type == pptVarInit || pp->type == pptFormalParameter)
    {
    struct pfParse *type = pp->children;
    struct pfParse *name = type->next;
    name->type = pptSymName;
    pp->name = name->name;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    varDecAndAssignToVarInit(pp);
}

static struct pfParse *parseTuple(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse , separated expression. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = parseAssign(pfc, parent, &tok, scope);
struct pfParse *tuple = NULL;
if (tok->type == ',')
    {
    tuple = pfParseNew(pptTuple, tok, parent, scope);
    tuple->children = pp;
    pp->parent = tuple;
    while (tok->type == ',')
	{
	tok = tok->next;
	if (tok->type == ')')
	    break;
	pp = parseAssign(pfc, tuple, &tok, scope);
	slAddHead(&tuple->children, pp);
	}
    }
*pTokList = tok;
if (tuple != NULL)
    {
    slReverse(&tuple->children);
    if (anyVarDecs(tuple))
	{
	addMissingTypesInDeclareTuple(tuple);
	varDecAndAssignToVarInit(tuple);
	}
    return tuple;
    }
else
    {
    if (isVarDecOrVarDecInit(pp))
	varDecAndAssignToVarInit(pp);
    return pp;
    }
}


struct pfParse *pfParseExpression(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse expression. */
{
return parseTuple(pfc, parent, pTokList, scope);
}

static struct pfParse *parseCompound(struct pfCompile *pfc,
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse a compound statement (statement list surrounded
 * by brackets */
{
struct pfParse *pp;
struct pfToken *tok = *pTokList;
struct pfParse *statement;
if (tok->type != '{')
    expectingGot("{", tok);
tok->val.scope->parent = scope;
scope = tok->val.scope;
pp = pfParseNew(pptCompound, *pTokList, parent, scope);
tok = tok->next;
for (;;)
    {
    if (tok == NULL)
	{
	errAt(*pTokList, "End of file in compound statement");
	}
    if (tok->type == '}')
        {
	tok = tok->next;
	break;
	}
    statement = pfParseStatement(pfc, pp, &tok, scope);
    slAddHead(&pp->children, statement);
    }
*pTokList = tok;
slReverse(&pp->children);
return pp;
}

static struct pfParse *parseFunction(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope, enum pfParseType type)
/* Parse something (...) [into (...)] */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
struct pfParse *name, *input, *output = NULL, *body = NULL;

scope = pfScopeNew(pfc, scope, 0, NULL);
pfScopeMarkLocal(scope);
pp = pfParseNew(type, tok, parent, scope);
tok = tok->next;	/* Skip to/flow (implicit in type) */
name = parseNameUse(parent, &tok, scope);
pp->name = name->name;

parseFunctionIo(pfc, pp, &tok, scope, &input, &output);

if (pfc->isPfh || 
	(parent->type == pptCompound && parent->parent->type == pptInterface))
    eatSemi(&tok);
else
    {
    body = parseCompound(pfc, pp, &tok, scope);
    pfScopeMarkLocal(body->scope);
    slAddHead(&pp->children, body);
    }

slAddHead(&pp->children, output);
slAddHead(&pp->children, input);
slAddHead(&pp->children, name);
*pTokList = tok;
return pp;
}

static struct pfParse *parseTo(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse to (...) [into (...)] */
{
return parseFunction(pfc, parent, pTokList, scope, pptToDec);
}


static struct pfParse *parseFlow(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse flow (...) [into (...)] */
{
return parseFunction(pfc, parent, pTokList, scope, pptFlowDec);
}

static struct pfParse *parseOperator(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse to (...) [into (...)] */
{
if (!pfc->isSys)
    {
    struct pfToken *tok = *pTokList;
    errAt(tok, "the word '%s' is reserved for system use", 
    	pfTokenAsString(tok));
    }
return parseFunction(pfc, parent, pTokList, scope, pptOperatorDec);
}


static struct pfParse *parsePolymorphic(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse flow (...) [into (...)] */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptPolymorphic, tok, parent, scope);
if (pfParseEnclosingClass(parent) == NULL)
    errAt(tok, "polymorphic keyword outside of class");
tok = tok->next;
switch (tok->type)
    {
    case pftTo:
    case pftPara:
    case pftFlow:
        pp->children = pfParseStatement(pfc, pp, &tok, scope);
	break;
    default:
        expectingGot("Expecting function declaration after polymorphic", tok);
	break;
    }
*pTokList = tok;
return pp;
}



static void checkForCircularity(struct pfBaseType *parent, struct pfBaseType *newBase,
	struct pfToken *tok)
/* Make sure that newBase is not parent, grandparent, etc of itself. */
{
while (parent != NULL)
    {
    if (parent == newBase)
        errAt(tok, "loop in inheritance tree");
    parent = parent->parent;
    }
}

static struct pfParse *parseClassOrInterface(struct pfCompile *pfc,
	struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope,
	enum pfParseType pptType)
/* Parse class declaration statement. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
struct pfParse *name, *body, *extends = NULL;
struct pfBaseType *myBase;
pp = pfParseNew(pptType, tok, parent, scope);
tok = tok->next;	/* Skip 'class' or 'interface' token */
name = parseNameUse(pp, &tok, scope);
name->type = pptTypeName;
pp->name = name->name;
myBase = pfScopeFindType(scope, name->name);
if (myBase == NULL)
    internalErr();
myBase->def = pp;
if (tok->type == pftExtends)
    {
    struct pfBaseType *parentBase;
    tok = tok->next;
    if (tok->type != pftName)
        expectingGot("name", tok);
    parentBase = pfScopeFindType(scope, tok->val.s);
    if (parentBase == NULL)
	errAt(tok, "undefined parent %s", tok->val.s);
    if (parentBase->isClass != myBase->isClass)
        errAt(tok, "can't mix classes and interfaces in extensions");
    extends = pfParseNew(pptTypeName, tok, pp, scope);
    extends->ty = pfTypeNew(parentBase);
    extends->name = tok->val.s;
    checkForCircularity(parentBase, myBase, tok);
    myBase->parent = parentBase;
    slAddHead(&pp->children, extends);
    tok = tok->next;
    }
body = parseCompound(pfc, pp, &tok, scope);
body->scope->class = myBase;
slAddHead(&pp->children, body);
slAddHead(&pp->children, name);
*pTokList = tok;
return pp;
}

static struct pfParse *parseClass(struct pfCompile *pfc,
	struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Parse class declaration statement. */
{
return parseClassOrInterface(pfc, parent, pTokList, scope, pptClass);
}

static struct pfParse *parseInterface(struct pfCompile *pfc,
	struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Parse interface declaration statement. */
{
return parseClassOrInterface(pfc, parent, pTokList, scope, pptInterface);
}



static struct pfParse *parseIf(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse if statement (which may include else) */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptIf, tok, parent, scope);
struct pfParse *conditional, *trueBody, *falseBody = NULL;
tok = tok->next;	/* Skip 'if' */

/* Get conditional. */
if (tok->type != '(')
    expectingGot("(", tok);
tok = tok->next;
conditional = pfParseExpression(pfc, pp, &tok, scope);
skipRequiredCharType(')', &tok);

/* Body */
trueBody = pfParseStatement(pfc, pp, &tok, scope);

/* Else clause. */
if (tok->type == pftElse)
    {
    tok = tok->next;
    falseBody = pfParseStatement(pfc, pp, &tok, scope);
    }
*pTokList = tok;

pp->children = conditional;
conditional->next = trueBody;
trueBody->next = falseBody;
return pp;
}

static struct pfParse *parseCase(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Process case statement, which is of form:
 *    case (exp)
 *       {
 *       tuple : statement
 *       tuple : statement
 *       else : statement 
 *       }
 * The output parse tree is
 *    pptCase
 *       <expression>
 *       pptCaseList
 *          pptCaseItem
 *             <tuple>
 *             <statement>
 *          pptCaseItem
 *             <tuple>
 *             <statement>
 *          pptCaseElse
 *             <statement>
 */
{
struct pfToken *tok = *pTokList;
struct pfParse *casePp = pfParseNew(pptCase, tok, parent, scope);
struct pfParse *expression;
struct pfParse *list;
boolean gotElse = FALSE;

tok = tok->next;
skipRequiredCharType('(', &tok);
expression = pfParseExpression(pfc, casePp, &tok, scope);
if (expression->type == pptTuple)
    errAt(expression->tok, "Only single values allowed inside case expression.");
skipRequiredCharType(')', &tok);
list = pfParseNew(pptCaseList, tok, casePp, scope);
skipRequiredCharType('{', &tok);
for (;;)
    {
    if (tok->type == '}')
        break;
    else if (tok->type == pftElse)
        {
	struct pfParse *elsePp = pfParseNew(pptCaseElse, tok, list, scope);
	struct pfParse *statement;
	if (gotElse)
	    errAt(tok, "Multiple elses inside of case.");
	gotElse = TRUE;
	tok = tok->next;
	skipRequiredCharType(':', &tok);
	statement = pfParseStatement(pfc, elsePp, &tok, scope);
	elsePp->children = statement;
	slAddHead(&list->children, elsePp);
	}
    else
        {
	struct pfParse *item;
	struct pfParse *tuple;
	struct pfParse *statement;
	if (gotElse)
	    errAt(tok, "Additional cases following else not allowed.");
	item = pfParseNew(pptCaseItem, tok, list, scope);
	tuple = pfParseExpression(pfc, item, &tok, scope);
	if (anyVarDecs(tuple))
	    errAt(tuple->tok,
	    	"You can't declare variables to the left of : in a case.");
	if (tuple->type != pptTuple)
	    tuple = pfSingleTuple(tuple->parent, tuple->tok, tuple);
	skipRequiredCharType(':', &tok);
	statement = pfParseStatement(pfc, item, &tok, scope);
	item->children = tuple;
	tuple->next = statement;
	slAddHead(&list->children, item);
	}
    }
skipRequiredCharType('}', &tok);
slReverse(&list->children);
casePp->children = expression;
expression->next = list;
*pTokList = tok;
return casePp;
}

static struct pfParse *parseWhile(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse if statement (which may include else) */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(pptWhile, tok, parent, scope);
struct pfParse *conditional, *body;

tok = tok->next;	/* Skip 'while' */

/* Get conditional. */
if (tok->type != '(')
    expectingGot("(", tok);
tok = tok->next;
conditional = pfParseExpression(pfc, pp, &tok, scope);
if (tok->type != ')')
    expectingGot(")", tok);
tok = tok->next;

/* Body */
body = pfParseStatement(pfc, pp, &tok, scope);

*pTokList = tok;

pp->children = conditional;
conditional->next = body;
return pp;
}


static struct pfParse *parseForIn(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, struct pfScope *scope)
/* Parse for (el in collection)  statement */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
struct pfParse *element;
struct pfParse *collection;
struct pfParse *statement;

scope = pfScopeNew(pfc, scope, 1, NULL);
pp = pfParseNew(pptForeach, tok, parent, scope);
tok = tok->next;	/* Skip over 'foreach' */
skipRequiredCharType('(', &tok);
element = pfParseExpression(pfc, pp, &tok, scope);

skipRequiredWord("in",  &tok);
collection = pfParseExpression(pfc, pp, &tok, scope);
skipRequiredCharType(')', &tok);
statement = pfParseStatement(pfc, pp, &tok, scope);
slAddHead(&pp->children, statement);
slAddHead(&pp->children, element);
slAddHead(&pp->children, collection);
*pTokList = tok;
return pp;
}

static struct pfParse *parseExpressionAndSemi(struct pfCompile *pfc,
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse expression and following semicolon if any. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
if (tok->type == ';')
    pp = emptyStatement(parent, tok, scope);
else
    pp = pfParseExpression(pfc, parent, &tok, scope);
eatSemi(&tok);
*pTokList = tok;
return pp;
}


static struct pfParse *parseParaStatement(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse when it starts with 'para' - will either be a para function declaration
 * or a para invokation. */
{
struct pfToken *tok = *pTokList;	/* "para" */
tok = tok->next;
if (tok->type != '(')
    expectingGot("(", tok);
return parseParaInvoke(pfc, parent, pTokList, scope, TRUE);
}



static struct pfParse *parseForLikeC(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse for (;;) statement */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp;
struct pfParse *init;
struct pfParse *check;
struct pfParse *advance;
struct pfParse *statement;

scope = pfScopeNew(pfc, scope, 1, NULL);
pp = pfParseNew(pptFor, tok, parent, scope);
tok = tok->next;	/* Skip over 'for' */
if (tok->type != '(')
    expectingGot("(", tok);
tok = tok->next;

init = parseExpressionAndSemi(pfc, pp, &tok, scope);
check = parseExpressionAndSemi(pfc, pp, &tok, scope);

/* Parse advance statment and closing ')' */
if (tok->type == ')')
    advance = emptyStatement(pp, tok, scope);
else
    advance = pfParseExpression(pfc, pp, &tok, scope);
if (tok->type != ')')
    expectingGot(")", tok);
tok = tok->next;

/* Get body of loop. */
statement = pfParseStatement(pfc, pp, &tok, scope);

/* Hang various subparts of for statement onto children list. */
slAddHead(&pp->children, statement);
slAddHead(&pp->children, advance);
slAddHead(&pp->children, check);
slAddHead(&pp->children, init);
*pTokList = tok;
return pp;
}

static struct pfParse *parseFor(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope)
/* Parse for statement.  Figure out whether it's for (;;) or
 * for (el in array) type and go to it. */
{
struct pfToken *forTok = *pTokList;
struct pfToken *openTok = forTok->next, *closeTok;
if (openTok->type != '(')
    expectingGot("(", openTok);
closeTok = pfTokenGetClosingTok(openTok, '(', ')');
if (pfTokenFirstBetween(openTok->next, closeTok, ';'))
    return parseForLikeC(pfc, parent, pTokList, scope);
else
    return parseForIn(pfc, parent, pTokList, scope);
}

static struct pfParse *fakeVarInit(char *string, struct pfBaseType *base,
	struct pfToken *tok, struct pfParse *parent, struct pfScope *scope)
/* Create a little parse tree like so:
 *        pptVarInit <name>
 *           pptTypeName <base-type-name>
 *           pptSymName name
 */
{
struct pfParse *varInit = pfParseNew(pptVarInit, tok, parent, scope);
struct pfParse *type = pfParseNew(pptTypeName, tok, varInit, scope);
struct pfParse *name = pfParseNew(pptSymName, tok, varInit, scope);
varInit->name = name->name = string;
type->name = base->name;
varInit->children = type;
type->next = name;
return varInit;
}

static struct pfParse *parseTry(struct pfCompile *pfc, struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse try {statements} catch (exception e)  statement 
 * into:
 *      pptTry
 *         <try body>
 *         pptCatch
 *            pptVarDec 
 *               pptTypeName string
 *               pptNameUse message
 *         <catch body>
 */
{
struct pfToken *tok = *pTokList;
struct pfParse *tryPp = pfParseNew(pptTry, tok, parent, scope);
struct pfParse *tryBody, *catchPp, *exceptionPp, *catchBody;
struct pfToken *catchTok;
int level = 0;
struct pfScope *catchScope;	/* Scope for catch var. */

tok = tok->next;  /* Skip "try" */

/* We require the statement after try to be compound.
 * This is just in case some day in the future we want
 * to add something (like a time-out maybe) as a parameter
 * to try. */
if (tok->type != '{')
    expectingGot("{", tok);
tryBody = pfParseStatement(pfc, tryPp, &tok, scope);

/* Parse out the catch.   */
if (tok->type != pftCatch)
    expectingGot("catch", tok);
catchTok = tok;
catchScope = pfScopeNew(pfc, scope, 2, NULL);
catchPp = pfParseNew(pptCatch, tok, tryPp, catchScope);
tok = tok->next;

/* Get exception declaration. */
skipRequiredCharType('(', &tok);
exceptionPp = parseVarDec(pfc, catchPp, &tok, catchScope);
if (exceptionPp->type != pptVarDec)
    expectingGot("exception e", exceptionPp->tok);
exceptionPp->type = pptVarInit;
skipRequiredCharType(')', &tok);

/* And get body of catch. */
catchBody = pfParseStatement(pfc, tryPp, &tok, catchScope);

/* Arrange parse tree under try. */
tryPp->children = tryBody;
tryBody->next = catchPp;
catchPp->children = exceptionPp;
catchPp->next = catchBody;
*pTokList = tok;
return tryPp;
}


static struct pfParse *parseWordStatement(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope, enum pfParseType type)
/* Parse one word statement. */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(type, tok, parent, scope);
tok = tok->next;
eatSemi(&tok);
*pTokList = tok;
return pp;
}

static struct pfParse *parseBreak(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse break statement */
{
return parseWordStatement(parent, pTokList, scope, pptBreak);
}

static struct pfParse *parseContinue(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse continue statement */
{
return parseWordStatement(parent, pTokList, scope, pptContinue);
}
    	
static struct pfParse *parseReturn(struct pfParse *parent,
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse return statement */
{
return parseWordStatement(parent, pTokList, scope, pptReturn);
}

static boolean isGoodSymName(char *s)
/* Return TRUE if it looks like a good symbol name - starting
 * with '_' or alphabet, and continuing with '_' or alphnum. */
{
char c = *s++;
if (c != '_' && !isalpha(c))
    return FALSE;
while ((c = *s++) != 0)
    {
    if (c != '_' && !isalnum(c))
	return FALSE;
    }
return TRUE;
}

static void checkModuleNameSimple(struct pfToken *tok, char *modName)
/* Make sure nothing but alphanumerics and _ in module name. */
{
if (!isGoodSymName(modName))
    errAt(tok, "Module names (after the path component) must follow rules for variable names");
}

static struct pfParse *parseIncludeOrImport(struct pfCompile *pfc, 
	struct pfParse *parent, struct pfToken **pTokList, 
	struct pfScope *scope, enum pfParseType ppt)
/* Parse include or import statement (depending on ppt parameter) */
{
struct pfToken *tok = *pTokList;
struct pfParse *pp = pfParseNew(ppt, tok, parent, scope);
struct pfParse *modPp;
char *pathName, *moduleName;

switch (parent->type)
    {
    case pptMainModule:
    case pptModuleRef:
    case pptModule:
         break;
    default:
        errAt(tok, "includes and imports only allowed at highest level of a module, not inside of any functions, classes, loops, etc.");
	break;
    }
tok = tok->next;	/* Have covered 'include' */
pp->children = modPp = pfParseNew(pptSymName, tok, pp, scope);
pathName = tok->val.s;
pp->name = pp->children->name = pathName;
tok = tok->next;

if (ppt == pptImport)
    {
    struct pfParse *localName;
    struct pfType *type;
    if (tok->type == pftName && sameString(tok->val.s, "as"))
        {
	tok = tok->next;
	if (tok->type != pftName)
	    errAt(tok, "Expecting symbol name after \"import as\"");
	localName = pfParseNew(pptSymName, tok, pp, scope);
	tok = tok->next;
	localName->name = localName->tok->val.s;
	}
    else
        {
	struct pfToken *pathTok = pp->children->tok;
	moduleName = strrchr(pathName, '/');
	if (moduleName == NULL)
	    moduleName = pathName;
	else
	    moduleName += 1;
	if (!isGoodSymName(moduleName))
	    errAt(tok, 
	    	"Module names (after the path component) must follow rules "
		"for variable names unless you include an 'as'.");
	checkModuleNameSimple(pathTok, moduleName);
	localName = pfParseNew(pptSymName, pathTok, pp, scope);
	localName->name = moduleName;
	}
    slAddTail(&pp->children, localName);
    type = pfTypeNew(pfc->moduleType);
    type->tyty = tytyModule;
    pp->var = pfScopeAddVar(pp->scope, localName->name, type, pp);
    }

*pTokList = tok;
return pp;
}

static void preventNakedUse(struct pfParse *pp)
/* Prevent statement that is nothing but a varUse. */
/* Make sure that expression used as statement has a side effect. */
{
switch(pp->type)
    {
    case pptNameUse:
        errAt(pp->tok, "Statement with no effect.");
	break;
    }
}

struct pfParse *pfParseStatement(struct pfCompile *pfc, struct pfParse *parent, 
	struct pfToken **pTokList, struct pfScope *scope)
/* Parse a single statement up to but not including semicolon,
 * and add results to (head) of
 * parent->childList. */
{
struct pfToken *tok = *pTokList;
struct pfParse *statement;

switch (tok->type)
    {
    case pftInclude:
        statement = parseIncludeOrImport(pfc, parent, &tok, scope, pptInclude);
	break;
    case pftImport:
        statement = parseIncludeOrImport(pfc, parent, &tok, scope, pptImport);
	break;
    case ';':
	statement = emptyStatement(parent, tok, scope);
	tok = tok->next;
	break;
    case '{':
	statement = parseCompound(pfc, parent, &tok, scope);
	break;
    case pftIf:
        statement = parseIf(pfc, parent, &tok, scope);
	break;
    case pftCase:
        statement = parseCase(pfc, parent, &tok, scope);
	break;
    case pftWhile:
        statement = parseWhile(pfc, parent, &tok, scope);
	break;
    case pftFor:
	statement = parseFor(pfc, parent, &tok, scope);
	break;
    case pftBreak:
    	statement = parseBreak(parent, &tok, scope);
	break;
    case pftContinue:
        statement = parseContinue(parent, &tok, scope);
        break;
    case pftReturn:
    	statement = parseReturn(parent, &tok, scope);
	break;
    case pftClass:
	statement = parseClass(pfc, parent, &tok, scope);
	break;
    case pftInterface:
        statement = parseInterface(pfc, parent, &tok, scope);
	break;
    case pftTo:
	statement = parseTo(pfc, parent, &tok, scope);
	break;
    case pftOperator:
        statement = parseOperator(pfc, parent, &tok, scope);
	break;
    case pftPara:
	statement = parseParaStatement(pfc, parent, &tok, scope);
	break;
    case pftFlow:
	statement = parseFlow(pfc, parent, &tok, scope);
	break;
    case pftPolymorphic:
    	statement = parsePolymorphic(pfc, parent, &tok, scope);
	break;
    case pftTry:
        statement = parseTry(pfc, parent, &tok, scope);
	break;
    case pftLocal:
    case pftGlobal:
        {
	enum pfTokType thisType = tok->type;
	enum pfTokType nextType = tok->next->type;
	enum pfAccessType access = (thisType == pftGlobal ? paGlobal : paLocal);
	if (nextType == pftTo || nextType == pftFlow || nextType == pftOperator)
	    {
	    tok = tok->next;
	    statement = pfParseStatement(pfc, parent, &tok, scope);
	    statement->access = access;
	    }
	else if (nextType == pftClass || nextType == pftInterface)
	    {
	    struct pfParse *classPp;
	    struct pfBaseType *base;
	    tok = tok->next;
	    statement = classPp = pfParseStatement(pfc, parent, &tok, scope);
	    base = pfScopeFindType(scope, classPp->name);
	    base->access = access;
	    }
	else
	    {
	    statement = pfParseExpression(pfc, parent, &tok, scope);
	    eatSemi(&tok);
	    }
	break;
	}
    case pftConst:
    case pftStatic:
    case pftReadable:
    case pftWritable:
    case pftPlusPlus:
    case pftMinusMinus:
    case '(':
	statement = pfParseExpression(pfc, parent, &tok, scope);
	eatSemi(&tok);
	break;
    case pftName:
	statement = pfParseExpression(pfc, parent, &tok, scope);
	eatSemi(&tok);
	preventNakedUse(statement);
	break;
    case pftPrivate:
    case pftProtected:
        errAt(tok, "This word is reserved for future expansion.");
	statement = NULL;
	break;
    default:
        expectingGot("statement", tok);
	statement = NULL;
	break;
    }
*pTokList = tok;
return statement;
}


static struct pfParse *pfParseTokens(struct pfCompile *pfc,
	struct pfToken *tokList, struct pfScope *scope,
	struct pfParse *program)
/* Convert token list to parsed program. */
{
struct pfToken *tok = tokList;

while (tok->type != pftEnd)
    {
    struct pfParse *statement = pfParseStatement(pfc, program, &tok, scope);
    slAddHead(&program->children, statement);
    }
slReverse(&program->children);
return program;
}


struct pfParse *pfParseModule(struct pfCompile *pfc, struct pfModule *module,
	struct pfParse *parent, struct pfScope *scope, enum pfParseType modType)
/* Parse a module and return parse tree associated with it. */
{
struct pfParse *modParse = pfParseNew(modType, NULL, parent, scope);
pfParseTokens(pfc, module->tokList, scope, modParse);
if (modType == pptMainModule) {FILE *f = mustOpen("out.preParse", "w"); pfParseDump(modParse, 0, f); carefulClose(&f);}
module->pp = modParse;
return modParse;
}

