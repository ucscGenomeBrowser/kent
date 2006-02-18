/* pfScope - handle heirarchies of symbol tables. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "pfScope.h"
#include "pfType.h"
#include "pfCompile.h"


struct pfScope *pfScopeNew(struct pfCompile *pfc,
	struct pfScope *parent, int size, boolean isModule)
/* Create new scope with given parent.  Size is just a hint
 * of what to make the size of the symbol table as a power of
 * two.  Pass in 0 if you don't care. */
{
struct pfScope *scope;
int varSize = size;
int typeSize;
static int id;

if (varSize <= 0)
    varSize = 5;
typeSize = varSize - 2;
if (typeSize <= 0)
    typeSize = 3;
AllocVar(scope);
scope->types = hashNew(typeSize);
scope->vars = hashNew(varSize);
scope->parent = parent;
scope->id = ++id;
scope->isModule = isModule;
if (parent != NULL)
    {
    slAddHead(&parent->children, scope);
    }
refAdd(&pfc->scopeRefList, scope);
return scope;
}

void pfScopeDump(struct pfScope *scope, FILE *f)
/* Write out line of info about scope. */
{
if (scope->types->elCount > 0)
    {
    struct hashCookie hc = hashFirst(scope->types);
    struct pfBaseType *bt;
    fprintf(f, " types: ");
    while ((bt = hashNextVal(&hc)) != NULL)
        fprintf(f, "%s, ", bt->name);
    }
if (scope->vars->elCount > 0)
    {
    struct hashCookie hc = hashFirst(scope->vars);
    struct pfVar *var;
    fprintf(f, " vars: ");
    while ((var = hashNextVal(&hc)) != NULL)
	{
	pfTypeDump(var->ty, f);
        fprintf(f, " %s,", var->name);
	}
    }
}


struct pfBaseType *pfScopeAddType(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parentType, int size, 
	boolean needsCleanup)
/* Add new base type to scope. */
{
struct pfBaseType *bt = pfBaseTypeNew(scope, name, isCollection, 
	parentType, size, needsCleanup);
hashAdd(scope->types, name, bt);
return bt;
}

struct pfVar *pfScopeAddVar(struct pfScope *scope, char *name, 
	struct pfType *ty, struct pfParse *pp)
/* Add variable to scope. */
{
struct pfVar *var;
AllocVar(var);
var->scope = scope;
var->ty = ty;
var->parse = pp;
hashAddSaveName(scope->vars, name, var, &var->name);
var->cName = var->name;
return var;
}

struct pfBaseType *pfScopeFindType(struct pfScope *scope, char *name)
/* Find type associated with name in scope and it's parents. */
{
while (scope != NULL)
    {
    struct pfBaseType *baseType = hashFindVal(scope->types, name);
    if (baseType != NULL)
        return baseType;
    scope = scope->parent;
    }
return NULL;
}

struct pfVar *pfScopeFindVar(struct pfScope *scope, char *name)
/* Find variable associated with name in scope and it's parents. */
{
while (scope != NULL)
    {
    struct pfVar *var = hashFindVal(scope->vars, name);
    if (var != NULL)
        return var;
    scope = scope->parent;
    }
return NULL;
}

void pfScopeMarkLocal(struct pfScope *scope)
/* Mark scope, and all of it's children, as local. */
{
scope->isLocal = TRUE;
for (scope = scope->children; scope != NULL; scope = scope->next)
    pfScopeMarkLocal(scope);
}
