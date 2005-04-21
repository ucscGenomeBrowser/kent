/* pfCheck - Stuff to check that flow and para statements
 * are good in the sense that there are no inappropriate writes
 * to variables. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "pfType.h"
#include "pfCheck.h"


static boolean enclosedScope(struct pfScope *outer, struct pfScope *inner)
/* Return true if inner is the same or is inside of outer. */
{
for ( ; inner != NULL; inner = inner->next)
    if (inner == outer)
       return TRUE;
return FALSE;
}

static void checkLocal(struct hash *outsideOk, struct pfScope *localScope,
	struct pfParse *pp)
/* Make sure that any pptVarUse's are local or function calls */
{
switch (pp->type)
    {
    case pptVarUse:
	{
	if (!enclosedScope(localScope, pp->var->scope))
	    {
	    if (!hashLookup(outsideOk, pp->name))
	        errAt(pp->tok, "write to non-local variable illegal in this context");
	    }
        break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    checkLocal(outsideOk, localScope, pp);
}

static void checkReadOnlyOutsideLocals(struct hash *outsideWriteOk,
	struct pfScope *localScope, struct pfParse *pp)
/* Check that anything on left hand side of an assignment
 * is local or in the outsideWriteOk hash. */
{
switch (pp->type)
    {
    case pptAssignment:
    case pptPlusEquals:
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
         {
	 checkLocal(outsideWriteOk, localScope, pp->children);
	 break;
	 }
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    checkReadOnlyOutsideLocals(outsideWriteOk, localScope, pp);
}

static void checkPara(struct pfCompile *pfc, struct pfParse *paraDec)
/* Make sure that paraDec does not write to anything but
 *   1) It's output.
 *   2) Local variables.
 *   3) It's first input parameter. */
{
struct pfParse *input = paraDec->children->next;
struct pfParse *output = input->next;
struct pfParse *body = output->next;
struct pfParse *pp;
struct hash *outerWriteOk = hashNew(6);

/* Check that have at least one input parameter. */
if ((pp = input->children) == NULL)
    errAt(input->tok, "Para declarations need at least one parameter");

/* Build up hash of variables that it's ok to write to. */
hashAdd(outerWriteOk, pp->name, NULL);
for (pp = output->children; pp != NULL; pp = pp->next)
    hashAdd(outerWriteOk, pp->name, NULL);

checkReadOnlyOutsideLocals(outerWriteOk, body->scope, body);

/* Clean up */
hashFree(&outerWriteOk);
}

void pfCheckParaFlow(struct pfCompile *pfc, struct pfParse *pp)
/* Check para and flow declarations throughout program. */
{
switch (pp->type)
    {
    case pptParaDec:
	checkPara(pfc, pp);
        break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    pfCheckParaFlow(pfc, pp);
}

static void checkNotInAncestor(struct pfBaseType *class,
	struct pfBaseType *parent, struct pfType *method)
/* Make sure method is not declared in ancestors. */
{
while (parent != NULL && parent->name[0] != '<')
    {
    struct pfType *m;
    for (m = parent->methods; m != NULL; m = m->next)
        {
	if (sameString(m->fieldName, method->fieldName))
	    {
	    errAbort("%s defined in class %s and ancestor %s, but %s isn't polymorphic",
	    	method->fieldName, class->name, parent->name, method->fieldName);
	    }
	}
    parent = parent->parent;
    }
}

static void checkPolyAndSameType(struct pfBaseType *class,
	struct pfBaseType *parent, struct pfType *method)
/* Make sure that method is polymorphic in ancestors, and
 * that it also agrees in type in ancestors. */
{
while (parent != NULL && parent->name[0] != '<')
    {
    struct pfType *m;
    for (m = parent->methods; m != NULL; m = m->next)
        {
	if (sameString(m->fieldName, method->fieldName))
	    {
	    if (m->tyty != tytyVirtualFunction)
	        {
		errAbort("%s is polymorphic in %s, but not in ancestor %s",
			method->fieldName, class->name, parent->name);
		}
	    if (!pfTypeSame(m, method))
	        {
		errAbort("%s is defined differently in %s and ancestor %s",
			method->fieldName, class->name, parent->name);
		}
	    return; /* It suffices to check nearest ancestor defining this */	
	    }
	}
    parent = parent->parent;
    }
}

static void checkPolymorphicMatch(struct pfBaseType *class)
/* Make sure that methods agree between self and ancestors
 * on polymorphism. */
{
struct pfType *method;
struct pfBaseType *parent = class->parent;
if (parent != NULL && parent->name[0] != '<')
    {
    for (method = class->methods; method != NULL; method = method->next)
	{
	if (method->tyty == tytyVirtualFunction)
	    {
	    uglyf("Checking virtual %s.%s\n", class->name, method->fieldName);
	    checkPolyAndSameType(class, parent, method);
	    }
	else
	    {
	    uglyf("Checking method %s.%s\n", class->name, method->fieldName);
	    checkNotInAncestor(class, parent, method);
	    }
	}
    }
}

void pfCheckScopes(struct pfCompile *pfc, struct pfScope *scopeList)
/* Check scopes - currently mostly for polymorphism consistency */
{
uglyf("Checking %d scopes\n", slCount(scopeList));
struct pfScope *scope;
for (scope = scopeList; scope != NULL; scope = scope->next)
    {
    struct pfBaseType *class = scope->class;
    if (class != NULL)
        {
	checkPolymorphicMatch(class);
	}
    }
}
