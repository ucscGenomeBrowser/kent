/* ctar - stuff to manage compile-time activation records.
 * This generates code for the run time activation records,
 * which are pushed onto a stack at the start of each function
 * and popped on the end. This lets us clean up local vars when 
 * there's an exception, and also allows us to print a nice
 * stack trace. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "recodedType.h"
#include "codedType.h"
#include "cCoder.h"
#include "ctar.h"

void ctarFree(struct ctar **pCtar)
/* Free up memory associated with activation record. */
{
struct ctar *ctar = *pCtar;
if (ctar != NULL)
    {
    slFreeList(&ctar->varRefList);
    freez(pCtar);
    }
}

static void rAddVars(struct ctar *ctar, struct pfParse *pp, 
	struct hash *aliasHash, int *pAliasNum)
/* Add variables in parse tree to activation record. */
{
switch (pp->type)
    {
    case pptVarInit:
	{
	struct pfVar *var = pp->var;
	if (var->ty->access != paStatic)
	    {
	    char *name = var->cName;

	    /* Make sure C name is unique.  It might not be if they reuse
	     * a name in an inner block. */
	    if (hashLookup(aliasHash, name))
		{
		int aliasSize = strlen(name) + 8 + 1;   /* Allow up to 8 digits of alias */
		char *alias = needMem(aliasSize);	
		for (;;)
		    {
		    *pAliasNum += 1;
		    safef(alias, aliasSize, "%s%d", name, *pAliasNum);
		    if (!hashLookup(aliasHash, alias))
			break;
		    }
		var->cName = alias;
		}
	    hashAdd(aliasHash, var->cName, NULL);
	    refAdd(&ctar->varRefList, var);
	    }
        break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rAddVars(ctar, pp, aliasHash, pAliasNum);
}


static char *cNameForFunction(struct pfParse *funcDec)
/* Return mangled C name for function */
{
struct dyString *dy = dyStringNew(0);
char *result;
struct pfBaseType *class = funcDec->var->scope->class;
if (class)
    {
    dyStringPrintf(dy, "_pf_cm%d_%s_%s", class->scope->id,
    	class->name, funcDec->name);
    }
else
    {
    dyStringPrintf(dy, "pf_%s", funcDec->name);
    }
result = cloneString(dy->string);
dyStringFree(&dy);
return result;
}

struct ctar *ctarOnFunction(struct pfParse *funcDec)
/* Create a compile time activation record for function. Hangs it on pp->var->ctar
 * and returns it. */
{
struct ctar *ctar;
struct hash *aliasHash = hashNew(8);
int aliasNum = 1;
static int id=0;
struct pfBaseType *class = funcDec->var->scope->class;

AllocVar(ctar);
ctar->id = ++id;
ctar->name = funcDec->name;
ctar->cName = cNameForFunction(funcDec);
ctar->pp = funcDec;
rAddVars(ctar, funcDec, aliasHash, &aliasNum);
if (class != NULL)
    {
    struct pfVar *selfVar = pfScopeFindVar(funcDec->scope, "self");
    assert(selfVar != NULL);
    refAdd(&ctar->varRefList, selfVar);
    if (pfBaseIsDerivedClass(class))
        {
	struct pfVar *parentVar = pfScopeFindVar(funcDec->scope, "parent");
	assert(parentVar != NULL);
	refAdd(&ctar->varRefList, parentVar);
	}
    }
slReverse(&ctar->varRefList);
hashFree(&aliasHash);
funcDec->var->ctar = ctar;
return ctar;
}


static void ctarFixedCode(struct ctar *ctar, struct pfCompile *pfc, FILE *f)
/* Write out fixed part of activation record for this function. */
{
struct pfParse *funcDec = ctar->pp;
struct pfBaseType *class = funcDec->var->scope->class;
int id = ctar->id;
int localCount = slCount(ctar->varRefList);
fprintf(f, "/* local var info for %s */\n",  ctar->cName);
if (localCount > 0)
    {
    struct slRef *ref;
    struct pfVar *var;
    fprintf(f, "static struct _pf_localVarInfo _pf_rtar%d_vars[%d] = {\n", id, localCount);
    for (ref = ctar->varRefList; ref != NULL; ref = ref->next)
        {
	var = ref->val;
	fprintf(f, " {\"%s\", ", var->name);
	fprintf(f, "%d, ", recodedTypeId(pfc, var->ty));
	fprintf(f, "0},\n");
	}
    fprintf(f, "};\n");
    }
fprintf(f, "static struct _pf_functionFixedInfo _pf_rtar%d_fixed = {", id);
if (class)
    {
    struct pfType *ty = pfTypeNew(class);
    fprintf(f, "%d, ", recodedTypeId(pfc, ty));
    freeMem(ty);
    }
else
    fprintf(f, "-1, ");

fprintf(f, "  \"%s\", %d, %d, ", ctar->name, 
	recodedTypeId(pfc, funcDec->var->ty), localCount);
if (localCount > 0)
    fprintf(f, "_pf_rtar%d_vars,", id);
else
    fprintf(f, "0,");
fprintf(f, "};\n\n");
}

void ctarCodeFixedParts(struct ctar *ctarList, struct pfCompile *pfc, FILE *f)
/* Write parts of function activation record that don't change from invocation
 * to invocation of function. */
{
struct ctar *ctar;
for (ctar = ctarList; ctar != NULL; ctar = ctar->next)
    ctarFixedCode(ctar, pfc, f);
fprintf(f, "static struct _pf_functionFixedInfo *_pf_rtar_fixed[] = {\n");
for (ctar = ctarList; ctar != NULL; ctar = ctar->next)
     fprintf(f, " &_pf_rtar%d_fixed,\n", ctar->id);
fprintf(f, "};\n\n");
}

void ctarCodeStartupCall(struct ctar *ctarList, struct pfCompile *pfc, FILE *f)
/* Code call to initialize types and offsets of fixed run time activation records. */
{
fprintf(f, "_pf_rtar_init_tables(_pf_rtar_fixed, %d, _pf_lti);\n", slCount(ctarList));
}

void ctarCodeLocalStruct(struct ctar *ctar, struct pfCompile *pfc, FILE *f,
	char *refName)
/* Write out a structure that has all of the local variables for a function. 
 * If refName is zero then make a pointer of this type equal to refName,
 * otherwise make an actual instand initialized to zero. */
{
struct slRef *ref;
if (ctar->varRefList)
    {
    fprintf(f, "struct _pf_l {\n");
    for (ref = ctar->varRefList; ref != NULL; ref = ref->next)
	{
	fprintf(f, " ");
	struct pfVar *var = ref->val;
	codeBaseType(pfc, f, var->ty->base);
	fprintf(f, " %s;\n", var->cName);
	}
    fprintf(f, "}");
    if (refName)
        fprintf(f, "*_pf_l = %s;\n", refName);
    else
	{
	fprintf(f, "_pf_l;\n");
	fprintf(f, "memset(&_pf_l, 0, sizeof(_pf_l));\n");
	}
    }
}

void ctarCodePush(struct ctar *ctar, struct pfCompile *pfc, FILE *f)
/* Write out code to define run-time activation structure and push
 * it on activation stack. */
{
fprintf(f, "struct _pf_activation _pf_act;\n");
fprintf(f, "_pf_act.parent = _pf_activation_stack;\n");
fprintf(f, "_pf_act.fixed = &_pf_rtar%d_fixed;\n", ctar->id);
if (ctar->varRefList == NULL)
    fprintf(f, "_pf_act.data = 0;\n");
else
    fprintf(f, "_pf_act.data = &_pf_l;\n");
fprintf(f, "_pf_activation_stack = &_pf_act;\n");
}

void ctarCodePop(struct ctar *ctar, struct pfCompile *pfc, FILE *f)
/* Write out code to pop activation structure off of stack. */
{
fprintf(f, "_pf_activation_stack = _pf_activation_stack->parent;\n");
}
