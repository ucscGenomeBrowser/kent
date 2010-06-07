/* pfScope - handle heirarchies of symbol tables. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#ifndef PFSCOPE_H
#define PFSCOPE_H

#ifndef PFTYPE_H
#include "pfType.h"
#endif

struct pfScope
/* The symbol tables for this scope and a pointer to the
 * parent scope. */
     {
     struct pfScope *next;	/* Sibling scope if any. */
     struct pfScope *children;	/* Child scopes if any. */
     struct pfScope *parent;	/* Parent scope if any. */
     struct hash *types;	/* Types defined in this scope. */
     struct hash *vars;		/* Variables and functions defined in this scope */
     struct hash *modules;	/* Imported modules (may be nil) */
     int id;			/* Unique ID for this scope. */
     struct pfModule *module;	/* If scope is module level, this is set. */
     boolean isLocal;		/* True local in function. */
     struct pfBaseType *class;	/* If it's a class scope this is set. */
     };

struct pfVar
/* A variable at a certain scope; */
     {
     char *name;			/* Name (not allocated here) */
     char *cName;			/* Name for C output, also not allocated here. */
     struct pfScope *scope;		/* Scope we're declared in. */
     struct pfType *ty;			/* Variable type. */
     struct pfParse *parse;		/* Declaration spot in parse tree. */
     struct ctar *ctar;	/* Compile-time Activation record for functions  */
     bool paraTainted;			/* If true then no writing to var allowed. */
     bool isExternal;			/* Variable in an external file? */
     bool constFolded;			/* True if constant folded. */
     };

/* Forward declarations so compiler doesn't squawk */
struct pfTokenizer;
struct pfCompile;
struct pfModule;	


struct pfScope *pfScopeNew(struct pfCompile *pfc, 
	struct pfScope *parent, int size, struct pfModule *module);
/* Create new scope with given parent.  Size is just a hint
 * of what to make the size of the symbol table as a power of
 * two.  Pass in 0 if you don't care. */

void pfScopeDump(struct pfScope *scope, FILE *f);
/* Write out line of info about scope. */

void pfScopeAddModule(struct pfScope *scope, struct pfModule *module);
/* Add module to scope. */

struct pfBaseType *pfScopeAddType(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parentType, int size, 
	boolean needsCleanup);
/* Add new base type to scope. */

struct pfVar *pfScopeAddVar(struct pfScope *scope, char *name, 
	struct pfType *ty, struct pfParse *pp);
/* Add variable to scope. */

struct pfModule *pfScopeFindModule(struct pfScope *scope, char *name);
/* Find module in scope or parent scope. */

struct pfBaseType *pfScopeFindType(struct pfScope *scope, char *name);
/* Find type associated with name in scope and it's parents. */

struct pfVar *pfScopeFindVar(struct pfScope *scope, char *name);
/* Find variable associated with name in scope and it's parents. */

void pfScopeMarkLocal(struct pfScope *scope);
/* Mark scope, and all of it's children, as local. */

#endif /* PFSCOPE_H */

