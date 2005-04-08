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

