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
     struct pfScope *next;	/* Next in scope list. */
     struct pfScope *parent;	/* Parent scope if any. */
     struct hash *types;	/* Types defined in this scope. */
     struct hash *vars;		/* Variables defined in this scope (including functions) */
     int id;			/* Unique ID for this scope. */
     boolean isModule;		/* True if it's a module scope. */
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
     struct comTimeActRec *ctar;	/* Compile-time Activation record for functions  */
     bool isExternal;			/* Variable in an external file? */
     bool paraTainted;			/* If true then no writing to var allowed. */
     };

struct pfTokenizer;
struct pfCompile;

struct pfScope *pfScopeNew(struct pfCompile *pfc, 
	struct pfScope *parent, int size, boolean isModule);
/* Create new scope with given parent.  Size is just a hint
 * of what to make the size of the symbol table as a power of
 * two.  Pass in 0 if you don't care. */

void pfScopeDump(struct pfScope *scope, FILE *f);
/* Write out line of info about scope. */

struct pfBaseType *pfScopeAddType(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parentType, int size, 
	boolean needsCleanup);
/* Add new base type to scope. */

struct pfVar *pfScopeAddVar(struct pfScope *scope, char *name, 
	struct pfType *ty, struct pfParse *pp);
/* Add variable to scope. */

struct pfBaseType *pfScopeFindType(struct pfScope *scope, char *name);
/* Find type associated with name in scope and it's parents. */

struct pfVar *pfScopeFindVar(struct pfScope *scope, char *name);
/* Find variable associated with name in scope and it's parents. */

#endif /* PFSCOPE_H */

