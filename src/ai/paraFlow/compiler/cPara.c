#include "common.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "refCount.h"
#include "ctar.h"
#include "pfPreamble.h"
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
    case pptAssign:
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

#if defined(PARALLEL)
void codeParaDo(struct pfCompile *pfc, FILE *f, struct pfParse *para)
/* Emit C code for para ... do statement. */
{
struct pfParse *collectionPp = para->children;
struct pfParse *enclosingFunction = pfParseEnclosingFunction(para);
struct pfBaseType *base = collectionPp->ty->base;
if (base == pfc->indexRangeType)
    {
    struct pfParse *fromPp = collectionPp->children;
    struct pfParse *toPp = fromPp->next;
    int offset = codeExpression(pfc, f, fromPp, 0, FALSE);
    codeExpression(pfc, f, toPp, offset, FALSE);
    fprintf(f, "_pf_paraRunRange(");
    codeParamAccess(pfc, f, pfc->longType, 0);
    fprintf(f, ", ");
    codeParamAccess(pfc, f, pfc->longType, offset);
    }
else if (base == pfc->arrayType)
    {
    codeExpression(pfc, f, collectionPp, 0, FALSE);
    fprintf(f, "_pf_paraRunArray(");
    codeParamAccess(pfc, f, collectionPp->ty->base, 0);
    }
else if (base == pfc->dirType)
    {
    codeExpression(pfc, f, collectionPp, 0, FALSE);
    fprintf(f, "_pf_paraRunDir(");
    codeParamAccess(pfc, f, collectionPp->ty->base, 0);
    }
else
    internalErrAt(para->tok);
fprintf(f, ", ");
if (enclosingFunction)
    fprintf(f, "&_pf_l, sizeof(_pf_l), ");
else
    fprintf(f, "0, 0, ");
fprintf(f, "%s, ", para->name);
fprintf(f, "%d, 0);\n", prtParaDo);
}

static char *paraBlockName(int *pCounter)
/* Generate a unique-to-module name for this block. */
{
char buf[256];
safef(buf, sizeof(buf), "_pf_para%d", ++(*pCounter));
return cloneString(buf);
}

static char *figureDeref(struct pfCompile *pfc,
	struct pfParse *collectionPp, struct pfParse *elPp)
/* Figure out how many levels to dereference - a single for arrays,
 * double for dirs of objects, triple for dirs of numbers. */
{
if (collectionPp->ty->base == pfc->dirType)
    {
    struct pfBaseType *base = elPp->ty->base;
    if (base->parent == pfc->numType)
        return "***";
    else
        return "**";
    }
else
    return "*";
}

static void codeParaBlockFunction(struct pfCompile *pfc, FILE *f, 
	struct pfParse *para, struct pfParse *enclosingFunction,
	enum paraRunType pftType)
/* Code para block function */
{
struct pfParse *collectionPp = para->children;
struct pfBaseType *colBase = collectionPp->ty->base;
char *deref;
struct pfParse *elIxPp = collectionPp->next;
struct pfParse *body = elIxPp->next;
struct dyString *elName;
struct dyString *keyName = NULL;
struct pfParse *elPp, *ixPp = NULL;
struct pfBaseType *elBase;

if (elIxPp->type == pptKeyVal)
    {
    ixPp = elIxPp->children;
    elPp = ixPp->next;
    keyName = codeVarName(pfc,ixPp->var);
    }
else
    elPp = elIxPp;

deref = figureDeref(pfc, collectionPp, elPp);
elBase = elPp->ty->base;
elName = codeVarName(pfc, elPp->var);

fprintf(f, "static void %s(%s *%s, char *_pf_key, void *_pf_item, void *_pf_localVars)\n", 
	para->name, stackType, stackName);
fprintf(f, "{\n");
if (enclosingFunction != NULL)
    {
    struct ctar *ctar = enclosingFunction->var->ctar;
    ctarCodeLocalStruct(ctar, pfc, f, "_pf_localVars");
    }
else
    codeScopeVars(pfc, f, para->scope, FALSE);
fprintf(f, "%s = %s((", elName->string, deref);
codeBaseType(pfc, f, elBase);
fprintf(f, "%s)_pf_item);\n", deref);
if (keyName != NULL)
    {
    fprintf(f, "%s = ", keyName->string);
    if (ixPp->ty->base == pfc->stringType)
	fprintf(f, "_pf_string_from_const(_pf_key);\n");
    else
        fprintf(f, "_pf_key - (char*)0;\n");
    }
codeStatement(pfc, f, body);
if (varWritten(elPp->var, body))
    {
    struct pfVar *var = elPp->var;
    struct dyString *elName = codeVarName(pfc, var);
    fprintf(f, "%s((", deref);
    codeBaseType(pfc, f, var->ty->base);
    fprintf(f, "%s)_pf_item) = %s;\n", deref, elName->string);
    dyStringFree(&elName);
    }
if (colBase == pfc->dirType && ixPp != NULL)
    codeCleanupVar(pfc, f, ixPp->var);
fprintf(f, "}\n\n");
dyStringFree(&elName);
dyStringFree(&keyName);
}

static void codeParaDoBlock(struct pfCompile *pfc, FILE *f, 
	int *pCounter, struct pfParse *enclosingFunction, 
	struct pfParse *enclosingClass, struct pfParse *para)
/* Generate a function for inside of para do. */
{
bool oldIsFunc = pfc->isFunc;
bool oldCodingPara = pfc->codingPara;
pfc->codingPara = TRUE;
pfc->isFunc = (enclosingFunction != NULL);
para->name = paraBlockName(pCounter);
codeParaBlockFunction(pfc, f, para, enclosingFunction, prtParaDo);
pfc->isFunc = oldIsFunc;
pfc->codingPara = oldCodingPara;
}

static void rCodeParaBlocks(struct pfCompile *pfc, FILE *f, 
	int *pCounter, struct pfParse *enclosingFunction, 
	struct pfParse *enclosingClass, struct pfParse *pp)
/* Generate a little subroutine for each para block. */
{
switch (pp->type)
    {
    case pptToDec:
    case pptFlowDec:
        enclosingFunction = pp;
	break;
    case pptClass:
        enclosingClass = pp;
	break;
    case pptParaDo:
        codeParaDoBlock(pfc, f, pCounter, enclosingFunction, enclosingClass,
		pp);
	break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rCodeParaBlocks(pfc, f, pCounter, enclosingFunction, enclosingClass, pp);
}

void codeParaBlocks(struct pfCompile *pfc, FILE *f, struct pfParse *module)
/* Create functions for code inside of para blocks. */
{
int count = 0;
rCodeParaBlocks(pfc, f, &count, NULL, NULL, module);
}

#else /* defined(PARALLEL) */

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
	struct pfParse *para)
/* Emit C code for para ... do statement. */
{
struct pfParse *collectionPp = para->children;
struct pfParse *elPp = collectionPp->next;
struct pfParse *body = elPp->next;
struct pfParse *elVarPp = NULL;

codeStartElInCollectionIteration(pfc, f, 0, 
	para->scope, elPp, collectionPp, TRUE);
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
codeEndElInCollectionIteration(pfc, f, para->scope, elPp, collectionPp, 
	TRUE);
}

void codeParaBlocks(struct pfCompile *pfc, FILE *f, struct pfParse *module)
/* Create functions for code inside of para blocks. */
{
}

#endif /* defined(PARALLEL) */

