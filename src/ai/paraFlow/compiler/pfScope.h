/* pfScope - handle heirarchies of symbol tables. */

#ifndef PFSCOPE_H
#define PFSCOPE_H

#ifndef PFTYPE_H
#include "pfType.h"
#endif

struct pfScope
/* The symbol tables for this scope and a pointer to the
 * parent scope. */
     {
     struct pfScope *next;	/* Next in scope list. */
     struct pfScope *parent;	/* Parent scope if any. */
     struct hash *types;	/* Types defined in this scope. */
     struct hash *vars;		/* Variables defined in this scope (including functions) */
     };

struct pfVar
/* A variable at a certain scope; */
     {
     char *name;			/* Name (not allocated here) */
     struct pfScope *scope;		/* Scope we're declared in. */
     struct pfType *ty;			/* Variable type. */
     };

struct pfTokenizer;

struct pfScope *pfScopeNew(struct pfScope *parent, int size);
/* Create new scope with given parent.  Size is just a hint
 * of what to make the size of the symbol table as a power of
 * two.  Pass in 0 if you don't care. */

struct pfBaseType *pfScopeAddType(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parentType);
/* Add new base type to scope. */

struct pfVar *pfScopeAddVar(struct pfScope *scope, char *name, struct pfType *ty);
/* Add type to scope. */

struct pfBaseType *pfScopeFindType(struct pfScope *scope, char *name);
/* Find type associated with name in scope and it's parents. */

struct pfVar *pfScopeFindVar(struct pfScope *scope, char *name);
/* Find variable associated with name in scope and it's parents. */

struct pfVar *pfScopeFindOrCreateVar(struct pfScope *scope, char *name);
/* Find variable.  If it doesn't exist create it in innermost scope. */

#endif /* PFSCOPE_H */

