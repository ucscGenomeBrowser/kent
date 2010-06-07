/* cCaseCode - produce C code for a ParaFlow case statement. */
/* The parse tree for a case looks like so:
 *    pptCase
 *       <expression>
 *       pptCaseList
 *          pptCaseItem
 *             <tuple>
 *             <statement>
 *          pptCaseItem
 *             <tuple>
 *             <statement>
 *          pptCaseElse
 *             <statement> */

#include "common.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "cCoder.h"

static boolean containsBreak(struct pfParse *pp)
/* Return true if statement of it's children is break. */
{
if (pp->type == pptBreak)
    return TRUE;
for (pp = pp->children; pp != NULL; pp = pp->next)
    if (containsBreak(pp))
        return TRUE;
return FALSE;
}

static boolean canCodeSwitch(struct pfCompile *pfc, struct pfParse *casePp)
/* Return TRUE if it looks like we can code it as a switch. */
{
struct pfParse *expPp = casePp->children;
struct pfBaseType *base = expPp->ty->base;
struct pfParse *listPp = expPp->next;
struct pfParse *itemPp;
if (!(base == pfc->byteType || base == pfc->shortType || base == pfc->charType
   || base == pfc->intType || base == pfc->longType))
     return FALSE;
for (itemPp = listPp->children; itemPp != NULL; itemPp = itemPp->next)
    {
    if (itemPp->type == pptCaseItem)
        {
	struct pfParse *tuplePp = itemPp->children;
	struct pfParse *statement = tuplePp->next;
	struct pfParse *pp;
	for (pp = tuplePp->children; pp != NULL; pp = pp->next)
	    {
	    switch (pp->type)
	        {
		case pptConstByte:
		case pptConstShort:
		case pptConstInt:
		case pptConstLong:
		case pptConstChar:
		case pptConstZero:
		    break;
		default:
		    return FALSE;
		}
	    }
	if (containsBreak(statement))
	    return FALSE;
	}
    }
return TRUE;
}

static void codeSwitch(struct pfCompile *pfc, FILE *f, struct pfParse *casePp)
/* Generate a switch statement corresponding to paraFlow case where
 * everything in all the tuples is an integer constant. */
{
struct pfParse *expPp = casePp->children;
struct pfBaseType *base = expPp->ty->base;
struct pfParse *listPp = expPp->next;
struct pfParse *itemPp;

codeExpression(pfc, f, expPp, 0, FALSE);
fprintf(f, "switch (");
codeParamAccess(pfc, f, base, 0);
fprintf(f, ")\n");
fprintf(f, "{\n");
for (itemPp = listPp->children; itemPp != NULL; itemPp = itemPp->next)
    {
    if (itemPp->type == pptCaseItem)
        {
	struct pfParse *tuplePp = itemPp->children;
	struct pfParse *statement = tuplePp->next;
	struct pfParse *pp;
	for (pp = tuplePp->children; pp != NULL; pp = pp->next)
	    {
	    struct pfToken *tok = pp->tok;
	    fprintf(f, "case ");
	    switch (pp->type)
	        {
		case pptConstByte:
		case pptConstShort:
		case pptConstInt:
		    fprintf(f, "%d", tok->val.i);
		    break;
		case pptConstChar:
		    fprintf(f, "%d", tok->val.s[0]);
		    break;
		case pptConstLong:
		    fprintf(f, "%lld", tok->val.l);
		    break;
		case pptConstZero:
		    fprintf(f, "0");
		    break;
		default:
		    internalErr();
		    break;
		}
	    fprintf(f, ":\n");
	    }
	codeStatement(pfc, f, statement);
	fprintf(f, "break;\n");
	}
    else
        {
	fprintf(f, "default:\n");
	codeStatement(pfc, f, itemPp->children);
	fprintf(f, "break;\n");
	}
    }
fprintf(f, "}\n");
}

static void codeBigIfElse(struct pfCompile *pfc, FILE *f, 
	struct pfParse *casePp)
/* Generate big if/else statement for more general case. */
{
struct pfParse *expPp = casePp->children;
struct pfBaseType *base = expPp->ty->base;
struct pfParse *listPp = expPp->next;
struct pfParse *itemPp;

fprintf(f, "{\n");
codeBaseType(pfc, f, base);
fprintf(f, " _pf_case;\n");
codeExpression(pfc, f, expPp, 0, TRUE);
fprintf(f, "_pf_case = ");
codeParamAccess(pfc, f, base, 0);
fprintf(f, ";\n");

fprintf(f, "{\n");
for (itemPp = listPp->children; itemPp != NULL; itemPp = itemPp->next)
    {
    if (itemPp->type == pptCaseItem)
        {
	struct pfParse *tuplePp = itemPp->children;
	struct pfParse *statement = tuplePp->next;
	struct pfParse *pp;
	int stack = 0, tupleSize=0, i;

	codeBaseType(pfc, f, pfc->bitType);
	fprintf(f, " _pf_bit;\n");
	for (pp = tuplePp->children; pp != NULL; pp = pp->next)
	    {
	    int expSize = codeExpression(pfc, f, pp, stack, TRUE);
	    stack += expSize;
	    assert(expSize == 1);
	    tupleSize += 1;
	    }
	fprintf(f, "_pf_bit = (");
	for (i=0; i<tupleSize; ++i)
	    {
	    if (i != 0)
	        fprintf(f, " || ");
	    if (base == pfc->stringType || base == pfc->dyStringType)
		{
		fprintf(f, "_pf_stringSame(_pf_case, ");
		codeParamAccess(pfc, f, base, i);
		fprintf(f, ")");
		}
	    else
		{
		codeParamAccess(pfc, f, base, i);
		fprintf(f, " == _pf_case");
		}
	    }
	fprintf(f, ");\n");
	for (i=tupleSize-1; i>=0; --i)
	    codeCleanupStackPos(pfc, f, expPp->ty, i);
	fprintf(f, "if (_pf_bit)\n");
	fprintf(f, "{\n");
	codeStatement(pfc, f, statement);
	fprintf(f, "}\n");
	fprintf(f, "else\n");
	fprintf(f, "{\n");
	}
    else if (itemPp->type == pptCaseElse)
        {
	codeStatement(pfc, f, itemPp->children);
	}
    else
        internalErr();
    }
for (itemPp = listPp->children; itemPp != NULL; itemPp = itemPp->next)
    if (itemPp->type == pptCaseItem)
	fprintf(f, "}\n");
fprintf(f, "}\n");
codeCleanupVarNamed(pfc, f, expPp->ty, "_pf_case");
fprintf(f, "}\n");
}

void codeCase(struct pfCompile *pfc, FILE *f, struct pfParse *casePp)
/* Emit C code for case statement. */
{
if (canCodeSwitch(pfc, casePp))
    codeSwitch(pfc, f, casePp);
else
    codeBigIfElse(pfc, f, casePp);
}

