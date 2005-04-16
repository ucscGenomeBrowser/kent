/* pfBindVars - bind variables into parse tree. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

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
	else
	    errAt(pp->tok, "Undefined class %s", pp->name);
	break;
	}
    case pptTypeTuple:
        {
	pfTypeOnTuple(pfc, pp);
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

static struct pfParse *enclosingClass(struct pfParse *pp)
/* Find enclosing class if any. */
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
	pp->var = pfScopeAddVar(pp->scope, name->name, pp->ty, pp);
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
	struct pfParse *class = enclosingClass(pp);
	name->type = pptSymName;
	if (hashLookup(pp->scope->parent->vars, name->name))
	    errAt(pp->tok, "%s redefined", name->name);
	pp->var = pfScopeAddVar(pp->scope->parent, name->name, pp->ty, pp);
	if (class != NULL)
	    {
	    pfScopeAddVar(pp->scope, "self", class->children->ty, class);
	    }
	break;
	}
    case pptInto:
        {
	struct pfParse *name = pp->children;
	name->type = pptSymName;
	if (hashLookup(pp->scope->vars, name->name))
	    errAt(pp->tok, "%s redefined", name->name);
	pfScopeAddVar(pp->scope, name->name, pp->ty, pp);
	break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    addDeclaredVarsToScopes(pp);
}

static void checkVarsDefined(struct pfParse *pp)
/* Attach variable name to pfVar in appropriate scope.
 * Complain and die if not found. */
{
struct pfParse *p;

switch (pp->type)
    {
    case pptDot:
        {
	for (p = pp->children->next; p != NULL; p = p->next)
	    p->type = pptFieldUse;
	break;
	}
    case pptNameUse:
	{
	struct pfVar *var = pfScopeFindVar(pp->scope, pp->name);
	if (var != NULL)
	    {
	    pp->var = var;
	    pp->type = pptVarUse;
	    pp->ty = CloneVar(var->ty);
	    }
	else
	    errAt(pp->tok, "%s used but not defined", pp->name);
        break;
	}
    }
for (p = pp->children; p != NULL; p = p->next)
    checkVarsDefined(p);
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
checkVarsDefined(pp);
}
