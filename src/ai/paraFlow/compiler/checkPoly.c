/* checkPoly - check for polymorphism consistency. */
/* Copyright 2005-6 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "pfType.h"
#include "checkPoly.h"


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
	    if (!sameString(m->fieldName, "init"))
		{
		errAbort("%s defined in class %s and ancestor %s, but %s isn't polymorphic",
		    method->fieldName, class->name, parent->name, method->fieldName);
		}
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

void checkPolymorphic(struct pfCompile *pfc, struct slRef *scopeRefs)
/* Check for polymorphism consistency */
{
struct slRef *ref;
for (ref = scopeRefs; ref != NULL; ref = ref->next)
    {
    struct pfScope *scope = ref->val;
    struct pfBaseType *class = scope->class;
    if (class != NULL)
        {
	checkPolymorphicMatch(class);
	calcPolyFunOffsets(pfc, class);
	}
    }
}
