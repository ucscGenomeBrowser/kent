/* isx - intermediate code -  hexes of
 *     label destType destName left op right 
 * For instance:
 *     100 + int t1 1 t2  (means int t1 = 1 + t2)
 *     101 * double v 0.5 t3  (means double v = 0.5 * t3)
 */

#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "isx.h"

static struct isxAddress *isxExpression(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, struct dlList *iList);
/* Generate intermediate code for expression. Return destination */

void isxStatement(struct pfCompile *pfc, struct pfParse *pp, 
	struct hash *varHash, struct dlList *iList);
/* Generate intermediate code for statement. */

char *isxValTypeToString(enum isxValType val)
/* Convert isxValType to string. */
{
switch (val)
    {
    case ivZero:
	return "zer";
    case ivByte:
	return "byt";
    case ivShort:
	return "sho";
    case ivInt:
	return "int";
    case ivLong:
	return "lng";
    case ivFloat:
	return "flt";
    case ivDouble:
	return "dbl";
    case ivObject:
	return "obj";
    case ivJump:
	return "jmp";
    default:
        internalErr();
	return NULL;
    }
}

char *isxOpTypeToString(enum isxOpType val)
/* Convert isxValType to string. */
{
switch (val)
    {
    case poInit:
        return "poInit";
    case poAssign:
        return "poAssign";
    case poPlus:
	return "poPlus";
    case poMinus:
	return "poMinus";
    case poMul:
	return "poMul";
    case poDiv:
	return "poDiv";
    case poMod:
	return "poMod";
    case poGoTo:
	return "poGoTo";
    case poBranch:
	return "poBranch";
    case poCall:
	return "poCall";
    default:
        internalErr();
	return NULL;
    }
}


static void isxAddrDump(struct isxAddress *iad, FILE *f)
/* Print variable name or n/a */
{
fprintf(f, "%s:", isxValTypeToString(iad->valType));
/* Convert isxValType to string. */
switch (iad->adType)
    {
    case iadZero:
	fprintf(f, "#zero");
	break;
    case iadConst:
	{
	struct pfToken *tok = iad->val.tok;
	union pfTokVal val = tok->val;
	switch(tok->type)
	    {
	    case pftInt:
		fprintf(f, "#%d", val.i);
		break;
	    case pftLong:
		fprintf(f, "#%lld", val.l);
		break;
	    case pftFloat:
		fprintf(f, "#%f", val.x);
		break;
	    case pftString:
		fprintf(f, "#'%s'", val.s);
		break;
	    }
	break;
	}
    case iadRealVar:
    case iadTempVar:
	fprintf(f, "%s", iad->name);
	break;
    }
}

void isxDump(struct isx *isx, FILE *f)
/* Dump out isx code */
{
struct isxAddress *iad;
fprintf(f, "%d: %s ", isx->label, isxOpTypeToString(isx->opType));
for (iad = isx->dest; iad != NULL; iad = iad->next)
    {
    fprintf(f, " ");
    isxAddrDump(iad, f);
    }
fprintf(f, " =");
for (iad = isx->source; iad != NULL; iad = iad->next)
    {
    fprintf(f, " ");
    isxAddrDump(iad, f);
    }
}

void isxDumpList(struct dlList *list, FILE *f)
/* Dump all of isx in list */
{
struct dlNode *node;
for (node = list->head; !dlEnd(node); node = node->next)
    {
    isxDump(node->val, f);
    fprintf(f, "\n");
    }
}

struct isx *isxNew(struct pfCompile *pfc, 
	enum isxOpType opType,
	struct isxAddress *dest,
	struct isxAddress *source,
	struct dlList *iList)
/* Return new isx */
{
struct isx *isx;
AllocVar(isx);
isx->label = ++pfc->isxLabelMaker;
isx->opType = opType;
isx->dest = dest;
isx->source = source;
dlAddValTail(iList, isx);
return isx;
}

static enum isxValType ppToIsxValType(struct pfCompile *pfc, 
	struct pfParse *pp)
/* Return isxValType corresponding to pp */
{
struct pfBaseType *base = pp->ty->base;
if (base == pfc->bitType || base == pfc->byteType)
    return ivByte;
else if (base == pfc->shortType)
    return ivShort;
else if (base == pfc->intType)
    return ivInt;
else if (base == pfc->longType)
    return ivLong;
else if (base == pfc->floatType)
    return ivFloat;
else if (base == pfc->doubleType)
    return ivDouble;
else
    return ivObject;
}

static struct isxAddress *constAddress(struct pfToken *tok, 
	enum isxValType valType)
