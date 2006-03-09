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

static void evalDeclarationTypes(struct pfCompile *pfc, struct pfParse *pp, int level)
/* Go through and fill in pp->ty field on type expressions related
 * to function and variable declarations. */
{
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    evalDeclarationTypes(pfc, p, level+1);
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
    case pptModuleDotType:
        {
	struct pfParse *modPp = pp->children;
	struct pfParse *typePp = modPp->next;
	struct pfVar *var = pfScopeFindVar(pp->scope, modPp->name);
	char *moduleName;
	struct pfModule *module;
	struct pfBaseType *base;
	struct pfType *ty;
	if (var == NULL)
	    internalErrAt(pp->tok);
	moduleName = var->parse->children->name;
	module = hashFindVal(pfc->moduleHash, moduleName);
	if (module == NULL)
	    internalErrAt(modPp->tok);
	base = pfScopeFindType(module->scope, typePp->name);
	if (base == NULL)
	    errAt(typePp->tok, "Class %s not defined in module %s", typePp->name, moduleName);
	ty = pfTypeNew(base);
	pp->ty = typePp->ty = ty;
	modPp->type = pptModuleUse;
	typePp->type = pptTypeName;
	typePp->scope = module->scope;
	break;
	}
    case pptTypeName:
        {
	struct pfBaseType *base = pfScopeFindType(pp->scope, pp->name);
	if (base != NULL)
	    {
	    struct pfType *ty = pfTypeNew(base);
	    ty->access = pp->access;
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

static void addDeclaredVarsToScopes(struct pfCompile *pfc, struct pfParse *pp)
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
	pp->ty->access = pp->access;
	pp->var = pfScopeAddVar(pp->scope, name->name, pp->ty, pp);
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
	pp->ty->access = pp->access;
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
    addDeclaredVarsToScopes(pfc, pp);
}

static void linkInVars(struct hash *vars, struct pfScope *scope)
/* Types hash is full of pfVar.  Add these to scope. */
{
struct hashCookie it = hashFirst(vars);
struct hashEl *el;
while ((el = hashNext(&it)) != NULL)
    {
    struct pfVar *var = el->val;
    enum pfAccessType access = var->ty->access;
    if (access == paGlobal || access == paWritable || access == paReadable)
	hashAdd(scope->vars, el->name, var);
    }
}

static void addIncludedSymbols(struct pfCompile *pfc, struct pfParse *pp,
	int level)
/* Add symbols from included files. */
{
if (pp->type == pptInclude)
    {
    struct pfModule *module;
    module = hashMustFindVal(pfc->moduleHash, pp->children->name);
    linkInVars(module->scope->vars, pp->scope);
    }
level += 1;
if (level <= 2)  /* Include at pptProgram->pptModule->pptInclude, no deeper */
    {
    for (pp = pp->children; pp != NULL; pp = pp->next)
	addIncludedSymbols(pfc, pp, level);
    }
}

static void rSetVarScope(struct pfParse *pp, struct pfScope *scope)
/* Set scope for the variable to module in construct like:
 *    module.variable
 *    module.variable[index]
 *    module.variable()
 */
{
pp->scope = scope;
switch (pp->type)
    {
    case pptDot:
    case pptIndex:
    case pptCall:
        rSetVarScope(pp->children, scope);
	break;
    case pptVarUse:
        pp->scope = scope;
	break;
    }
}

static void checkVarsDefined(struct pfCompile *pfc, struct pfParse *pp,
	struct pfModule *module)
/* Attach variable name to pfVar in appropriate scope.
 * Complain and die if not found. */
{
struct pfParse *p;

switch (pp->type)
    {
    case pptModule:
    case pptModuleRef:
    case pptMainModule:
	module = pp->scope->module;
        break;
    case pptDot:
        {
	struct pfParse *leftOfDot = pp->children;
	struct pfParse *rightOfDot = leftOfDot->next;
	if (leftOfDot->type == pptNameUse)
	    {
	    char *name = leftOfDot->name;
	    struct pfVar *var = pfScopeFindVar(leftOfDot->scope, name);
	    if (var != NULL && var->ty->base == pfc->moduleType)
	        {
		char *modName = var->parse->name;
		struct pfModule *module = pfScopeFindModule(pp->scope, modName);
		if (!module)
		    internalErrAt(leftOfDot->tok);
		leftOfDot->type = pptModuleUse;
		rSetVarScope(rightOfDot, module->scope);
		}
	    else 
		{
		rightOfDot->type = pptFieldUse;
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
	    struct pfModule *varModule = var->scope->module;
	    if (varModule != NULL && varModule != module)
	        {
		enum pfAccessType access = var->ty->access;
		if (access != paGlobal && access != paWritable)
		    {
		    errAt(pp->tok, "%s is private to module %s", 
		    	var->name, varModule->name);
		    }
		}
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
    checkVarsDefined(pfc, p, module);
}


void pfBindVars(struct pfCompile *pfc, struct pfParse *pp)
/* pfBindVars - massage parse tree slightly to regularize
 * variable and function declarations.  Then put all declarations
 * in the appropriate scope, and bind variable uses and function
 * calls to the appropriate declaration.  This will complain
 * about undefined symbols. */
{
evalDeclarationTypes(pfc, pp, 0);
addDeclaredVarsToScopes(pfc, pp);
addIncludedSymbols(pfc, pp, 0);
checkVarsDefined(pfc, pp, NULL);
}
