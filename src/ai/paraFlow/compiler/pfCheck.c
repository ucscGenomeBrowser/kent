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
for ( ; inner != NULL; inner = inner->parent)
    if (inner == outer)
       return TRUE;
return FALSE;
}

static boolean isOutsideObj(struct hash *outputVars, struct pfScope *localScope,
	struct pfParse *pp)
/* Return TRUE if pp (which must be of type pptVarUse) is outside of local
 * scope, and also outside the outputVars hash (which typically is used for
 * a function's return values. */
{
return !(enclosedScope(localScope, pp->var->scope) || hashLookup(outputVars, pp->name));
}

static void checkLocal(struct hash *outputVars, struct pfScope *localScope,
	struct pfParse *pp)
/* Make sure that any pptVarUse's are local or function calls */
{
switch (pp->type)
    {
    case pptVarUse:
	{
	if (pp->var->paraTainted)
	    errAt(pp->tok, "%s may contain a non-local value, so you can no longer write to it here",
	    	pp->name); 
	if (isOutsideObj(outputVars, localScope, pp))
	    errAt(pp->tok, "write to non-local variable illegal in this context");
        break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    checkLocal(outputVars, localScope, pp);
}

struct pfParse *firstVarInTree(struct pfParse *pp)
/* Return first variable use in parse tree */
{
struct pfParse *v;
if (pp->type == pptVarUse)
    return pp;
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    v = firstVarInTree(pp);
    if (v != NULL)
        return v;
    }
return NULL;
}


static boolean usesOutsideObj(struct hash *outputVars,
	struct pfScope *localScope, struct pfParse *pp)
/* Return TRUE if any non-local objects are involved under pp. */
{
switch (pp->type)
    {
    case pptVarUse:
        return isOutsideObj(outputVars, localScope, pp);
    case pptDot:
    case pptIndex:
        return usesOutsideObj(outputVars, localScope, pp->children);
    case pptTuple:
        {
	for (pp = pp->children; pp != NULL; pp = pp->next)
	    {
	    if (usesOutsideObj(outputVars, localScope, pp))
	        return TRUE;
	    }
	return FALSE;
	}
    }
return FALSE;
}

static void markParaTainted(struct hash *outputVars,
	struct pfScope *localScope, struct pfParse *pp)
/* If we're storing a non-local object or string inside a
 * a local object or a string var, then mark that local variable
 * as tainted so we no longer allow writes to it. */
{
if (pp->ty->base->needsCleanup)
    {
    if (pp->type == pptVarInit)
        {
	struct pfParse *type = pp->children;
	struct pfParse *name = type->next;
	struct pfParse *init = name->next;
	if (init != NULL)
	    {
	    if (usesOutsideObj(outputVars, localScope, init))
	        pp->var->paraTainted = TRUE;
	    }
	}
    else 
	{
	struct pfParse *left = pp->children;
	struct pfParse *init = left->next;
	struct pfParse *leftVar = firstVarInTree(left);
	if (hashLookup(outputVars, leftVar->name))
	    {
	    if (usesOutsideObj(outputVars, localScope, init))
	        errAt(pp->tok, "Can't assign a para or flow output to something non-local.");
	    }
	else if (!isOutsideObj(outputVars, localScope, leftVar))
	    {
	    if (usesOutsideObj(outputVars, localScope, init))
	        leftVar->var->paraTainted = TRUE;
	    }
	}
    }
}

static void checkReadOnlyOutsideLocals(struct hash *outputVars,
	struct pfScope *localScope, struct pfParse *pp)
/* Check that anything on left hand side of an assignment
 * is local or in the outputVars hash. */
{
switch (pp->type)
    {
    case pptAssignment:
    case pptPlusEquals:
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
         {
	 checkLocal(outputVars, localScope, pp->children);
	 markParaTainted(outputVars, localScope, pp);
	 break;
	 }
    case pptVarInit:
        {
	markParaTainted(outputVars, localScope, pp);
	break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    checkReadOnlyOutsideLocals(outputVars, localScope, pp);
}

static void checkCalls(struct pfCompile *pfc, struct pfParse *pp, boolean paraOk)
/* Check that all calls are of type flow, or optionally
 * of type flow or type para. */
{
switch (pp->type)
    {
    case pptCall:
         {
	 struct pfParse *ppFuncVar = pp->children;
	 struct pfBaseType *base = ppFuncVar->ty->base;
	 if (base == pfc->flowType)
	     ;
	 else if (paraOk && base == pfc->paraType)
	     ;
	 else
	     {
	     if (paraOk)
	         errAt(pp->tok, 
		    "Only calls to para and flow functions allowed inside a para function");
	     else
	         errAt(pp->tok, 
		    "Only calls to other flow functions allowed inside a flow function");
	     }
	 break;
	 }
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    checkCalls(pfc, pp, paraOk);
}

static void checkPara(struct pfCompile *pfc, struct pfParse *paraDec)
/* Make sure that paraDec does not write to anything but
 *   1) It's output.
 *   2) Local variables.
 */
{
struct pfParse *input = paraDec->children->next;
struct pfParse *output = input->next;
struct pfParse *body = output->next;
if (body != NULL)
    {
    struct pfParse *pp;
    struct hash *outputVars = hashNew(6);

    /* Build up hash of variables that it's ok to write to. */
    for (pp = output->children; pp != NULL; pp = pp->next)
	hashAdd(outputVars, pp->name, pp->var);

    checkCalls(pfc, body, TRUE);
    checkReadOnlyOutsideLocals(outputVars, body->scope, body);

    /* Clean up */
    hashFree(&outputVars);
    }
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
	    checkPolyAndSameType(class, parent, method);
	    }
	else
	    {
	    checkNotInAncestor(class, parent, method);
	    }
	}
    }
}

static void rMakePolyFunList(struct pfBaseType *base,
	struct pfPolyFunRef **pList, struct hash *hash,
	int *pOffset)
/* Recursively add polymorphic functions in parents and self to list. */
{
if (base != NULL)
    {
    struct pfType *method;
    rMakePolyFunList(base->parent, pList, hash, pOffset);
    for (method = base->methods; method != NULL; method = method->next)
        {
	if (method->tyty == tytyVirtualFunction)
	    {
	    struct pfPolyFunRef *ref = hashFindVal(hash, method->fieldName);
	    if (ref == NULL)
		{
		AllocVar(ref);
		ref->method = method;
		method->polyOffset = *pOffset;
		*pOffset += 1;
		slAddHead(pList, ref);
		hashAdd(hash, method->fieldName, ref);
		}
	    else
	        {
		method->polyOffset = ref->method->polyOffset;
		ref->method = method;
		}
	    ref->class = base;
	    }
	}
    }
}

static void calcPolyFunOffsets(struct pfCompile *pfc, struct pfBaseType *base)
/* Calculate offsets into virtual function table. */
{
int polyCount = 0;
struct pfBaseType *b;
for (b = base; b != NULL; b = b->parent)
    polyCount += b->selfPolyCount;
if (polyCount > 0)
    {
    int offset = 0;
    struct pfPolyFunRef *pfrList = NULL, *pfr;
    struct hash *hash = newHash(8);
    rMakePolyFunList(base, &pfrList, hash, &offset);
    slReverse(&pfrList);
    base->polyList = pfrList;
    hashFree(&hash);
    }
}

#ifdef NEVER
static void calcAllPolyFunOffsets(struct pfCompile *pfc, 
	struct pfScope *scopeList)
/* Calculate info on all polymorphic functions 
 * FIXME - move this from code generator to pfCheck maybe? */
{
struct pfScope *scope;
for (scope = scopeList; scope != NULL; scope = scope->next)
    {
    struct pfBaseType *class = scope->class;
    if (class != NULL)
        {
	calcPolyFunOffsets(pfc, class);
	}
    }
}
#endif /* NEVER */


void pfCheckScopes(struct pfCompile *pfc, struct pfScope *scopeList)
/* Check scopes - currently mostly for polymorphism consistency */
{
struct pfScope *scope;
for (scope = scopeList; scope != NULL; scope = scope->next)
    {
    struct pfBaseType *class = scope->class;
    if (class != NULL)
        {
	checkPolymorphicMatch(class);
	calcPolyFunOffsets(pfc, class);
	}
    }
}