/* Get place to put constant. */
{
struct isxAddress *iad;
AllocVar(iad);
iad->adType = iadConst;
iad->valType = valType;
iad->val.tok = tok;
return iad;
}

static struct isxAddress *zeroAddress()
/* Get place representing zero or nil. */
{
static struct isxAddress zero;
return &zero;
}

static struct isxAddress *tempAddress(struct pfCompile *pfc, struct hash *hash,
	enum isxValType valType)
/* Create a new temporary */
{
struct isxAddress *iad;
char buf[18];
safef(buf, sizeof(buf), "$t%X", ++(pfc->tempLabelMaker));
AllocVar(iad);
iad->adType = iadTempVar;
iad->valType = valType;
iad->val.tempId = pfc->tempLabelMaker;
hashAddSaveName(hash, buf, iad, &iad->name);
return iad;
}

static struct isxAddress *varAddress(struct pfVar *var, struct hash *hash,
	enum isxValType valType)
/* Create reference to a real var */
{
struct isxAddress *iad;
AllocVar(iad);
iad->adType = iadRealVar;
iad->valType = valType;
iad->val.var = var;
hashAddSaveName(hash, var->cName, iad, &iad->name);
return iad;
}


static struct isxAddress *isxBinaryOp(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, 
	enum isxOpType op, struct dlList *iList)
/* Generate intermediate code for expression. Return destination */
{
struct isxAddress *left = isxExpression(pfc, pp->children,
	varHash, iList);
struct isxAddress *right = isxExpression(pfc, pp->children->next,
	varHash, iList);
struct isxAddress *dest = tempAddress(pfc, varHash, ppToIsxValType(pfc,pp));
left->next = right;
isxNew(pfc, op, dest, left, iList);
return dest;
}

static struct isxAddress *isxExpression(struct pfCompile *pfc, 
	struct pfParse *pp, struct hash *varHash, struct dlList *iList)
/* Generate intermediate code for expression. Return destination */
{
switch (pp->type)
    {
    case pptConstBit:
    case pptConstByte:
    case pptConstChar:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
    case pptConstString:
	return constAddress(pp->tok, ppToIsxValType(pfc, pp));
    case pptVarUse:
	return varAddress(pp->var, varHash, ppToIsxValType(pfc, pp));
    case pptPlus:
	return isxBinaryOp(pfc, pp, varHash, poPlus, iList);
    case pptMinus:
	return isxBinaryOp(pfc, pp, varHash, poMinus, iList);
    case pptMul:
	return isxBinaryOp(pfc, pp, varHash, poMul, iList);
    case pptDiv:
	return isxBinaryOp(pfc, pp, varHash, poDiv, iList);
    case pptMod:
	return isxBinaryOp(pfc, pp, varHash, poMod, iList);
    default:
	pfParseDump(pp, 3, uglyOut);
        internalErr();
	return NULL;
    }
}

void isxStatement(struct pfCompile *pfc, struct pfParse *pp, 
	struct hash *varHash, struct dlList *iList)
/* Generate intermediate code for statement. */
{
switch (pp->type)
    {
    case pptVarInit:
	{
	struct pfParse *init = pp->children->next->next;
	struct isxAddress *dest = varAddress(pp->var, varHash, 
		ppToIsxValType(pfc, pp));
	struct isxAddress *source;
	if (init)
	    source = isxExpression(pfc, init, varHash, iList);
	else
	    source = zeroAddress();
	isxNew(pfc, poInit, dest, source, iList);
	break;
	}
    case pptAssign:
        {
	struct pfParse *use = pp->children;
	struct pfParse *val = use->next;
	struct isxAddress *source = isxExpression(pfc, val, varHash, iList);
	struct isxAddress *dest = varAddress(use->var, varHash, 
		ppToIsxValType(pfc, use));
	isxNew(pfc, poAssign, dest, source, iList);
	break;
	}
    default:
	pfParseDump(pp, 3, uglyOut);
        errAt(pp->tok, "Unrecognized statement in isxStatement");
	break;
    }
}

void isxModule(struct pfCompile *pfc, struct pfParse *pp, 
	struct dlList *iList)
/* Generate instructions for module. */
{
struct hash *varHash = hashNew(16);
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    isxStatement(pfc, pp, varHash, iList);
    }
}

struct dlList *isxFromParse(struct pfCompile *pfc, struct pfParse *pp)
/* Convert parse tree to isx. */
{
struct dlList *iList = dlListNew(0);
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    switch (pp->type)
        {
	case pptMainModule:
	case pptModule:
	    isxModule(pfc, pp, iList);
	    break;
	}
    }
return iList;
}

