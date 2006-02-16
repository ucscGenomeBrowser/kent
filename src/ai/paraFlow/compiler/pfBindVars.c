/* pfBindVars - bind variables into parse tree. Fill in types of
 * nodes containing variables. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "pfType.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "pfBindVars.h"

static void evalFunctionType(struct pfParse *pp, struct pfBaseType *base,
	enum pfTyty tyty)
/* Fill in type info on to/para/flow node. */
{
struct pfParse *name = pp->children;
struct pfParse *input = name->next;
struct pfParse *output = input->next;
struct pfType *ty = pfTypeNew(base);
ty->tyty = tyty;
pp->ty = ty;
ty->children = input->ty;
if (output->ty == NULL)
    errAt(output->tok, "Expecting type before %s", pp->name);
ty->children->next = output->ty;
}

static void evalFunctionPtType(struct pfCompile *pfc, struct pfParse *pp,
	struct pfBaseType *base)
/* Fill in pp->ty for a function pointer node */
{
struct pfParse *input = pp->children;
struct pfParse *output = input->next;
struct pfType *ty = pfTypeNew(base);
ty->tyty = tytyFunctionPointer;
pp->ty = ty;
ty->children = input->ty;
ty->children->next = output->ty;
}

static void evalDeclarationTypes(struct pfCompile *pfc, struct pfParse *pp)
/* Go through and fill in pp->ty field on type expressions related
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
    case pptFormalParameter:
	{
	struct pfParse *type = pp->children;
	struct pfParse *name = type->next;
	struct pfParse *init = name->next;
	pp->ty = CloneVar(type->ty);
	pp->ty->init = init;
	break;
	}
    case pptToDec:
        evalFunctionType(pp, pfc->toType, tytyFunction);
	break;
    case pptFlowDec:
        evalFunctionType(pp, pfc->flowType, tytyFunction);
	break;
    case pptOperatorDec:
        evalFunctionType(pp, pfc->operatorType, tytyOperator);
	break;
    case pptTypeToPt:
	evalFunctionPtType(pfc, pp, pfc->toPtType);
	break;
    case pptTypeFlowPt:
	evalFunctionPtType(pfc, pp, pfc->flowPtType);
	break;
    case pptForeach:
    case pptParaDo:
    case pptParaAdd:
    case pptParaMultiply:
    case pptParaAnd:
    case pptParaOr:
    case pptParaMin:
    case pptParaMax:
    case pptParaArgMin:
    case pptParaArgMax:
    case pptParaGet:
    case pptParaFilter:
        {
	/* Allow users to skip the type declaration of the
	 * element in a (el in collection) phrase. */
	struct pfParse *collection = pp->children;
	struct pfParse *element = collection->next;
	if (element->type == pptNameUse)
	    {
	    struct pfType *dummyType = pfTypeNew(pfc->nilType);
	    element->type = pptUntypedElInCollection;
	    element->var = pfScopeAddVar(element->scope, element->name, 
	    	dummyType, element);
	    }
	break;
	}
    case pptOf:
        {
	/* The symbols 'array of dir of string' get converted
	 * into a 'pptOf' parse array with children array,dir,string.
	 * We convert this into a type heirachy with array at top,
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
    case pptInclude:
        {
	pp->ty = pfTypeNew(pfc->moduleType);
	pp->ty->tyty = tytyModule;
	break;
	}
    }
}

static void checkNoOutput(struct pfParse *pp)
/* The pp parse tree for the method looks like so:
 *      pptFlowDec (or pptToDec)
 *         pptSymName
 *         pptTuple (input parameters)
 *            ...
 *         pptTuple (output parameters)
 *            ...
 * This function makes sure that the output parameters are empty. */
{
struct pfParse *name = pp->children;
struct pfParse *input = name->next;
struct pfParse *output = input->next;
if (output->children != NULL)
    errAt(output->tok, "create method can't have any output.");
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
	pp->ty->isStatic = pp->isStatic;
	break;
	}
    case pptToDec:
    case pptFlowDec:
    case pptOperatorDec:
	{
	struct pfParse *name = pp->children;
	struct pfParse *input = name->next;
	struct pfParse *output = input->next;
	struct pfParse *body = output->next;
	struct pfParse *class = pfParseEnclosingClass(pp->parent);
	name->type = pptSymName;
	if (hashLookup(pp->scope->parent->vars, name->name))
	    errAt(pp->tok, "%s redefined", name->name);
	pp->var = pfScopeAddVar(pp->scope->parent, name->name, pp->ty, pp);
	if (class != NULL)
	    {
	    struct pfType *classType = class->children->ty;
	    pfScopeAddVar(pp->scope, "self", classType, class);
	    if (sameString(name->name, "init"))
	        {
		struct pfBaseType *classBase = classType->base;
		classBase->initMethod = pp;
		checkNoOutput(pp);
		}
	    }
	break;
	}
    case pptInclude:
        {
	struct pfParse *name = pp->children;
	name->type = pptSymName;
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
	pp->children->next->type = pptFieldUse;
	break;
	}
    case pptNameUse:
	{
	struct pfVar *var = pfScopeFindVar(pp->scope, pp->name);
	if (var != NULL)
	    {
	    pp->var = var;
	    pp->type = pptVarUse;
	    pp->ty = var->ty;
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
