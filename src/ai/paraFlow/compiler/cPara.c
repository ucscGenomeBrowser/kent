#include "common.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "refCount.h"
#include "cCoder.h"

static boolean varUsed(struct pfVar *var, struct pfParse *pp)
/* Return TRUE if parse subtree uses var. */
{
switch (pp->type)
    {
    case pptVarUse:
	if (pp->var == var)
	    return TRUE;
	break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    if (varUsed(var, pp))
        return TRUE;
    }
return FALSE;
}

static boolean varWritten(struct pfVar *var, struct pfParse *pp)
/* Return TRUE if parse subtree  writes to var. */
{
switch (pp->type)
    {
    case pptAssignment:
    case pptPlusEquals:
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
        if (varUsed(var, pp->children))
	    return TRUE;
	break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    if (varWritten(var, pp))
        return TRUE;
    }
return FALSE;
}

static void saveBackToCollection(struct pfCompile *pfc, FILE *f,
	int stack, struct pfParse *elPp, struct pfParse *collectionPp)
/* Save altered value of element back to collection. */
{
struct pfBaseType *base = collectionPp->ty->base;
struct dyString *elName = codeVarName(pfc, elPp->var);
if (elPp->ty->base->needsCleanup)
    codeVarIncRef(f, elName->string);
if (base == pfc->arrayType)
    {
    fprintf(f, "*((");
    codeBaseType(pfc, f, elPp->ty->base);
    fprintf(f, "*)(_pf_collection->elements + _pf_offset)) = %s;\n",
    	elName->string);
    }
else if (base == pfc->stringType)
    {
    internalErr();	/* Type check module should prevent this */
    }
else if (base == pfc->dyStringType)
    {
    fprintf(f, "_pf_collection->s[_pf_offset] = %s;\n", elName->string);
    }
else if (base == pfc->dirType)
    {
    codeParamAccess(pfc, f, elPp->ty->base, stack);
    fprintf(f, " = %s;\n", elName->string);
    if (elPp->ty->base->needsCleanup)
	{
	fprintf(f, "_pf_dir_add_obj(_pf_collection, _pf_key, %s+%d);\n",  
		stackName, stack);
	}
    else 
	{
	fprintf(f, "_pf_dir_add_num(_pf_collection, _pf_key, %s+%d);\n",  
		stackName, stack);
	}
    }
else if (base == pfc->indexRangeType)
    {
    /* Do nothing. */
    }
else
    {
    internalErr();
    }
dyStringFree(&elName);
}

void codeParaDo(struct pfCompile *pfc, FILE *f,
	struct pfParse *foreach, boolean isPara)
/* Emit C code for para ... do statement. */
{
struct pfParse *collectionPp = foreach->children;
struct pfParse *elPp = collectionPp->next;
struct pfParse *body = elPp->next;
struct pfParse *elVarPp = NULL;

codeStartElInCollectionIteration(pfc, f, 0, 
	foreach->scope, elPp, collectionPp, isPara);
codeStatement(pfc, f, body);
if (elPp->type == pptKeyVal)
    {
    struct pfParse *keyPp = elPp->children;
    struct pfParse *valPp = keyPp->next;
    elVarPp = valPp;
    }
else if (elPp->type == pptVarInit)
    elVarPp = elPp;
else
    internalErrAt(elPp->tok);
if (varWritten(elVarPp->var, body))
    saveBackToCollection(pfc, f, 0, elVarPp, collectionPp);
codeEndElInCollectionIteration(pfc, f, foreach->scope, elPp, collectionPp, 
	isPara);
}

