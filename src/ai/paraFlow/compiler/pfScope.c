/* pfScope - handle heirarchies of symbol tables. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "pfScope.h"
#include "pfType.h"

struct pfScope *pfScopeNew(struct pfScope *parent, int size)
/* Create new scope with given parent.  Size is just a hint
 * of what to make the size of the symbol table as a power of
 * two.  Pass in 0 if you don't care. */
{
struct pfScope *scope;
int varSize = size;
int typeSize;

if (varSize <= 0)
    varSize = 5;
typeSize = varSize - 2;
if (typeSize <= 0)
    typeSize = 3;
AllocVar(scope);
scope->types = hashNew(typeSize);
scope->vars = hashNew(varSize);
scope->parent = parent;
return scope;
}

struct pfBaseType *pfScopeAddType(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parentType)
/* Add new base type to scope. */
{
struct pfBaseType *bt = pfBaseTypeNew(scope, name, isCollection, parentType);
hashAdd(scope->types, name, bt);
return bt;
}

struct pfVar *pfScopeAddVar(struct pfScope *scope, char *name, struct pfType *ty)
/* Add type to scope. */
{
struct pfVar *var;
AllocVar(var);
var->scope = scope;
var->ty = ty;
hashAddSaveName(scope->vars, name, var, &var->name);
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

struct pfVar *pfScopeFindOrCreateVar(struct pfScope *scope, char *name)
/* Find variable.  If it doesn't exist create it in innermost scope. */
{
struct pfVar *var = pfScopeFindVar(scope, name);
if (var == NULL)
    var = pfScopeAddVar(scope, name, NULL);
return var;
}

