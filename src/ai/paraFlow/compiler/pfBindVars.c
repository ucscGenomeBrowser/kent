/* pfBindVars - bind variables into parse tree. */

#include "common.h"
#include "hash.h"
#include "pfType.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "pfBindVars.h"

static void evalFunctionType(struct pfParse *pp, struct pfBaseType *base)
/* Fill in type info on to/para/flow node. */
{
struct pfParse *name = pp->children;
struct pfParse *input = name->next;
struct pfParse *output = input->next;
struct pfType *ty = pfTypeNew(base);
ty->isFunction = TRUE;
pp->ty = ty;
ty->children = input->ty;
ty->children->next = output->ty;
}

static void evalDeclarationTypes(struct pfCompile *pfc, struct pfParse *pp)
/* Go through and fill in pp->ct field on type expressions related
 * to function and variable declarations. */
{
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    evalDeclarationTypes(pfc, p);
switch (pp->type)
    {
    case pptVarDec:
    	internalErr();
	break;
    case pptVarInit:
	{
	struct pfParse *type = pp->children;
	pp->ty = type->ty;
	break;
	}
    case pptToDec:
        evalFunctionType(pp, pfc->toType);
	break;
    case pptParaDec:
        evalFunctionType(pp, pfc->paraType);
	break;
    case pptFlowDec:
        evalFunctionType(pp, pfc->flowType);
	break;
    case pptOf:
        {
	/* The symbols 'tree of dir of string' get converted
	 * into a 'pptOf' parse tree with children tree,dir,string.
	 * We convert this into a type heirachy with tree at top,
	 * then dir, then string.  To do this we mix child and 
	 * next pointers in a somewhat unholy fashion. */
	struct pfParse *type = pp->children;
	pp->ty = type->ty;
	while (type != NULL)
	     {
	     struct pfParse *childType = type->next;
	     if (childType != NULL)
	          type->ty->children = childType->ty;
	     type = childType;
	     }
	break;
	}
    case pptTypeName:
        {
	struct pfBaseType *base = pfScopeFindType(pp->scope, pp->name);
	if (base != NULL)
	    {
	    struct pfType *ty = pfTypeNew(base);
	    pp->ty = ty;
	    }
	break;
	}
    case pptTuple:
        {
	pp->ty = pfTypeNew(NULL);
	pp->ty->isTuple = TRUE;
	if (pp->children != NULL)
	    {
	    pp->ty->children = pp->children->ty;
	    pp = pp->children;
	    while (pp->next != NULL)
	        {
		if (pp->ty == NULL)
		    errAt(pp->tok, "void value in tuple");
		pp->ty->next = pp->next->ty;
		pp = pp->next;
		}
	    }
	break;
	}
    case pptInto:
        {
	pp->ty = pfTypeNew(NULL);
	pp->ty->isModule = TRUE;
	break;
	}
    }
}


static void addDeclaredVarsToScopes(struct pfParse *pp)
/* Go through and put declared variables into symbol table
 * for scope. */
{
switch (pp->type)
    {
    case pptVarDec:
    	internalErr();
	break;
    case pptVarInit:
	{
	struct pfParse *type = pp->children;
	struct pfParse *name = type->next;
	if (hashLookup(pp->scope->vars, name->name))
	    errAt(pp->tok, "%s redefined", name->name);
	pfScopeAddVar(pp->scope, name->name, pp->ty);
	break;
	}
    case pptToDec:
    case pptParaDec:
    case pptFlowDec:
	{
	struct pfParse *name = pp->children;
	struct pfParse *input = name->next;
	struct pfParse *output = input->next;
	struct pfParse *body = output->next;
	name->type = pptSymName;
	assert(input->type == pptTuple);
	assert(output->type == pptTuple);
	pfParseTypeSub(input, pptTuple, pptTypeTuple);
	pfParseTypeSub(output, pptTuple, pptTypeTuple);
	if (hashLookup(pp->scope->parent->vars, name->name))
	    errAt(pp->tok, "%s redefined", name->name);
	pfScopeAddVar(pp->scope->parent, name->name, pp->ty);
	break;
	}
    case pptInto:
        {
	struct pfParse *name = pp->children;
	name->type = pptSymName;
	if (hashLookup(pp->scope->vars, name->name))
	    errAt(pp->tok, "%s redefined", name->name);
	pfScopeAddVar(pp->scope, name->name, pp->ty);
	break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    addDeclaredVarsToScopes(pp);
}

static void bindVars(struct pfParse *pp)
/* Attach variable name to pfVar in appropriate scope.
 * Complain and die if not found. */
{
struct pfParse *p;

for (p = pp->children; p != NULL; p = p->next)
    bindVars(p);
switch (pp->type)
    {
    case pptNameUse:
	{
	struct pfVar *var = pfScopeFindVar(pp->scope, pp->name);
	if (var == NULL)
	    errAt(pp->tok, "Use of undefined variable %s", pp->name);
	pp->var = var;
	pp->type = pptVarUse;
	pp->ty = var->ty;
        break;
	}
    }
}


void pfBindVars(struct pfCompile *pfc, struct pfParse *pp)
/* pfBindVars - massage parse tree slightly to regularize
 * variable and function declarations.  Then put all declarations
 * in the appropriate scope, and bind variable uses and function
 * calls to the appropriate declaration.  This will complain
 * about undefined symbols. */
{
evalDeclarationTypes(pfc, pp);
addDeclaredVarsToScopes(pp);
bindVars(pp);
}
