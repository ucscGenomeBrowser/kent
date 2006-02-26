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
	if (type->type == pptDot)
	    {
	    struct pfParse *modPp = type->children;
	    if (modPp->type != pptModuleUse)
	        internalErrAt(type->tok);
	    type = modPp->next;
	    }
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
    case pptKeyVal:
	{
	/* Here we allow either strings or naked names on the
	 * left of a pptKeyVal.  */
	struct pfParse *key = pp->children;
	if (key->type != pptNameUse && key->type != pptConstUse)
	    errAt(pp->tok, 
	    	"The key in a key : val expression must be string or name.");
	key->type = pptKeyName;
	key->tok->type = pftString;
	key->name = key->tok->val.s;
        break;
	}
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
	 * element in a (el in collection) phrase. 
	 * I wonder if we could move this to pfParse
	 * actually.  WOrks here though. */
	struct pfParse *collection = pp->children;
	struct pfParse *element = collection->next;
	if (element->type == pptNameUse)
	    {
	    }
	else if (element->type == pptKeyVal)
	    {
	    struct pfParse *keyVal = element;
	    struct pfParse *key = keyVal->children;
	    element = key->next;
	    if (element->type != pptNameUse || key->type != pptKeyName)
	         errAt(keyVal->tok, 
		 	"Expecting simple name on either side of ':'");
	    if (sameString(key->name, element->name))
	        errAt(keyVal->tok, "Key and value must have different name.");
	    key->var = pfScopeAddVar(key->scope, key->name, 
		pfTypeNew(pfc->nilType), key);
	    }
	else
	    {
	    errAt(element->tok, "Can't handle this construct before 'in.' \n"
		  "Expecting variable name with no type, or key:val.");
	    }
	element->type = pptUntypedElInCollection;
	element->var = pfScopeAddVar(element->scope, element->name, 
	    pfTypeNew(pfc->nilType), element);
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
#ifdef OLD
    case pptInclude:
        {
	pp->ty = pfTypeNew(pfc->moduleType);
	pp->ty->tyty = tytyModule;
	break;
	}
#endif /* OLD */
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
	    struct pfBaseType *classBase = classType->base;
	    pfScopeAddVar(pp->scope, "self", classType, class);
	    if (pfBaseIsDerivedClass(classBase))
	        {
		struct pfBaseType *parentBase = classBase->parent;
		pfScopeAddVar(pp->scope, "parent", pfTypeNew(parentBase), 
			class);
		}
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

static void linkInVars(struct hash *vars, struct pfScope *scope)
/* Types hash is full of pfVar.  Add these to scope. */
{
struct hashCookie it = hashFirst(vars);
struct hashEl *el;
while ((el = hashNext(&it)) != NULL)
    hashAdd(scope->vars, el->name, el->val);
}

static void addIncludedSymbols(struct pfCompile *pfc, struct pfParse *pp,
	int level)
/* Add symbols from included files. */
{
if (pp->type == pptInclude)
    {
    struct pfModule *module;
    module = hashMustFindVal(pfc->moduleHash, pp->name);
    linkInVars(module->scope->vars, pp->scope);
    }
level += 1;
if (level <= 2)  /* Include at pptProgram->pptModule->pptInclude, no deeper */
    {
    for (pp = pp->children; pp != NULL; pp = pp->next)
	addIncludedSymbols(pfc, pp, level);
    }
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
	struct pfParse *leftOfDot = pp->children;
	struct pfParse *rightOfDot = leftOfDot->next;
	if (leftOfDot->type == pptNameUse)
	    {
	    char *name = leftOfDot->name;
	    struct pfVar *var = pfScopeFindVar(leftOfDot->scope, name);
	    if (var)
		{
		rightOfDot->type = pptFieldUse;
		}
	    else 
	        {
		struct pfModule *module = pfScopeFindModule(pp->scope, name);
		// TODO - figure out whether this belongs here or in pfType.
		// I guess it has to go here but hmmm....
		if (module)
		    {
		    leftOfDot->type = pptModuleUse;
		    rightOfDot->scope = module->scope;
		    }
		}
	    }
	else
	    {
	    rightOfDot->type = pptFieldUse;
	    }
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
addIncludedSymbols(pfc, pp, 0);
checkVarsDefined(pp);
}
