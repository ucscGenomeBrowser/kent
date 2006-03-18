/* isix - intermediate code -  hexes of
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
#include "isix.h"


char *isixValTypeToString(enum isixValType val)
/* Convert isixValType to string. */
{
switch (val)
    {
    case ivByte:
	return "ivByte";
    case ivShort:
	return "ivShort";
    case ivInt:
	return "ivInt";
    case ivLong:
	return "ivLong";
    case ivFloat:
	return "ivFloat";
    case ivDouble:
	return "ivDouble";
    case ivObject:
	return "ivObject";
    case ivJump:
	return "ivJump";
    default:
        internalErr();
	return NULL;
    }
}

char *isixOpTypeToString(enum isixOpType val)
/* Convert isixValType to string. */
{
switch (val)
    {
    case poZero:
        return "poZero";
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
    case poGoTo:
	return "poGoTo";
    case poBranch:
	return "poBranch";
    default:
        internalErr();
	return NULL;
    }
}


static void isixAddrDump(struct isixAddress *iad, FILE *f)
/* Print variable name or n/a */
{
if (iad == NULL)
    fprintf(f, "n/a");
else
    {
    switch (iad->type)
	{
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
}

void isixDump(struct isix *isix, FILE *f)
/* Dump out isix code */
{
fprintf(f, "%d: %s %s ", isix->label, isixOpTypeToString(isix->opType),
	isixValTypeToString(isix->valType));
isixAddrDump(isix->dest, f);
fprintf(f, " ");
isixAddrDump(isix->left, f);
fprintf(f, " ");
isixAddrDump(isix->right, f);
fprintf(f, "\n");
}

void isixDumpList(struct dlList *list, FILE *f)
/* Dump all of isix in list */
{
struct dlNode *node;
for (node = list->head; !dlEnd(node); node = node->next)
    isixDump(node->val, f);
}

struct isix *isixNew(struct pfCompile *pfc, 
	enum isixOpType opType,
	enum isixValType valType,
	struct isixAddress *dest,
	struct isixAddress *left,
	struct isixAddress *right, 
	struct dlList *iList)
/* Return new isix */
{
struct isix *isix;
AllocVar(isix);
isix->label = ++pfc->isixLabelMaker;
isix->opType = opType;
isix->valType = valType;
isix->dest = dest;
isix->left = left;
isix->right = right;
dlAddValTail(iList, isix);
return isix;
}

static enum isixValType ppToIsixValType(struct pfCompile *pfc, struct pfParse *pp)
/* Return isixValType corresponding to pp */
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

static struct isixAddress *constAddress(struct pfToken *tok)
/* Get place to put constant. */
{
struct isixAddress *iad;
AllocVar(iad);
iad->type = iadConst;
iad->val.tok = tok;
return iad;
}

static struct isixAddress *tempAddress(struct pfCompile *pfc, struct hash *hash)
/* Create a new temporary */
{
struct isixAddress *iad;
char buf[18];
safef(buf, sizeof(buf), "$t%X", ++(pfc->tempLabelMaker));
AllocVar(iad);
iad->type = iadTempVar;
iad->val.tempId = pfc->tempLabelMaker;
hashAddSaveName(hash, buf, iad, &iad->name);
return iad;
}

static struct isixAddress *varAddress(struct pfVar *var, struct hash *hash)
/* Create reference to a real var */
{
struct isixAddress *iad;
AllocVar(iad);
iad->type = iadRealVar;
iad->val.var = var;
hashAddSaveName(hash, var->cName, iad, &iad->name);
return iad;
}


struct isixAddress *isixExpression(struct pfCompile *pfc, struct pfParse *pp, 
	struct isixAddress *dest, struct hash *varHash, struct dlList *iList)
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
        {
	if (dest)
	    {
	    isixNew(pfc, poAssign, ppToIsixValType(pfc, pp), dest, 
	    	constAddress(pp->tok), NULL, iList);
	    return dest;
	    }
	else
	    return constAddress(pp->tok);
	}
    case pptVarUse:
        {
	if (dest)
	    {
	    isixNew(pfc, poAssign, ppToIsixValType(pfc, pp), dest, 
	    	varAddress(pp->var, varHash), NULL, iList);
	    return dest;
	    }
	else
	    return varAddress(pp->var, varHash);
	}
    case pptPlus:
        {
	struct isixAddress *left = isixExpression(pfc, pp->children,
		NULL, varHash, iList);
	struct isixAddress *right = isixExpression(pfc, pp->children->next,
		NULL, varHash, iList);
	if (!dest)
	    dest = tempAddress(pfc, varHash);
	isixNew(pfc, poPlus, ppToIsixValType(pfc, pp), dest, left, right, iList);
	return dest;
	}
    default:
	pfParseDump(pp, 3, uglyOut);
        internalErr();
	return NULL;
    }
}

void isixStatement(struct pfCompile *pfc, struct pfParse *pp, 
	struct hash *varHash, struct dlList *iList)
/* Generate intermediate code for statement. */
{
switch (pp->type)
    {
    case pptVarInit:
	{
	struct pfParse *init = pp->children->next->next;
	struct isixAddress *dest = varAddress(pp->var, varHash);
	if (init)
	    isixExpression(pfc, init, dest, varHash, iList);
	else
	    isixNew(pfc, poZero, ppToIsixValType(pfc, pp), dest, NULL, NULL, 
	    	iList);
	break;
	}
    default:
        errAt(pp->tok, "Unrecognized statement in isixStatement");
	break;
    }
}

void isixModule(struct pfCompile *pfc, struct pfParse *pp, 
	struct dlList *iList)
/* Generate instructions for module. */
{
struct hash *varHash = hashNew(16);
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    isixStatement(pfc, pp, varHash, iList);
    }
}

struct dlList *isixFromParse(struct pfCompile *pfc, struct pfParse *pp)
/* Convert parse tree to isix. */
{
struct dlList *iList = dlListNew(0);
for (pp = pp->children; pp != NULL; pp = pp->next)
    {
    switch (pp->type)
        {
	case pptMainModule:
	case pptModule:
	    isixModule(pfc, pp, iList);
	    break;
	}
    }
return iList;
}

