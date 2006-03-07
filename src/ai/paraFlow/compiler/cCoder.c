/* cCoder - produce C code from type-checked parse tree. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "recodedType.h"
#include "codedType.h"
#include "ctar.h"
#include "cCoder.h"

#define globalPrefix "pf_"
#define localPrefix "_pf_l."

void codeStatement(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp);
/* Emit code for one statement. */

int codeExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef);
/* Emit code for one expression.  Returns how many items added
 * to stack. */

static void codeScope(struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean isModuleScope, struct ctar *ctarList);
/* Print types and then variables from scope. */

static void codeScopeVars(struct pfCompile *pfc, FILE *f, 
	struct pfScope *scope, boolean zeroUnitialized);
/* Print out variable declarations associated with scope. */

static void printPreamble(struct pfCompile *pfc, FILE *f, char *fileName, boolean doExtern)
/* Print out C code for preamble. */
{
fprintf(f, "/* This file is a translation of %s by paraFlow. */\n", 
	fileName);
fprintf(f, "\n");
fprintf(f, "#include \"pfPreamble.h\"\n");
fprintf(f, "\n");
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_Var _pf_var_zero;   /* Helps initialize vars to zero. */\n");
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_Array %sargs;	    /* Command line arguments go here. */\n",
	globalPrefix);
fprintf(f, "static _pf_Bit %strue=1, %sfalse=0;\n", globalPrefix, globalPrefix);
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_String %sprogramName; /* Name of program (argv[0]) */\n",
	globalPrefix);
fprintf(f, "\n");
}

static char *moduleRuntimeType = "_pf_moduleRuntime";
static char *moduleRuntimeList = "_pf_moduleList";
static char *moduleRuntimeName = "_pf_lmodule";
static char *moduleCleanupName = "_pf_moduleCleanup";

static void printSysVarsAndPrototypes(FILE *f)
/* Print stuff needed for main() */
{
fprintf(f, "struct _pf_activation *_pf_activation_stack;\n");
fprintf(f, "struct %s *%s;\n", moduleRuntimeType, moduleRuntimeList);
fprintf(f, "void _pf_init_args(int argc, char **argv, _pf_String *retProg, _pf_Array *retArgs, char *environ[]);\n");
fprintf(f, "\n");
}

char *stackName = "_pf_stack";
char *stackType = "_pf_Stack";

static void printModuleCleanupPrototype(FILE *f)
/* Print function prototype for module cleanup routine. */
{
fprintf(f, "static void %s(struct %s *module, %s *stack)",
	moduleCleanupName, moduleRuntimeType, stackType);
}

static char *localTypeTableType = "_pf_local_type_info";
static char *localTypeTableName = "_pf_lti";

static void codeLocalTypeRef(FILE *f, int ref)
/* Print out local type reference. */
{
fprintf(f, "%s[%d].id", localTypeTableName, ref);
}

static void codeLocalTypeTableName(FILE *f, char *moduleName)
/* Print out local type table name. */
{
fprintf(f, "struct %s %s_%s", localTypeTableType, localTypeTableName, 
	mangledModuleName(moduleName));
}

static void codeForType(struct pfCompile *pfc, FILE *f, struct pfType *type)
/* Print out code to access type ID */
{
codeLocalTypeRef(f, recodedTypeId(pfc, type));
}

static void codeRunTimeError(FILE *f, struct pfParse *pp, char *message)
/* Print code for a run time error message. */
{
char *file;
struct pfToken *tok = pp->tok;
int line, col;
pfSourcePos(tok->source, tok->text, &file, &line, &col);
fprintf(f, "_pf_run_err(\"Run time error line %d col %d of %s: %s\");\n", 
	line+1, col+1, file, message);
}

static void codeUseOfNil(FILE *f, struct pfParse *pp)
/* Print code for use of nil message. */
{
codeRunTimeError(f, pp, "using uninitialized variable");
}

static void codeNilCheck(FILE *f, struct pfParse *pp, int stack)
/* Print code to check stack for nil. */
{
fprintf(f, "if (%s[%d].v == 0) ", stackName, stack);
codeUseOfNil(f, pp);
}

static char *typeKey(struct pfCompile *pfc, struct pfBaseType *base)
/* Return key for type if available, or NULL */
{
if (base == pfc->bitType)
    return "Bit";
else if (base == pfc->byteType)
    return "Byte";
else if (base == pfc->shortType)
    return "Short";
else if (base == pfc->intType)
    return "Int";
else if (base == pfc->longType)
    return "Long";
else if (base == pfc->floatType)
    return "Float";
else if (base == pfc->doubleType)
    return "Double";
else if (base == pfc->charType)
    return "Char";
else if (base == pfc->stringType || base == pfc->dyStringType)
    return "String";
else if (base == pfc->varType)
    return "Var";
else if (base == pfc->arrayType)
    return "Array";
else if (base == pfc->dirType)
    return "Dir";
//else if (base == pfc->listType)
//   return "List";
//else if (base == pfc->treeType)
//    return "Tree";
else if (base == pfc->toPtType)
    return "FunctionPt";
else if (base == pfc->flowPtType)
    return "FunctionPt";
else 
    return NULL;
}

void codeBaseType(struct pfCompile *pfc, FILE *f, struct pfBaseType *base)
/* Print out type info for C. */
{
char *s = typeKey(pfc, base);
if (s == NULL)
    fprintf(f, "struct %s*", base->cName);
else
    fprintf(f, "_pf_%s", s);
}

static void codeStackAccess(FILE *f, int offset)
/* Print out code to access stack at offset. */
{
fprintf(f, "%s[%d]", stackName, offset);
}

static void codeStackFieldAccess(FILE *f, char *field, int offset)
/* Code access to field on stack at offset. */
{
codeStackAccess(f, offset);
fprintf(f, ".%s", field);
}

struct dyString *varName(struct pfCompile *pfc, struct pfVar *var)
/* Return  variable name from C point of view.  (Easy unless it's static). */
{
struct dyString *name = dyStringNew(256);
struct pfType *type = var->ty;
if (type->access == paStatic)
    {
    /* Find enclosing function. */
    struct pfParse *toDec, *classDec;
    char *className = "";
    for (toDec = var->parse; toDec != NULL; toDec = toDec->parent)
	if (toDec->type == pptToDec)
	    break;
    if (toDec == NULL)
        internalErr();
    for (classDec = toDec->parent; classDec!=NULL; classDec = classDec->parent)
	if (toDec->type == pptClass)
	    break;
    if (classDec != NULL)
        className = classDec->name;
    dyStringPrintf(name, "_pf_sta_%d_%s_%s_%s", 
		var->scope->id, className, toDec->name, var->name);
    }
else
    {
    if (var->scope->isLocal)
	dyStringAppend(name, "_pf_l.");
    else
	dyStringAppend(name, globalPrefix);
    dyStringAppend(name, var->cName);
    }
return name;
}

static void printVarName(struct pfCompile *pfc, FILE *f, struct pfVar *var)
/* Print variable name from C point of view.  (Easy unless it's static). */
{
struct dyString *name = varName(pfc, var);
fprintf(f, "%s", name->string);
dyStringFree(&name);
}

static char *vTypeKey(struct pfCompile *pfc, struct pfBaseType *base)
/* Return typeKey, or "v" is that would be NULL.  The v is used
 * to access void pointer on stack. */
{
char *s = typeKey(pfc, base);
if (s == NULL)
    s = "v";
return s;
}

void codeParamAccess(struct pfCompile *pfc, FILE *f, 
	struct pfBaseType *base, int offset)
/* Print out code to access paramater of given type at offset. */
{
char *s = vTypeKey(pfc, base);
codeStackFieldAccess(f, s, offset);
}

static void rPrintFields(struct pfCompile *pfc, 
	FILE *f, struct pfBaseType *base)
/* Print out fields - parents first */
{
struct pfType *type;
if (base->parent != NULL)
    rPrintFields(pfc, f, base->parent);
for (type = base->fields; type != NULL; type = type->next)
    {
    codeBaseType(pfc, f, type->base);
    fprintf(f, " %s;\n", type->fieldName);
    }
}


static boolean isInitialized(struct pfVar *var)
/* Return TRUE if variable is initialized on declaration in  parse tree. */
{
struct pfParse *pp = var->parse;
if (pp->type != pptVarInit)
    errAt(pp->tok, "Expecting pptVarInit got %s for var %s",
    	pfParseTypeAsString(pp->type), var->name);
return pp->children->next->next != NULL;
}

static void codeMethodName(struct pfCompile *pfc, struct pfToken *tok, 
	FILE *f, struct pfBaseType *base, char *name, int stack)
/* Find method in current class or one of it's parents, and print
 * call to it */
{
if (base == pfc->stringType || base == pfc->dyStringType)
    {
    fprintf(f, "_pf_cm_string_%s", name);
    }
else if (base == pfc->arrayType)
    {
    fprintf(f, "_pf_cm_array_%s", name);
    }
else if (base == pfc->dirType)
    {
    fprintf(f, "_pf_cm_dir_%s", name);
    }
else
    {
    struct pfType *method = NULL;
    while (base != NULL)
	{
	for (method = base->methods; method != NULL; method = method->next)
	    {
	    if (sameString(method->fieldName, name))
		break;
	    }
	if (method != NULL)
	     break;
	base = base->parent;
	}
    if (base != NULL)
	{
	if (base->isClass)
	    {
	    if (method->tyty == tytyVirtualFunction)
		{
		fprintf(f, "%s[%d].Obj->_pf_polyTable[%d]",
		    stackName, stack, method->polyOffset);
		}
	    else
		fprintf(f, "%s_%s", base->methodPrefix, name);
	    }
	else
	    internalErrAt(tok);
	}
    else
	{
	internalErrAt(tok);
	}
    }
}


static int codeCall(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for a function call. */
{
struct pfParse *function = pp->children;
struct pfParse *inTuple = function->next;
struct pfType *outTuple = function->ty->children->next;
int outCount = slCount(outTuple->children);
struct pfParse *in;

if (function->type == pptDot)
    {
    struct pfParse *dotList = function->children;
    struct pfParse *class = NULL, *method = NULL;
    struct pfBaseType *base = NULL;
    int dotCount = slCount(dotList);
    assert(dotCount == 2);

    class = dotList;
    method = dotList->next;
    codeExpression(pfc, f, class, stack, FALSE);
    base = class->ty->base;

    /* Put rest of input on the stack, and print call with mangled function name. */
    codeExpression(pfc, f, inTuple, stack+1, TRUE);
    if (base->isInterface)
        {
	fprintf(f, "{\n");
	fprintf(f, "struct %s *_pf_face = %s[%d].v;\n",
		base->cName, stackName, stack);
	fprintf(f, "%s[%d].v = _pf_face->_pf_obj;\n", stackName, stack);
	fprintf(f, "_pf_face->%s(%s+%d);\n", method->name, 
		stackName, stack);
	fprintf(f, "}\n");
	}
    else
	{
	codeMethodName(pfc, pp->tok, f, base, method->name, stack);
	fprintf(f, "(%s+%d);\n", stackName, stack);
	}
    }
else
    {
    codeExpression(pfc, f, inTuple, stack, TRUE);
    assert(function->type == pptVarUse);
    if (stack == 0)
	fprintf(f, "%s%s(%s);\n", globalPrefix, pp->name, stackName);
    else
	fprintf(f, "%s%s(%s+%d);\n", globalPrefix, pp->name, stackName, stack);
    }
return outCount;
}

static int codeIndirectCall(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for a function call through a function pointer. */
{
struct pfParse *function = pp->children;
struct pfParse *inTuple = function->next;
struct pfType *outTuple = function->ty->children->next;
int outCount = slCount(outTuple->children);
int paramSize = codeExpression(pfc, f, inTuple, stack, TRUE);
codeExpression(pfc, f, function, stack + paramSize, FALSE);
codeParamAccess(pfc, f, function->ty->base, stack+paramSize);
fprintf(f, "(%s+%d);\n", stackName, stack);
return outCount;
}


void codeCleanupVarNamed(struct pfCompile *pfc, FILE *f, 
	struct pfType *type, char *name)
/* Emit cleanup code for variable of given type and name. */
{
if (type->base->needsCleanup)
    {
    if (type->base == pfc->varType)
	fprintf(f, "_pf_var_cleanup(%s);\n", name);
    else
	{
	fprintf(f, "if (0!=%s && (%s->_pf_refCount-=1) <= 0)\n", name, name);
	fprintf(f, "   %s->_pf_cleanup(%s, ", name, name);
	codeForType(pfc, f, type);
	fprintf(f, ");\n");
	}
    }
}

void codeCleanupStackPos(struct pfCompile *pfc, FILE *f,
     struct pfType *type, int stack)
/* Generate cleanup code for stack position. */
{
if (type->base->needsCleanup)
    {
    if (type->base == pfc->varType)
	{
	fprintf(f, "_pf_var_cleanup(");
	codeParamAccess(pfc, f, type->base, stack);
	fprintf(f, ");\n");
	}
    else
        {
	fprintf(f, "if (0!=");
	codeParamAccess(pfc, f, type->base, stack);
	fprintf(f, " && (");
	codeParamAccess(pfc, f, type->base, stack);
	fprintf(f, "->_pf_refCount-=1) <= 0)\n");
	codeParamAccess(pfc, f, type->base, stack);
	fprintf(f, "->_pf_cleanup(");
	codeParamAccess(pfc, f, type->base, stack);
	fprintf(f, ", ");
	codeForType(pfc, f, type);
	fprintf(f, ");\n");
	}
    }
}


static void codeCleanupVar(struct pfCompile *pfc, FILE *f, 
        struct pfVar *var)
/* Emit cleanup code for variable of given type and name. */
{
struct dyString *name = varName(pfc, var);
codeCleanupVarNamed(pfc, f, var->ty, name->string);
dyStringFree(&name);
}


static void startElInCollectionIteration(struct pfCompile *pfc, FILE *f,
	int stack, struct pfScope *scope, struct pfParse *elIxPp, 
	struct pfParse *collectionPp, boolean reverse)
/* This highly technical routine generates some of the code for
 * foreach and para actions.  */
{
struct pfBaseType *base = collectionPp->ty->base;
struct dyString *elName;
struct dyString *keyName = NULL;
struct pfParse *elPp, *ixPp = NULL;

if (elIxPp->type == pptKeyVal)
    {
    ixPp = elIxPp->children;
    elPp = ixPp->next;
    keyName = varName(pfc,ixPp->var);
    assert(base != pfc->indexRangeType);  /* Type checker prevents this */
    }
else
    elPp = elIxPp;

elName = varName(pfc, elPp->var);

if (base == pfc->indexRangeType)
    {
    struct pfParse *fromPp = collectionPp->children;
    struct pfParse *toPp = fromPp->next;
    fprintf(f, "{\n");
    fprintf(f, "{\n");	/* Extra brace so compatible with other collections */
    codeScopeVars(pfc, f, scope, FALSE);
    fprintf(f, "long _pf_offset, _pf_endOffset;\n");
    codeExpression(pfc, f, fromPp, stack, FALSE);
    fprintf(f, "_pf_offset = ");
    codeParamAccess(pfc, f, pfc->longType, stack);
    fprintf(f, ";\n");
    codeExpression(pfc, f, toPp, stack, FALSE);
    fprintf(f, "_pf_endOffset = ");
    codeParamAccess(pfc, f, pfc->longType, stack);
    fprintf(f, ";\n");
    if (reverse)
        fprintf(f, "for (%s=_pf_endOffset-1; %s>=_pf_offset; --%s)\n",
	    elName->string, elName->string, elName->string);
    else
	fprintf(f, "for (%s=_pf_offset; %s<_pf_endOffset; ++%s)\n",
	    elName->string, elName->string, elName->string);
    fprintf(f, "{\n");
    }
else
    {
    /* Get local copy of collection in new scope. */
    fprintf(f, "{\n");
    codeBaseType(pfc, f, collectionPp->ty->base);
    fprintf(f, " _pf_collection;\n");
    codeExpression(pfc, f, collectionPp, stack, FALSE);
    codeNilCheck(f, collectionPp, stack);
    fprintf(f, "_pf_collection = ");
    codeParamAccess(pfc, f, collectionPp->ty->base, stack);
    fprintf(f, ";\n");

    /* Print element variable in a new scope. */
    fprintf(f, "{\n");
    codeScopeVars(pfc, f, scope, TRUE);
    if (base == pfc->arrayType)
	{
	struct pfBaseType *elBase = collectionPp->ty->children->base;
	fprintf(f, "int _pf_offset;\n");
	fprintf(f, "int _pf_elSize = _pf_collection->elSize;\n");
	fprintf(f, "int _pf_endOffset = _pf_collection->size * _pf_elSize;\n");
	if (keyName != NULL)
	    {
	    if (reverse)	/* To help simulate parallelism, do it in reverse. */
		fprintf(f, "for (%s = _pf_collection->size-1, _pf_offset=_pf_endOffset-_pf_elSize; _pf_offset >= 0; %s -= 1, _pf_offset -= _pf_elSize)\n", keyName->string, keyName->string);
	    else
		fprintf(f, "for (%s=0, _pf_offset=0; _pf_offset<_pf_endOffset; _pf_offset += _pf_elSize, %s+=1)\n", keyName->string, keyName->string);
	    }
	else
	    {
	    if (reverse)	/* To help simulate parallelism, do it in reverse. */
		fprintf(f, "for (_pf_offset=_pf_endOffset-_pf_elSize; _pf_offset >= 0; _pf_offset -= _pf_elSize)\n");
	    else
		fprintf(f, "for (_pf_offset=0; _pf_offset<_pf_endOffset; _pf_offset += _pf_elSize)\n");
	    }

	fprintf(f, "{\n");
	fprintf(f, "%s = *((", elName->string);
	codeBaseType(pfc, f, elBase);
	fprintf(f, "*)(_pf_collection->elements + _pf_offset));\n");
	if (elBase->needsCleanup)
	    fprintf(f, "if (0 != %s) %s->_pf_refCount+=1;\n", elName->string,
		elName->string);
	}
    else if (base == pfc->stringType || base == pfc->dyStringType)
	{
	char *ixName = "_pf_offset";
	if (keyName != NULL)
	    ixName = keyName->string;
	else
	    fprintf(f, "int %s;\n", ixName);
	fprintf(f, "int _pf_endOffset = _pf_collection->size;\n");
	if (reverse)	/* To help simulate parallelism, do it in reverse. */
	    fprintf(f, "for (%s=_pf_endOffset-1; %s>=0; %s -= 1)\n", ixName, ixName, ixName);
	else
	    fprintf(f, "for (%s=0; %s<_pf_endOffset; %s += 1)\n", ixName, ixName, ixName);
	fprintf(f, "{\n");
	fprintf(f, "%s = _pf_collection->s[%s];\n", elName->string, ixName);
	}
    else if (base == pfc->dirType)
	{
	fprintf(f, "char *_pf_key;\n");
	fprintf(f, 
	    "struct _pf_iterator _pf_ix = _pf_dir_iterator_init(_pf_collection);\n");
	fprintf(f, "while (_pf_ix.next(&_pf_ix, &%s, &_pf_key))\n", elName->string);
	fprintf(f, "{\n");
	if (keyName != NULL)
	    {
	    fprintf(f, "%s = _pf_string_from_const(_pf_key);\n", keyName->string);
	    }
	}
    else
	{
	internalErr();
	}
    }
dyStringFree(&elName);
dyStringFree(&keyName);
}

static void saveBackToCollection(struct pfCompile *pfc, FILE *f,
	int stack, struct pfParse *elPp, struct pfParse *collectionPp)
/* Save altered value of element back to collection. */
{
struct pfBaseType *base = collectionPp->ty->base;
struct dyString *elName = varName(pfc, elPp->var);
if (elPp->ty->base->needsCleanup)
    fprintf(f, "%s->_pf_refCount += 1;\n", elName->string);
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


static void endElInCollectionIteration(struct pfCompile *pfc, FILE *f,
	struct pfScope *scope, struct pfParse *elIxPp, 
	struct pfParse *collectionPp, boolean reverse)
/* This highly technical routine generates some of the code for
 * foreach and para actions.  */
{
struct pfBaseType *base = collectionPp->ty->base;
struct dyString *keyName = NULL;
struct pfParse *elPp, *ixPp = NULL;

if (elIxPp->type == pptKeyVal)
    {
    ixPp = elIxPp->children;
    elPp = ixPp->next;
    keyName = varName(pfc,ixPp->var);
    assert(base != pfc->indexRangeType);  /* Type checker prevents this */
    }
else
    elPp = elIxPp;
if (base == pfc->arrayType)
    {
    struct pfBaseType *elBase = collectionPp->ty->children->base;
    if (elBase->needsCleanup)
	{
	struct dyString *elName;
	if (elPp->type == pptKeyVal)
	    elPp = elPp->children->next;
	elName = varName(pfc, elPp->var);
	codeCleanupVarNamed(pfc, f, elPp->var->ty, elName->string);
	dyStringFree(&elName);
	}
    }
else if (base == pfc->dirType)
    {
    if (keyName != NULL)
	{
	codeCleanupVar(pfc, f, ixPp->var);
	}
    }
fprintf(f, "}\n");
if (base == pfc->dirType)
    {
    fprintf(f, "_pf_ix.cleanup(&_pf_ix);\n");
    }
fprintf(f, "}\n");
fprintf(f, "}\n");
}

static void setBestIx(struct pfCompile *pfc, FILE *f,
    struct pfParse *elPp, struct pfBaseType *colBase)
{
fprintf(f, "_pf_best_ix = ");
if (colBase == pfc->dirType)
    fprintf(f, "_pf_key;\n");
else if (colBase == pfc->indexRangeType)
    {
    struct dyString *elName = varName(pfc, elPp->var);
    fprintf(f, " %s;\n", elName->string);
    dyStringFree(&elName);
    }
else if (colBase == pfc->arrayType)
    {
    fprintf(f, " (_pf_offset/_pf_elSize);\n");
    }
else
    {
    internalErr();
    }
}

static void updateArgMinMax(struct pfCompile *pfc, FILE *f,
    struct pfParse *elPp, struct pfBaseType *base, struct pfBaseType *colBase,
    int stack, enum pfParseType paraType)
/* Generate code for paraArgMin/paraArgMax */
{
fprintf(f, "if (");
codeParamAccess(pfc, f, base, stack);
if (paraType == pptParaArgMin)
    fprintf(f, " < ");
else
    fprintf(f, " > ");
fprintf(f, "_pf_acc)\n");
fprintf(f, "{\n");
fprintf(f, "_pf_acc = ");
codeParamAccess(pfc, f, base, stack);
fprintf(f, ";\n");
setBestIx(pfc, f, elPp, colBase);

fprintf(f, "}\n");
}

static int codeParaExpSingle(struct pfCompile *pfc, FILE *f,
	struct pfParse *para, int stack)
/* Generate code for a para expression that just has a single
 * value as a result. */
{
struct pfParse *collectionPp = para->children;
struct pfParse *elPp = collectionPp->next;
struct pfParse *body = elPp->next;
struct pfBaseType *base = para->ty->base;
struct pfBaseType *colBase = collectionPp->ty->base;
boolean isArgType = FALSE;

switch (para->type)
    {
    case pptParaArgMin:
    case pptParaArgMax:
        isArgType = TRUE;
	base = elPp->ty->base;
	break;
    }

fprintf(f, "/* Start %s */\n", pfParseTypeAsString(para->type));
fprintf(f, "{\n");
fprintf(f, "int _pf_first = 1;\n");
codeBaseType(pfc, f, base);
fprintf(f, " _pf_acc = 0;\n");

if (isArgType)
     {
     struct pfBaseType *colBase = collectionPp->ty->base;
     if (colBase == pfc->dirType)
	 fprintf(f, "char *_pf_best_ix = 0;\n");
     else
	 fprintf(f, "long _pf_best_ix = -1;\n");
     }

startElInCollectionIteration(pfc, f, stack, 
	para->scope, elPp, collectionPp, TRUE);

/* Calculate expression */
codeExpression(pfc, f, body, stack, FALSE);

/* In all cases first time through we just initialize accumalator with
 * expression value. */
fprintf(f, "if (_pf_first)\n");
fprintf(f, "{\n");
fprintf(f, "_pf_first = 0;\n");
fprintf(f, "_pf_acc = ");
codeParamAccess(pfc, f,base, stack);
fprintf(f, ";\n");
if (isArgType)
    setBestIx(pfc, f, elPp, colBase);
fprintf(f, "}\n");

/* What we do rest of time depends on the type of para. */
fprintf(f, "else\n");
fprintf(f, "{\n");
switch (para->type)
    {
    case pptParaMin:
	 fprintf(f, "if (");
	 codeParamAccess(pfc, f, base, stack);
	 fprintf(f, " < _pf_acc) _pf_acc = ");
	 codeParamAccess(pfc, f, base, stack);
	 fprintf(f, ";\n");
         break;
    case pptParaMax:
	 fprintf(f, "if (");
	 codeParamAccess(pfc, f, base, stack);
	 fprintf(f, " > _pf_acc) _pf_acc = ");
	 codeParamAccess(pfc, f, base, stack);
	 fprintf(f, ";\n");
         break;
    case pptParaArgMin:
    case pptParaArgMax:
	 updateArgMinMax(pfc, f, elPp, base, colBase, stack, para->type);
	 break;
    case pptParaAdd:
	 fprintf(f, "_pf_acc += ");
	 codeParamAccess(pfc, f, base, stack);
	 fprintf(f, ";\n");
         break;
    case pptParaMultiply:
	 fprintf(f, "_pf_acc *= ");
	 codeParamAccess(pfc, f, base, stack);
	 fprintf(f, ";\n");
         break;
    case pptParaAnd:
         fprintf(f, "_pf_acc &= ");
	 codeParamAccess(pfc, f, base, stack);
	 fprintf(f, ";\n");
	 break;
    case pptParaOr:
         fprintf(f, "_pf_acc |= ");
	 codeParamAccess(pfc, f, base, stack);
	 fprintf(f, ";\n");
	 break;
    default:
        internalErr();
	break;
    }
fprintf(f, "}\n");

endElInCollectionIteration(pfc, f, para->scope, elPp, collectionPp, TRUE);

codeParamAccess(pfc, f, para->ty->base, stack);
if (isArgType)
     {
     struct pfBaseType *colBase = collectionPp->ty->base;
     if (colBase == pfc->dirType)
	 fprintf(f, " = _pf_string_from_const(_pf_best_ix);\n");
     else
	 fprintf(f, " = _pf_best_ix;\n");
     }
else
    {
    fprintf(f, " = _pf_acc;\n");
    }
fprintf(f, "}\n");
fprintf(f, "/* End %s */\n", pfParseTypeAsString(para->type));

return 1;
}

static void codeRangeIntoStartEndSize(struct pfCompile *pfc,
	FILE *f, struct pfParse *indexRange, int stack)
/* Generate code to created _pf_start/_pf_end/_pf_size variables
 * with values filled in */
{
struct pfParse *startPp = indexRange->children;
struct pfParse *endPp = startPp->next;
fprintf(f, "long _pf_start, _pf_end, _pf_size;\n");
fprintf(f, "_pf_Array _pf_result;\n");
codeExpression(pfc, f, startPp, stack, FALSE);
fprintf(f, "_pf_start = %s[%d].Long;\n", stackName, stack);
codeExpression(pfc, f, endPp, stack, FALSE);
fprintf(f, "_pf_end = %s[%d].Long;\n", stackName, stack);
fprintf(f, "_pf_size = _pf_end - _pf_start;\n");
fprintf(f, "if (_pf_size <= 0)\n");
codeRunTimeError(f, indexRange, "no items in range");
}

static int codeParaGet(struct pfCompile *pfc, FILE *f,
	struct pfParse *para, int stack)
/* Generate code for a para get expression */
{
struct pfParse *collection = para->children;
struct pfParse *element = collection->next;
struct pfParse *expression = element->next;
struct pfBaseType *collectBase = collection->ty->base;

fprintf(f, "/* start para get */\n");
fprintf(f, "{\n");

if (collectBase == pfc->arrayType || collectBase == pfc->indexRangeType)
    {
    fprintf(f, "int _pf_resElSize, _pf_resOffset = 0;\n");
    if (collectBase == pfc->arrayType)
	{
	fprintf(f, "_pf_Array _pf_coll, _pf_result;\n");
	codeExpression(pfc, f, collection, stack, FALSE);
	fprintf(f, "_pf_coll = ");
	codeParamAccess(pfc, f, collectBase, stack);
	fprintf(f, ";\n");
	fprintf(f, "_pf_result = _pf_dim_array(_pf_coll->size, ");
	codeForType(pfc, f, expression->ty);
	fprintf(f, ");\n");
	}
    else
	{
	codeRangeIntoStartEndSize(pfc, f, collection, stack);
	fprintf(f, "_pf_result = _pf_dim_array(_pf_size, ");
	codeForType(pfc, f, expression->ty);
	fprintf(f, ");\n");
	}
    fprintf(f, "_pf_resElSize = _pf_result->elSize;\n");
    startElInCollectionIteration(pfc, f, stack, para->scope, element, 
    	collection, FALSE);
    codeExpression(pfc, f, expression, stack, TRUE);
    fprintf(f, "*((");
    codeBaseType(pfc, f, expression->ty->base);
    fprintf(f, "*)(_pf_result->elements + _pf_resOffset)) = ");
    codeParamAccess(pfc, f, expression->ty->base, stack);
    fprintf(f, ";\n");
    fprintf(f, "_pf_resOffset += _pf_resElSize;\n");
    endElInCollectionIteration(pfc, f, para->scope, element, 
    	collection, FALSE);
    }
else if (collectBase == pfc->dirType)
    {
    fprintf(f, "_pf_Dir _pf_coll, _pf_result;\n");
    codeExpression(pfc, f, collection, stack, FALSE);
    fprintf(f, "_pf_coll = ");
    codeParamAccess(pfc, f, collectBase, stack);
    fprintf(f, ";\n");
    fprintf(f, "_pf_result = _pf_dir_new(_pf_dir_size(_pf_coll), ");
    codeForType(pfc, f, expression->ty);
    fprintf(f, ");\n");

    startElInCollectionIteration(pfc, f, stack, para->scope, element, 
    	collection, FALSE);
    codeExpression(pfc, f, expression, stack, TRUE);
    if (expression->ty->base->needsCleanup)
	{
	fprintf(f, "_pf_dir_add_obj(_pf_result, _pf_key, %s+%d);\n",  
		stackName, stack);
	}
    else 
	{
	fprintf(f, "_pf_dir_add_num(_pf_result, _pf_key, %s+%d);\n",  
		stackName, stack);
	}
    endElInCollectionIteration(pfc, f, para->scope, element, 
    	collection, FALSE);
    }
else
    {
    internalErr();
    }


codeParamAccess(pfc, f, collectBase, stack);
fprintf(f, " = _pf_result;\n");
fprintf(f, "}\n");
fprintf(f, "/* end para get */\n");
return 1;
}

static int codeParaFilter(struct pfCompile *pfc, FILE *f,
	struct pfParse *para, int stack)
/* Generate code for a para filter expression */
{
struct pfParse *collection = para->children;
struct pfParse *element = collection->next;
struct pfParse *expression = element->next;
struct pfBaseType *collectBase = collection->ty->base;
struct dyString *elName = varName(pfc, element->var);

fprintf(f, "/* start para filter */\n");
fprintf(f, "{\n");
if (collectBase == pfc->arrayType || collectBase == pfc->indexRangeType)
    {
    /* Generate code that will create an array of chars
     * filled with 1's where filter is passed, and 0's elsewhere. */
    fprintf(f, "int _pf_ix=0,_pf_passCount=0,_pf_passOne,_pf_resOffset=0;\n");
    fprintf(f, "char *_pf_passed;\n");
    if (collectBase == pfc->arrayType)
	{
	fprintf(f, "_pf_Array _pf_result;\n");
	fprintf(f, "_pf_Array _pf_coll;\n");
	codeExpression(pfc, f, collection, stack, FALSE);
	fprintf(f, "_pf_coll = ");
	codeParamAccess(pfc, f, collectBase, stack);
	fprintf(f, ";\n");
	fprintf(f, "_pf_passed = _pf_need_mem(_pf_coll->size);\n");
	}
    else
        {
	fprintf(f, "int _pf_elSize = sizeof(_pf_Long);\n");
	codeRangeIntoStartEndSize(pfc, f, collection, stack);
	fprintf(f, "_pf_passed = _pf_need_mem(_pf_size);\n");
	}

    startElInCollectionIteration(pfc, f, stack, para->scope, element, 
    	collection, FALSE);
    codeExpression(pfc, f, expression, stack, FALSE);
    fprintf(f, "_pf_passOne = ");
    codeParamAccess(pfc, f, pfc->bitType, stack);
    fprintf(f, ";\n");
    fprintf(f, "_pf_passed[_pf_ix++] = _pf_passOne;\n");
    fprintf(f, "_pf_passCount += _pf_passOne;\n");
    endElInCollectionIteration(pfc, f, para->scope, element, 
    	collection, FALSE);

    /* Allocate results array. */
    fprintf(f, "_pf_result = _pf_dim_array(_pf_passCount, ");
    if (collectBase == pfc->arrayType)
	codeForType(pfc, f, collection->ty->children);
    else
        codeForType(pfc, f, pfc->longFullType);
    fprintf(f, ");\n");

    /* Generate code that will copy passing elements to results */
    fprintf(f, "_pf_passCount = 0;\n");
    fprintf(f, "_pf_ix = 0;\n");
    startElInCollectionIteration(pfc, f, stack, para->scope, element, 
    	collection, FALSE);
    fprintf(f, "if (_pf_passed[_pf_ix++])\n");
    fprintf(f, "{\n");
    if (element->ty->base->needsCleanup)
        fprintf(f, "%s->_pf_refCount+=1;\n", elName->string);
    fprintf(f, "*((");
    codeBaseType(pfc, f, element->ty->base);
    fprintf(f, "*)(_pf_result->elements + _pf_resOffset))");
    fprintf(f, "=%s;\n", elName->string);
    fprintf(f, "_pf_resOffset += _pf_elSize;\n");
    fprintf(f, "}\n");
    endElInCollectionIteration(pfc, f, para->scope, element, 
    	collection, FALSE);

    /* Generate clean up code. */
    fprintf(f, "_pf_free_mem(_pf_passed);\n");
    }
else if (collectBase == pfc->dirType)
    {
    struct pfType *elType = element->ty;
    fprintf(f, "_pf_Dir _pf_coll, _pf_result;\n");
    codeExpression(pfc, f, collection, stack, FALSE);
    fprintf(f, "_pf_coll = ");
    codeParamAccess(pfc, f, collectBase, stack);
    fprintf(f, ";\n");
    fprintf(f, "_pf_result = _pf_dir_new(_pf_dir_size(_pf_coll), ");
    codeForType(pfc, f, elType);
    fprintf(f, ");\n");

    startElInCollectionIteration(pfc, f, stack, para->scope, element, 
    	collection, FALSE);
    codeExpression(pfc, f, expression, stack, TRUE);
    fprintf(f, "if (");
    codeParamAccess(pfc, f, pfc->bitType, stack);
    fprintf(f, ")\n");
    fprintf(f, "{\n");
    codeParamAccess(pfc, f, elType->base, stack);
    fprintf(f, "=%s;\n", elName->string);
    if (elType->base->needsCleanup)
	{
        fprintf(f, "%s->_pf_refCount+=1;\n", elName->string);
	fprintf(f, "_pf_dir_add_obj(_pf_result, _pf_key, %s+%d);\n",  
		stackName, stack);
	}
    else 
	{
	fprintf(f, "_pf_dir_add_num(_pf_result, _pf_key, %s+%d);\n",  
		stackName, stack);
	}
    fprintf(f, "}\n");

    endElInCollectionIteration(pfc, f, para->scope, element, 
    	collection, FALSE);
    }
else
    internalErr();
codeParamAccess(pfc, f, collectBase, stack);
fprintf(f, " = _pf_result;\n");
fprintf(f, "}\n");
fprintf(f, "/* end para filter */\n");
dyStringFree(&elName);
return 1;
}

static void startCleanTemp(FILE *f)
/* Declare a temp variable to assist in cleanup */
{
fprintf(f, " {\n struct _pf_object *_pf_tmp = (struct _pf_object *)\n  ");
}

static void endCleanTemp(struct pfCompile *pfc, FILE *f, struct pfType *type)
{
fprintf(f, ";\n");
fprintf(f, " if (_pf_tmp != 0 && --_pf_tmp->_pf_refCount <= 0)\n");
fprintf(f, "   _pf_tmp->_pf_cleanup(_pf_tmp, ");
codeForType(pfc, f, type);
fprintf(f, ");\n");
fprintf(f, " }\n");
}

static void bumpStackRefCount(struct pfCompile *pfc,
	FILE *f, struct pfType *type, int stack)
/* If type is reference counted, bump up refCount of top of stack. */
{
struct pfBaseType *base = type->base;

if (base == pfc->varType)
    {
    fprintf(f, "_pf_var_link(");
    codeStackAccess(f, stack);
    fprintf(f, ".Var");
    fprintf(f, ");\n");
    }
else if (base->needsCleanup)
    {
    fprintf(f, "if (0 != ");
    codeStackAccess(f, stack);
    fprintf(f, ".Obj) ");
    codeStackAccess(f, stack);
    fprintf(f, ".Obj->_pf_refCount+=1;\n");
    }
}

static int pushArrayIndexAndBoundsCheck(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Put collection and index on stack.  Check index is in range. 
 * Return offset to index. */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfBaseType *colBase = collection->ty->base;
struct pfParse *index = collection->next;

/* Push array and index onto expression stack */
int offset = codeExpression(pfc, f, collection, stack, FALSE);
codeNilCheck(f, collection, stack);
codeExpression(pfc, f, index, stack+offset, FALSE);

/* Do bounds checking */
fprintf(f, "if (");
codeParamAccess(pfc, f, pfc->intType, stack+offset);
fprintf(f, "< 0 || ");
codeParamAccess(pfc, f, pfc->intType, stack+offset);
fprintf(f, " >= ");
codeParamAccess(pfc, f, colBase, stack);
fprintf(f, "->size)\n  ");
codeRunTimeError(f, pp, "array access out of bounds");
return offset;
}

static void codeArrayAccess(struct pfCompile *pfc, FILE *f,
	struct pfBaseType *base, int stack, int indexOffset)
/* Print out code to access array (on either left or right
 * side */
{
fprintf(f, "((");
codeBaseType(pfc, f, base);
fprintf(f, "*)(");
codeParamAccess(pfc, f, pfc->arrayType, stack);
fprintf(f, "->elements + %d * ",  base->size);
codeParamAccess(pfc, f, pfc->intType, stack+indexOffset);
fprintf(f, "))[0]");
}

static void codeAccessToByteInString(struct pfCompile *pfc, FILE *f,
	struct pfBaseType *base, int stack, int indexOffset)
/* Print out code to a byte in string. */
{
codeParamAccess(pfc, f, pfc->stringType, stack);
fprintf(f, "->s[");
codeParamAccess(pfc, f, pfc->intType, stack+indexOffset);
fprintf(f, "]");
}


static int codeIndexRval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Generate code for index expression (not on left side of
 * assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfParse *index = collection->next;
struct pfBaseType *colBase = collection->ty->base;
if (colBase == pfc->arrayType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, stack);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, " = ");
    codeArrayAccess(pfc, f, outType->base, stack, indexOffset);
    fprintf(f, ";\n");
    if (addRef) 
    	bumpStackRefCount(pfc, f, outType, stack);
    }
else if (colBase == pfc->stringType || colBase == pfc->dyStringType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, stack);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, " = ");
    codeAccessToByteInString(pfc, f, outType->base, stack, indexOffset);
    fprintf(f, ";\n");
    }
else if (colBase == pfc->dirType)
    {
    int offset = codeExpression(pfc, f, collection, stack, FALSE);
    codeExpression(pfc, f, index, stack+offset, FALSE);
    if (outType->base->needsCleanup)
	{
	fprintf(f, "%s[%d].Obj", stackName, stack);
	fprintf(f, " = ");
        fprintf(f, "_pf_dir_lookup_object(%s+%d, %d);\n", stackName, stack,
		addRef);
	}
    else 
	{
	fprintf(f, "_pf_dir_lookup_number(%s+%d);\n",  stackName, stack);
	}
    }
else
    {
    internalErr();
    }
return 1;
}

static void codeIndexLval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op, int expSize, 
	boolean cleanupOldVal)
/* Generate code for index expression  on left side of assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfParse *index = collection->next;
struct pfBaseType *colBase = collection->ty->base;
int emptyStack = stack + expSize;
if (colBase == pfc->arrayType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, emptyStack);
    if (outType->base == pfc->varType)
        {
	fprintf(f, "_pf_var_cleanup(");
	codeArrayAccess(pfc, f, outType->base, emptyStack, indexOffset);
	fprintf(f, ");\n");
	}
    else if (outType->base->needsCleanup)
        {
	startCleanTemp(f);
	codeArrayAccess(pfc, f, outType->base, emptyStack, indexOffset);
	endCleanTemp(pfc, f, outType);
	}
    codeArrayAccess(pfc, f, outType->base, emptyStack, indexOffset);
    fprintf(f, " %s ", op);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, ";\n");
    }
else if (colBase == pfc->dyStringType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, emptyStack);
    codeAccessToByteInString(pfc, f, outType->base, emptyStack, indexOffset);
    fprintf(f, " %s ", op);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, ";\n");
    }
else if (colBase == pfc->stringType)
    {
    internalErr();  /* Type checker prevents this. */
    }
else if (colBase == pfc->dirType)
    {
    int offset = codeExpression(pfc, f, collection, emptyStack, FALSE);
    codeExpression(pfc, f, index, emptyStack+offset, FALSE);
    if (outType->base->needsCleanup)
	{
        fprintf(f, "_pf_dir_add_object(%s+%d, %d);\n", stackName, stack, expSize);
	}
    else 
	{
	/* We're doing something like:
	 *     wordCount['the'] += 1;
	 * or more abstractly:
	 *     collection[index] op expression;
	 * The stack currently contains:
	 *     expression collection index. */
	if (!sameString(op, "="))
	    {
	    fprintf(f, "_pf_dir_lookup_number(%s+%d);\n", stackName, 
	    	emptyStack);
	    codeParamAccess(pfc, f, outType->base, stack);
	    fprintf(f, " %s ", op);
	    codeParamAccess(pfc, f, outType->base, emptyStack);
	    fprintf(f, ";\n");
	    offset = codeExpression(pfc, f, collection, emptyStack, FALSE);
	    codeExpression(pfc, f, index, emptyStack+offset, FALSE);
	    }
	fprintf(f, "_pf_dir_add_number(%s+%d, %d);\n",  stackName, stack, expSize);
	}
    }
else
    {
    internalErr();
    }
}

static void codeDotAccess(struct pfCompile *pfc, FILE *f, 
	struct pfParse *pp, int stack)
/* Print out code to access field (on either left or right
 * side */
{
struct pfParse *class = pp->children;
struct pfParse *field = class->next;
struct pfBaseType *base = class->ty->base;
if (base == pfc->stringType || base == pfc->dyStringType || base == pfc->arrayType)
    fprintf(f, "(");
else
    fprintf(f, "((struct %s *)", base->cName);
codeParamAccess(pfc, f, base, stack);
fprintf(f, ")");
assert(field->next == NULL);
fprintf(f, "->%s", field->name);
field = field->next;
}
	

static int codeDotRval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for . expression (not on left side of
 * assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *class = pp->children;
codeExpression(pfc, f, class, stack, FALSE);
codeNilCheck(f, class, stack);
codeParamAccess(pfc, f, outType->base, stack);
fprintf(f, " = ");
codeDotAccess(pfc, f, pp, stack);
fprintf(f, ";\n");
bumpStackRefCount(pfc, f, outType, stack);
return 1;
}


static void codeDotLval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op, int expSize, 
	boolean cleanupOldVal)
/* Generate code for dot expression  on left side of assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *class = pp->children;
struct pfParse *field = class->next;
int emptyStack = stack + expSize;
codeExpression(pfc, f, class, emptyStack, FALSE);
codeNilCheck(f, class, emptyStack);
if (outType->base->needsCleanup)
    {
    startCleanTemp(f);
    codeDotAccess(pfc, f, pp, emptyStack);
    endCleanTemp(pfc, f, outType);
    }
codeDotAccess(pfc, f, pp, emptyStack);
fprintf(f, " %s ", op);
codeParamAccess(pfc, f, outType->base, stack);
fprintf(f, ";\n");
}

static void codeZeroInitOfType(struct pfCompile *pfc, FILE *f, struct pfType *type)
/* Print out default initialization of type. */
{
if (type->base == pfc->varType)
    fprintf(f, "=_pf_var_zero");
else
    fprintf(f, "=0");
}

static void codeVarsInHelList(struct pfCompile *pfc, FILE *f,
	struct hashEl *helList, boolean zeroUninit, boolean isModuleScope)
/* Print out variable declarations in helList. */
{
struct hashEl *hel;
boolean gotVar = FALSE;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct pfVar *var = hel->val;
    struct pfType *type = var->ty;
    if ((type->tyty == tytyVariable || type->tyty == tytyFunctionPointer)
	&& type->access != paStatic)
	{
	if (var->isExternal)
	    fprintf(f, "extern ");
	if (pfc->isFunc)
	    {
	    if (zeroUninit && !var->isExternal && !isInitialized(var))
		{
		printVarName(pfc, f, var);
		codeZeroInitOfType(pfc, f, type);
		fprintf(f, ";\n");
		}
	    }
	else
	    {
	    enum pfAccessType access = var->ty->access;
	    if (access != paGlobal && access != paWritable && isModuleScope)
	         fprintf(f, "static ");
	    codeBaseType(pfc, f, type->base);
	    fprintf(f, " ");
	    printVarName(pfc, f, var);
	    if (zeroUninit && !var->isExternal && !isInitialized(var))
		{
		codeZeroInitOfType(pfc, f, type);
		}
	    fprintf(f, ";\n");
	    }
	gotVar = TRUE;
	}
    }
}

static void codeCleanupVarsInHelList(struct pfCompile *pfc, FILE *f,
	struct hashEl *helList)
/* Print out variable cleanups for helList. */
{
struct hashEl *hel;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct pfVar *var = hel->val;
    if (!var->isExternal && var->ty->access != paStatic)
	codeCleanupVar(pfc, f, var);
    }
}

static void codeScopeVars(struct pfCompile *pfc, FILE *f, struct pfScope *scope,
	boolean zeroUninitialized)
/* Print out variable declarations associated with scope. */
{
struct hashEl *helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);
codeVarsInHelList(pfc, f, helList, zeroUninitialized, FALSE);
hashElFreeList(&helList);
}

static void cleanupScopeVars(struct pfCompile *pfc, FILE *f, 
	struct pfScope *scope)
/* Print out variable declarations associated with scope. */
{
struct hashEl *helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);
slReverse(&helList);
codeCleanupVarsInHelList(pfc, f, helList);
hashElFreeList(&helList);
}



static int lvalOffStack(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op, int expSize,
	boolean cleanupOldVal)
/* Take an lval off of stack. */
{
switch (pp->type)
    {
    case pptVarUse:
    case pptVarInit:
	{
	struct pfBaseType *base = pp->ty->base;
	if (cleanupOldVal && base->needsCleanup)
	    codeCleanupVar(pfc, f, pp->var);
	printVarName(pfc, f, pp->var);
	fprintf(f, " %s ", op);
	codeParamAccess(pfc, f, base, stack);
	fprintf(f, ";\n");
	return 1;
	}
    case pptTuple:
        {
	int total = 0;
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
            {
	    total += lvalOffStack(pfc, f, p, stack+total, op, expSize-total,
	    	cleanupOldVal);
	    }
	return total;
	}
    case pptIndex:
        {
	codeIndexLval(pfc, f, pp, stack, op, expSize, cleanupOldVal);
	return 1;
	}
    case pptDot:
        {
	codeDotLval(pfc, f, pp, stack, op, expSize, cleanupOldVal);
	return 1;
	}
    default:
        fprintf(f, "using %s %s as an lval\n", pp->name, pfParseTypeAsString(pp->type));
	internalErr();
	return 0;
    }
}

static void rCodeTupleType(struct pfCompile *pfc, FILE *f, struct pfType *type)
/* Recursively encode tuple type to output. */
{
struct pfType *t;
fprintf(f, "(");
for (t = type->children; t != NULL; t = t->next)
    {
    struct pfType *tk = t;
    if (t->base == pfc->keyValType)
        tk = t->children->next;
    if (tk->tyty == tytyTuple)
        rCodeTupleType(pfc, f, tk);
    else
        fprintf(f, "x");
    }
fprintf(f, ")");
}

static void codeTupleIntoCollection(struct pfCompile *pfc, FILE *f,
	struct pfType *type, struct pfParse *rval, int stack, int tupleSize)
/* Shovel tuple into array, list, dir, tree. */
{
struct pfBaseType *base = type->base;
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = ");
if (base == pfc->arrayType // ||  base == pfc->listType || base == pfc->treeType
	|| base == pfc->dirType)
    {
    struct pfBaseType *base = type->children->base;
    if (base == pfc->bitType || base == pfc->byteType || base == pfc->shortType
	|| base == pfc->intType || base == pfc->longType || base == pfc->floatType
	|| base == pfc->doubleType || base == pfc->charType 
	|| base == pfc->stringType  || base == pfc->dyStringType
	|| base == pfc->varType)
	{
	fprintf(f, "_pf_%s_%s_from_tuple(%s+%d, %d, ",
		base->name, type->base->name, stackName, stack, tupleSize);
	codeForType(pfc, f, type), 
	fprintf(f, ", ");
	codeForType(pfc, f, type->children);
	fprintf(f, ");\n");
	}
    else
	{
	fprintf(f, "_pf_tuple_to_%s(%s+%d, ", type->base->name,
		stackName, stack);
	codeForType(pfc, f, type->children);
	fprintf(f, ", \"");
	rCodeTupleType(pfc, f, rval->ty);
	fprintf(f, "\");\n");
	}
    }
else
    {
    internalErr();
    }
}

static void codeTupleIntoClass(struct pfCompile *pfc, FILE *f,
	struct pfParse *typePp, struct pfParse *tuplePp, 
	int stack, int tupleSize)
/* Shovel tuple into class. */
{
// TODO - add this to function call as well? 
struct pfBaseType *base = typePp->ty->base;
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = ");
fprintf(f, "_pf_tuple_to_class(%s+%d, ",
	stackName, stack);
codeForType(pfc, f, typePp->ty);
fprintf(f, ", \"");
rCodeTupleType(pfc, f, tuplePp->ty);
fprintf(f, "\");\n");
}

static int codeInitOrAssign(struct pfCompile *pfc, FILE *f,
	struct pfParse *lval, struct pfParse *rval,
	int stack)
/* Emit code for initialization or assignment. */
{
int count = codeExpression(pfc, f, rval, stack, TRUE);
if (rval->type == pptUniformTuple)
    codeTupleIntoCollection(pfc, f, lval->ty, rval, stack, count);
if (lval->type == pptTuple)
    return slCount(lval->children);
else
    return 1;
}

static void cantCopeParamType(struct pfParse *pp, int code)
/* Explain that can't deal with parameterized type complications.
 * Hopefully this is temporary. */
{
errAt(pp->tok, "Don't know how to deal with this parameterized type (code %d)", 
	code);
}

static boolean isArraysOnAcross(struct pfCompile *pfc, struct pfParse *ppOf)
/* Chase children of a pptOf node.  If these are all pptTypeNames,
 * and all but the last is base type array, then return TRUE. */
{
struct pfParse *type;
for (type = ppOf->children; type != NULL; type = type->next)
    {
    if (type->next == NULL)
	break;
    if (type->ty->base != pfc->arrayType)
        return FALSE;
    }
return TRUE;
}

static boolean checkIfNeedsDims(struct pfCompile *pfc, struct pfParse *type)
/* Return TRUE if it looks like we need to generate initialzation code 
 * for an array even if it isn't assigned anything. */
{
if (type->type != pptOf)
    return FALSE;
if (type->children == NULL)
    internalErr();
if (type->children->children == NULL)
    return FALSE;
if (!isArraysOnAcross(pfc, type))
    errAt(type->tok, "Sorry right now you can only parameterize arrays");
return TRUE;
}


static void codeCheckDims(struct pfCompile *pfc, FILE *f,
        struct pfParse *lval, struct pfParse *rval, int count, int stack)
/* Generate code that checks that the dimensions of the array
 * in the tuple is the same as the dimensions of the array as
 * declared. */
{
uglyf("Theoretically doing codeCheckDims - count %d.\n", count);
}

void codeInitDims(struct pfCompile *pfc, FILE *f, struct pfParse *pp, int stack)
/* Generate code that creates an array that is initialized to
 * zero (as opposed to the empty array). */
{
struct pfParse *ppOf = pp->children;
struct pfParse *sym = ppOf->next;
int dimCount = slCount(ppOf->children) - 1;
if (dimCount == 1)
    {
    struct pfParse *type = ppOf->children;
    codeExpression(pfc, f, type->children, stack, FALSE);
    fprintf(f, "%s[%d].Array = ", stackName, stack);
    fprintf(f, "_pf_dim_array(%s[%d].Long, ", stackName, stack);
    codeForType(pfc, f, type->next->ty);
    fprintf(f, ");\n");
    lvalOffStack(pfc, f, pp, stack, "=", 1, FALSE);
    }
else
    {
    int offset = 0;
    int inittedDims = 0;
    struct pfParse *type;
    for (type = ppOf->children; type != NULL; type = type->next)
        {
	if (type->next == NULL || type->children == NULL)
	    {
	    /* Final type - contains type of array cell.  Output call. */
	    fprintf(f, "%s[%d].Array = ", stackName, stack);
	    fprintf(f, "_pf_multi_dim_array(%s+%d, %d, ", 
	    	stackName, stack, inittedDims);
	    codeForType(pfc, f, ppOf->ty);
	    fprintf(f, ");\n");
	    lvalOffStack(pfc, f, pp, stack, "=", 1, FALSE);
	    break;
	    }
	else
	    {
	    offset += codeExpression(pfc, f, type->children, stack+offset, FALSE);
	    inittedDims += 1;
	    }
	}
    }
}

static int codeAllocInit(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code to allocate memory and initialize a class
 * first by default values in tuple, and then by init method. */
{
struct pfParse *defaultTuple = pp->children;
struct pfParse *initTuple = defaultTuple->next;
struct pfBaseType *base = pp->ty->base;
int offset = codeExpression(pfc, f, defaultTuple, stack, TRUE);
fprintf(f, "{\n");
fprintf(f, "struct _pf_object *_pf_obj = ");
fprintf(f, "_pf_tuple_to_class(%s+%d, ", stackName, stack);
codeForType(pfc, f, pp->ty);
fprintf(f, ", \"");
rCodeTupleType(pfc, f, defaultTuple->ty);
fprintf(f, "\");\n");
fprintf(f, "%s[%d].Obj = _pf_obj;\n", stackName, stack);
codeExpression(pfc, f, initTuple, stack+1, TRUE);
codeMethodName(pfc, pp->tok, f, base, "init", stack);
fprintf(f, "(%s+%d);\n", stackName, stack);
fprintf(f, "%s[%d].Obj = _pf_obj;\n", stackName, stack);
fprintf(f, "}\n");
return 1;
}

static int codeNewOp(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for new operator. */
{
struct pfParse *typePp = pp->children;
struct pfParse *initPp = typePp->next;
return codeExpression(pfc, f, initPp, stack, TRUE);
}

static void codeVarInit(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for an assignment. */
{
struct pfParse *lval = pp;
struct pfParse *type = pp->children;
struct pfParse *name = type->next;
struct pfParse *rval = name->next;
boolean gotDims = checkIfNeedsDims(pfc, type);
if (rval != NULL)
    {
    int count = codeInitOrAssign(pfc, f, lval, rval, stack);
    lvalOffStack(pfc, f, lval, stack, "=", count, FALSE);
    if (gotDims)
	codeCheckDims(pfc, f, lval, rval, count, stack);
    }
else if (gotDims)
    codeInitDims(pfc, f, pp, stack);
}

static int codeAssignment(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Generate code for an assignment. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;

if (lval->type == pptTuple && rval->type == pptTuple)
    {
    for (lval = lval->children, rval = rval->children; 
    	lval != NULL; lval = lval->next, rval = rval->next)
        {
	int count = codeInitOrAssign(pfc, f, lval, rval, stack);
	lvalOffStack(pfc, f, lval, stack, op, count, TRUE);
	}
    }
else
    {
    int count = codeInitOrAssign(pfc, f, lval, rval, stack);
    lvalOffStack(pfc, f, lval, stack, op, count, TRUE);
    }
return 0;
}

static int codeVarUse(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Generate code for moving a variable onto stack. */
{
struct pfBaseType *base = pp->ty->base;
struct dyString *name = varName(pfc, pp->var);
if (addRef && base->needsCleanup)
    {
    if (base == pfc->varType)
	fprintf(f, "_pf_var_link(%s);\n", name->string);
    else
	{
	fprintf(f, "if (0 != %s) ", name->string);
	fprintf(f, "%s->_pf_refCount+=1;\n", name->string);
	}
    }
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = %s;\n", name->string);
dyStringFree(&name);
return 1;
}
	
static int codeBinaryOp(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Emit code for some sort of binary op. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;
codeExpression(pfc, f, lval, stack, TRUE);
codeExpression(pfc, f, rval, stack+1, TRUE);
codeParamAccess(pfc, f, pp->ty->base, stack);
fprintf(f, " = ");
if (lval->ty->base == pfc->stringType || lval->ty->base == pfc->dyStringType)
    {
    fprintf(f, " (_pf_strcmp(%s+%d) %s 0);\n", stackName, stack, op);
    }
else
    {
    codeParamAccess(pfc, f, lval->ty->base, stack);
    fprintf(f, " %s ", op);
    codeParamAccess(pfc, f, rval->ty->base, stack+1);
    fprintf(f, ";\n");
    }
return 1;
}

static int codeUnaryOp(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Emit code for some sort of unary op. */
{
struct pfParse *child = pp->children;
struct pfBaseType *base = child->ty->base;
codeExpression(pfc, f, child, stack, TRUE);
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = ");
fprintf(f, "%s", op);
codeParamAccess(pfc, f, base, stack);
fprintf(f, ";\n");
return 1;
}

static int codeStringCat(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Generate code for a string concatenation. */
{
int total = 0;
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    total += codeExpression(pfc, f, p, stack+total, addRef);
codeParamAccess(pfc, f, pp->ty->base, stack);
fprintf(f, " = ");
fprintf(f, "_pf_string_cat(%s+%d, %d);\n", stackName, stack, total);
return 1;
}

static void castStack(struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	int stack);
/* Cast stack location. */


static void castCallToTuple(struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	int stack)
/* Cast function call results, which are on stack, to types specified by
 * cast list. */
{
struct pfParse *call = pp->children;
struct pfParse *castList = call->next;
struct pfParse *cast;
for (cast = castList; cast != NULL; cast = cast->next)
    {
    if (cast->type != pptPlaceholder)
	castStack(pfc, f, cast, stack);
    stack += 1;
    }
}

static void printLocalInterfaceMethodAssignments(struct pfCompile *pfc, 
	FILE *f, struct pfParse *pp, struct pfBaseType *classBase, 
	struct pfToken *tok, int stack)
/* Print assignments of methods declared in this interface. */
{
switch (pp->type)
    {
    case pptFlowDec:
    case pptToDec:
	fprintf(f, "_pf_face->%s = ", pp->name);
	codeMethodName(pfc, tok, f, classBase, pp->name, stack);
	fprintf(f, ";\n");
        break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    printLocalInterfaceMethodAssignments(pfc, f, pp, classBase, tok, stack);
}

static void printInterfaceMethodAssignments(struct pfCompile *pfc, FILE *f,
	struct pfBaseType *faceBase, struct pfBaseType *classBase, 
	struct pfToken *tok, int stack)
/* Print assignments of methods to interface, starting with
 * parent interface. */
{
if (faceBase->parent != NULL && faceBase->parent->def != NULL)
    {
    printInterfaceMethodAssignments(pfc, f, faceBase->parent, classBase, 
    	tok, stack);
    }
printLocalInterfaceMethodAssignments(pfc, f, faceBase->def, classBase, 
	tok, stack);
}

static void castClassToInterface(struct pfCompile *pfc, FILE *f, 
	struct pfParse *pp, int stack)
/* Generate code to allocate an interface type, fill in it's methods
 * and fill in it's object pointer with what's on the stack. */
{
struct pfBaseType *faceBase = pp->ty->base;
char *faceName = faceBase->cName;
struct pfParse *classPp = pp->children;
struct pfBaseType *classBase = classPp->ty->base;
char *className = classBase->cName;
fprintf(f, "{\n");
fprintf(f, "struct %s *_pf_face = _pf_need_mem(sizeof(struct %s));\n", 
	faceName, faceName);
fprintf(f, "struct _pf_object *_pf_obj = %s[%d].Obj;\n", stackName, stack);
fprintf(f, "if (_pf_obj == 0)\n");
codeUseOfNil(f, pp);
fprintf(f, "_pf_face->_pf_refCount = 1;\n");
fprintf(f, "_pf_face->_pf_cleanup = _pf_cleanup_interface;\n");
fprintf(f, "_pf_face->_pf_obj = _pf_obj;\n");
fprintf(f, "_pf_face->_pf_objTypeId = ");
codeForType(pfc, f, classPp->ty);
fprintf(f, ";\n");
printInterfaceMethodAssignments(pfc, f, faceBase, classBase, pp->tok, stack);
fprintf(f, "%s[%d].v = _pf_face;\n", stackName, stack);
fprintf(f, "}\n");
}

static void castStack(struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	int stack)
/* Cast stack location. */
{
switch(pp->type)
    {
    case pptCastBitToBit:
    case pptCastBitToByte:
    case pptCastBitToChar:
    case pptCastBitToShort:
    case pptCastBitToInt:
    case pptCastBitToLong:
    case pptCastBitToFloat:
    case pptCastBitToDouble:
    case pptCastByteToByte:
    case pptCastByteToChar:
    case pptCastByteToShort:
    case pptCastByteToInt:
    case pptCastByteToLong:
    case pptCastByteToFloat:
    case pptCastByteToDouble:
    case pptCastCharToByte:
    case pptCastCharToChar:
    case pptCastCharToShort:
    case pptCastCharToInt:
    case pptCastCharToLong:
    case pptCastCharToFloat:
    case pptCastCharToDouble:
    case pptCastShortToByte:
    case pptCastShortToChar:
    case pptCastShortToShort:
    case pptCastShortToInt:
    case pptCastShortToLong:
    case pptCastShortToFloat:
    case pptCastShortToDouble:
    case pptCastIntToByte:
    case pptCastIntToChar:
    case pptCastIntToShort:
    case pptCastIntToInt:
    case pptCastIntToLong:
    case pptCastIntToFloat:
    case pptCastIntToDouble:
    case pptCastLongToByte:
    case pptCastLongToChar:
    case pptCastLongToShort:
    case pptCastLongToInt:
    case pptCastLongToLong:
    case pptCastLongToFloat:
    case pptCastLongToDouble:
    case pptCastFloatToByte:
    case pptCastFloatToChar:
    case pptCastFloatToShort:
    case pptCastFloatToInt:
    case pptCastFloatToLong:
    case pptCastFloatToFloat:
    case pptCastFloatToDouble:
    case pptCastDoubleToByte:
    case pptCastDoubleToChar:
    case pptCastDoubleToShort:
    case pptCastDoubleToInt:
    case pptCastDoubleToLong:
    case pptCastDoubleToFloat:
    case pptCastDoubleToDouble:
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ";\n");
	break;
    case pptCastByteToBit:
    case pptCastCharToBit:
    case pptCastShortToBit:
    case pptCastIntToBit:
    case pptCastLongToBit:
    case pptCastFloatToBit:
    case pptCastDoubleToBit:
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, " != 0");
	fprintf(f, ";\n");
	break;
    case pptCastObjectToBit:
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = (0 != ");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ");\n");
	break;
    case pptCastStringToBit:
        {
	fprintf(f, "{_pf_String _pf_tmp = %s[%d].String; ", stackName, stack);
	fprintf(f, "%s[%d].Bit = (_pf_tmp != 0 && _pf_tmp->size != 0);", stackName, stack);
	fprintf(f, "}\n");
	break;
	}
    case pptCastBitToString:
    case pptCastByteToString:
    case pptCastShortToString:
    case pptCastIntToString:
    case pptCastLongToString:
    case pptCastFloatToString:
    case pptCastDoubleToString:
	{
	char *dest;
	if (pp->type == pptCastFloatToString || pp->type == pptCastDoubleToString)
	    dest = "double";
	else
	    dest = "long";
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = _pf_string_from_%s(", dest);
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ");\n");
	break;
	}
    case pptCastCharToString:
        {
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = _pf_string_new((char*)&");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ",1);\n");
	break;
	}
    case pptCastTypedToVar:
	{
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".val.%s = ", vTypeKey(pfc, pp->children->ty->base));
	codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ";\n");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".typeId = ");
	codeForType(pfc, f, pp->children->ty);
	fprintf(f, ";\n");
	break;
	}
    case pptCastVarToTyped:
        {
	fprintf(f, "if (");
	codeForType(pfc, f, pp->ty);
	fprintf(f, " != ");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".typeId)\n");
	fprintf(f, "if (!_pf_check_types(");
	codeForType(pfc, f, pp->ty);
	fprintf(f, ", ");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".typeId))\n");
	codeRunTimeError(f, pp, "run-time type mismatch");
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".val.%s;\n", vTypeKey(pfc, pp->ty->base));
	break;
	}
    case pptCastCallToTuple:
        castCallToTuple(pfc, f, pp, stack);
	break;
    case pptCastClassToInterface:
        castClassToInterface(pfc, f, pp, stack);
	break;
    default:
	{
	internalErr();
	break;
	}
    }
}

static int codeStringAppend(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Emit code to append to a string. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;

codeExpression(pfc, f, lval, stack, FALSE);
fprintf(f, "if (%s[%d].String == 0)\n", stackName, stack);
codeRunTimeError(f, lval, "string is nil");
codeExpression(pfc, f, rval, stack+1, TRUE);
fprintf(f, "_pf_cm_string_append(%s+%d);\n", stackName, stack);
return 0;
}

static int codeTupleExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Code tuple as expression.  This just shoves each item
 * on tuple on stack one at at time. */
{
int total = 0;
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    total += codeExpression(pfc, f, p, stack+total, addRef);
return total;
}

int codeExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Emit code for one expression.  Returns how many items added
 * to stack. */
{
switch (pp->type)
    {
    case pptTuple:
    case pptUniformTuple:
    case pptKeyVal:
        return codeTupleExpression(pfc, f, pp, stack, addRef);
    case pptClassAllocFromTuple:
        {
	int count = codeTupleExpression(pfc, f, pp->children, stack, addRef);
	codeTupleIntoClass(pfc, f, pp, pp->children, stack, count);
	return 1;
	}
    case pptClassAllocFromInit:
	return codeAllocInit(pfc, f, pp, stack);
    case pptNew:
        return codeNewOp(pfc, f, pp, stack);
    case pptParaMin:
    case pptParaMax:
    case pptParaArgMin:
    case pptParaArgMax:
    case pptParaAdd:
    case pptParaMultiply:
    case pptParaAnd:
    case pptParaOr:
        return codeParaExpSingle(pfc, f, pp, stack);
    case pptParaGet:
        return codeParaGet(pfc, f, pp, stack);
    case pptParaFilter:
        return codeParaFilter(pfc, f, pp, stack);
    case pptCall:
	return codeCall(pfc, f, pp, stack);
    case pptIndirectCall:
        return codeIndirectCall(pfc, f, pp, stack);
    case pptAssignment:
	return codeAssignment(pfc, f, pp, stack, "=");
    case pptPlusEquals:
	if (pp->ty->base == pfc->dyStringType)
	    return codeStringAppend(pfc, f, pp, stack);
	else if (pp->ty->base == pfc->stringType)
	    internalErr();  /* Type checker should prevent this. */
	else
	    return codeAssignment(pfc, f, pp, stack, "+=");
	break;
    case pptMinusEquals:
	return codeAssignment(pfc, f, pp, stack, "-=");
    case pptMulEquals:
	return codeAssignment(pfc, f, pp, stack, "*=");
    case pptDivEquals:
	return codeAssignment(pfc, f, pp, stack, "/=");
    case pptVarInit:
    	codeVarInit(pfc, f, pp, stack);
	return 0;
    case pptVarUse:
	return codeVarUse(pfc, f, pp, stack, addRef);
    case pptIndex:
         return codeIndexRval(pfc, f, pp, stack, addRef);
    case pptDot:
         return codeDotRval(pfc, f, pp, stack);
    case pptPlus:
        return codeBinaryOp(pfc, f, pp, stack, "+");
    case pptMinus:
        return codeBinaryOp(pfc, f, pp, stack, "-");
    case pptMul:
        return codeBinaryOp(pfc, f, pp, stack, "*");
    case pptDiv:
        return codeBinaryOp(pfc, f, pp, stack, "/");
    case pptMod:
        return codeBinaryOp(pfc, f, pp, stack, "%");
    case pptGreater:
        return codeBinaryOp(pfc, f, pp, stack, ">");
    case pptGreaterOrEquals:
        return codeBinaryOp(pfc, f, pp, stack, ">=");
    case pptSame:
        return codeBinaryOp(pfc, f, pp, stack, "==");
    case pptNotSame:
        return codeBinaryOp(pfc, f, pp, stack, "!=");
    case pptLess:
        return codeBinaryOp(pfc, f, pp, stack, "<");
    case pptLessOrEquals:
        return codeBinaryOp(pfc, f, pp, stack, "<=");
    case pptLogAnd:
        return codeBinaryOp(pfc, f, pp, stack, "&&");
    case pptLogOr:
        return codeBinaryOp(pfc, f, pp, stack, "||");
    case pptBitAnd:
        return codeBinaryOp(pfc, f, pp, stack, "&");
    case pptBitOr:
        return codeBinaryOp(pfc, f, pp, stack, "|");
    case pptShiftLeft:
        return codeBinaryOp(pfc, f, pp, stack, "<<");
    case pptShiftRight:
        return codeBinaryOp(pfc, f, pp, stack, ">>");
    case pptNegate:
         return codeUnaryOp(pfc, f, pp, stack, "-");
    case pptFlipBits:
         return codeUnaryOp(pfc, f, pp, stack, "~");
    case pptNot:
         return codeUnaryOp(pfc, f, pp, stack, "!");
    case pptConstBit:
    case pptConstByte:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
        {
	struct pfToken *tok = pp->tok;
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
	switch (tok->type)
	    {
	    case pftString:
		assert(pp->type == pptConstBit);
	        fprintf(f, "1;\n");
		break;
	    case pftFloat:
	        fprintf(f, "%f;\n", pp->tok->val.x);
		break;
	    case pftLong:
		fprintf(f, "%lldLL;\n", pp->tok->val.l);
		break;
	    case pftInt:
		fprintf(f, "%d;\n", pp->tok->val.i);
		break;
	    }
	return 1;
	}
    case pptConstChar:
        {
	assert(pp->tok->type == pftString || pp->tok->type == pftSubstitute);
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = %d;\n", pp->tok->val.s[0]);
	return 1;
	}
    case pptConstString:
        {
	assert(pp->tok->type == pftString || pp->tok->type == pftSubstitute);
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = _pf_string_from_const(");
	printEscapedString(f, pp->tok->val.s);
	fprintf(f, ");\n");
	return 1;
	}
    case pptConstZero:
        {
	if (pp->ty->base == pfc->varType)
	    {
	    fprintf(f, "%s[%d].Var.typeId = 0;\n", stackName, stack);
	    fprintf(f, "%s[%d].Var.val.Int = 0;\n", stackName, stack);
	    }
	else
	    {
	    codeParamAccess(pfc, f, pp->ty->base, stack);
	    fprintf(f, " = 0;\n");
	    }
	return 1;
	}
    case pptSubstitute:
        {
	struct pfParse *source = pp->children;
	struct pfParse *varTuple = source->next;
	int offset;
	offset = codeExpression(pfc, f, source, stack, TRUE);
	codeExpression(pfc, f, varTuple, stack+offset, TRUE);
	fprintf(f, "_pf_string_substitute(%s+%d);\n", stackName, stack);
	return 1;
	}
    case pptStringCat:
        {
	return codeStringCat(pfc, f, pp, stack, addRef);
	}
    case pptStringDupe:
        {
	codeExpression(pfc, f, pp->children, stack, TRUE);
	fprintf(f, "_pf_string_make_independent_copy(%s+%d);\n", 
		stackName, stack);
	return 1;
	}
    case pptCastBitToBit:
    case pptCastBitToByte:
    case pptCastBitToChar:
    case pptCastBitToShort:
    case pptCastBitToInt:
    case pptCastBitToLong:
    case pptCastBitToFloat:
    case pptCastBitToDouble:
    case pptCastByteToBit:
    case pptCastByteToByte:
    case pptCastByteToChar:
    case pptCastByteToShort:
    case pptCastByteToInt:
    case pptCastByteToLong:
    case pptCastByteToFloat:
    case pptCastByteToDouble:
    case pptCastCharToBit:
    case pptCastCharToByte:
    case pptCastCharToChar:
    case pptCastCharToShort:
    case pptCastCharToInt:
    case pptCastCharToLong:
    case pptCastCharToFloat:
    case pptCastCharToDouble:
    case pptCastShortToBit:
    case pptCastShortToByte:
    case pptCastShortToChar:
    case pptCastShortToShort:
    case pptCastShortToInt:
    case pptCastShortToLong:
    case pptCastShortToFloat:
    case pptCastShortToDouble:
    case pptCastIntToBit:
    case pptCastIntToByte:
    case pptCastIntToChar:
    case pptCastIntToShort:
    case pptCastIntToInt:
    case pptCastIntToLong:
    case pptCastIntToFloat:
    case pptCastIntToDouble:
    case pptCastLongToBit:
    case pptCastLongToByte:
    case pptCastLongToChar:
    case pptCastLongToShort:
    case pptCastLongToInt:
    case pptCastLongToLong:
    case pptCastLongToFloat:
    case pptCastLongToDouble:
    case pptCastFloatToBit:
    case pptCastFloatToByte:
    case pptCastFloatToChar:
    case pptCastFloatToShort:
    case pptCastFloatToInt:
    case pptCastFloatToLong:
    case pptCastFloatToFloat:
    case pptCastFloatToDouble:
    case pptCastDoubleToBit:
    case pptCastDoubleToByte:
    case pptCastDoubleToChar:
    case pptCastDoubleToShort:
    case pptCastDoubleToInt:
    case pptCastDoubleToLong:
    case pptCastDoubleToFloat:
    case pptCastDoubleToDouble:
    case pptCastObjectToBit:
    case pptCastStringToBit:
    case pptCastBitToString:
    case pptCastByteToString:
    case pptCastCharToString:
    case pptCastShortToString:
    case pptCastIntToString:
    case pptCastLongToString:
    case pptCastFloatToString:
    case pptCastDoubleToString:
    case pptCastTypedToVar:
    case pptCastVarToTyped:
    case pptCastCallToTuple:
    case pptCastClassToInterface:
	{
	codeExpression(pfc, f, pp->children, stack, addRef);
	castStack(pfc, f, pp, stack);
	return 1;
	}
    case pptCastFunctionToPointer:
        {
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = %s%s;\n", globalPrefix, pp->children->name);
	return 1;
	}
    default:
	{
	fprintf(f, "(%s expression)\n", pfParseTypeAsString(pp->type));
	return 0;
	}
    }
assert(FALSE);
return  0;
}

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

static void codeForeach(struct pfCompile *pfc, FILE *f,
	struct pfParse *foreach, boolean isPara)
/* Emit C code for foreach statement. */
{
struct pfParse *collectionPp = foreach->children;
struct pfParse *elPp = collectionPp->next;
struct pfParse *body = elPp->next;
struct pfParse *elVarPp = NULL;

startElInCollectionIteration(pfc, f, 0, 
	foreach->scope, elPp, collectionPp, isPara);
codeStatement(pfc, f, body);
if (isPara)
    {
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
    }
endElInCollectionIteration(pfc, f, foreach->scope, elPp, collectionPp, isPara);
}

static void codeFor(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for for statement. */
{
struct pfParse *init = pp->children;
struct pfParse *end = init->next;
struct pfParse *next = end->next;
struct pfParse *body = next->next;

fprintf(f, "{\n");
codeScopeVars(pfc, f, pp->scope, TRUE);
codeStatement(pfc, f, init);
fprintf(f, "for(;;)\n");
fprintf(f, "{\n");
if (next->type != pptNop)
    {
    codeExpression(pfc, f, end, 0, TRUE);
    fprintf(f, "if (!");
    codeParamAccess(pfc, f, pfc->bitType, 0);
    fprintf(f, ") break;\n");
    }
codeStatement(pfc, f, body);
codeStatement(pfc, f, next);
fprintf(f, "}\n");
fprintf(f, "}\n");
}

static void codeForEachCall(struct pfCompile *pfc, FILE *f, struct pfParse *foreach)
/* Emit C code for foreach call statement. */
{
struct pfParse *callPp = foreach->children;
struct pfParse *elPp = callPp->next;
struct pfParse *body = elPp->next;
struct pfParse *cast = body->next;
struct pfBaseType *base = callPp->ty->base;
int expSize;

/* Print element variable in a new scope. */
fprintf(f, "{\n");
codeScopeVars(pfc, f, foreach->scope, TRUE);
fprintf(f, "for(;;)\n");
fprintf(f, "{\n");
expSize = codeInitOrAssign(pfc, f, elPp, callPp, 0);
lvalOffStack(pfc, f, elPp, 0, "=", expSize, TRUE);
if (cast != NULL)
    castStack(pfc, f, cast, 0);
fprintf(f, "if (%s[0].Bit == 0)", stackName);
fprintf(f, "   break;\n");
codeStatement(pfc, f, body);
fprintf(f, "}\n");
cleanupScopeVars(pfc, f, foreach->scope);
fprintf(f, "}\n");
}


static void codeWhile(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for while statement. */
{
struct pfParse *end = pp->children;
struct pfParse *body = end->next;
fprintf(f, "for(;;)\n");
fprintf(f, "{\n");
codeExpression(pfc, f, end, 0, TRUE);
fprintf(f, "if (!");
codeParamAccess(pfc, f, pfc->bitType, 0);
fprintf(f, ") break;\n");
codeStatement(pfc, f, body);
fprintf(f, "}\n");
}

static void codeIf(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for if statement. */
{
struct pfParse *cond = pp->children;
struct pfParse *trueBody = cond->next;
struct pfParse *falseBody = trueBody->next;
codeExpression(pfc, f, cond, 0, TRUE);
fprintf(f, "if (");
codeParamAccess(pfc, f, pfc->bitType, 0);
fprintf(f, ")\n");
fprintf(f, "{\n");
codeStatement(pfc, f, trueBody);
fprintf(f, "}\n");
if (falseBody != NULL)
    {
    fprintf(f, "else\n");
    fprintf(f, "{\n");
    codeStatement(pfc, f, falseBody);
    fprintf(f, "}\n");
    }
}

static void codeTry(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for try statement. */
{
struct pfParse *tryBody = pp->children;
struct pfParse *catchVars = tryBody->next;
struct pfParse *catchBody = catchVars->next;
struct pfParse *messageVar = catchVars->children;
struct pfParse *sourceVar = messageVar->next;
struct pfParse *levelVar = sourceVar->next;
struct dyString *messageVarName = varName(pfc, messageVar->var);
struct dyString *sourceVarName = varName(pfc, sourceVar->var);
struct dyString *levelVarName = varName(pfc, levelVar->var);

fprintf(f, "{\n");
if (!pfc->isFunc)
    fprintf(f, "int pf_catchLevel;\n");
codeStatement(pfc, f, levelVar);
if (pfc->isFunc)
    fprintf(f, "_pf_Err_catch _pf_err = _pf_err_catch_new(&_pf_act, %s);\n",
	    levelVarName->string);
else
    fprintf(f, "_pf_Err_catch _pf_err = _pf_err_catch_new(0, %s);\n",
	    levelVarName->string);

fprintf(f, "if (_pf_err_catch_start(_pf_err))\n");
fprintf(f, "{\n");
codeStatement(pfc, f, tryBody);
fprintf(f, "}\n");
fprintf(f, "_pf_err_catch_end(_pf_err);\n");
fprintf(f, "if (_pf_err_catch_got_err(_pf_err))\n");
fprintf(f, "{\n");
fprintf(f, "  /* catch block should start here. */\n");
if (!pfc->isFunc)
    {
    fprintf(f, "_pf_String %s = 0;\n", messageVarName->string);
    fprintf(f, "_pf_String %s = 0;\n", sourceVarName->string);
    }
fprintf(f, "{\n");
fprintf(f, "%s = _pf_string_from_const(_pf_err->message);\n",
	messageVarName->string);
fprintf(f, "%s = _pf_string_from_const(_pf_err->source);\n",
	sourceVarName->string);
codeStatement(pfc, f, catchBody);
fprintf(f, "}\n");
fprintf(f, "  /* catch block should end here. */\n");
fprintf(f, "}\n");
fprintf(f, "_pf_err_catch_free(&_pf_err);\n");
fprintf(f, "}\n");
dyStringFree(&levelVarName);
dyStringFree(&sourceVarName);
dyStringFree(&messageVarName);
}

static void codeArrayAppend(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for array append . */
{
struct pfParse *array = pp->children;
struct pfParse *element = array->next;
int stack;
stack = codeExpression(pfc, f, array, 0, FALSE);
codeExpression(pfc, f, element, stack, TRUE);
fprintf(f, "_pf_array_append(%s[0].Array, &", stackName);
codeParamAccess(pfc, f, element->ty->base, stack);
fprintf(f, ");\n");
}

void codeStatement(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for one statement. */
{
switch (pp->type)
    {
    case pptCompound:
        {
	fprintf(f, "{\n");
	codeScope(pfc, f, pp, FALSE, NULL);
	fprintf(f, "}\n");
	break;
	}
    case pptCall:
    case pptIndirectCall:
    case pptAssignment:
    case pptPlusEquals:
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
        codeExpression(pfc, f, pp, 0, TRUE);
	break;
    case pptTuple:
        {
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    codeStatement(pfc, f, p);
#ifdef OLD
	internalErrAt(pp->tok);
#endif /* OLD */
	break;
	}
    case pptVarInit:
	codeVarInit(pfc, f, pp, 0);
        break;
    case pptParaDo:
        codeForeach(pfc, f, pp, TRUE);
	break;
    case pptForeach:
	codeForeach(pfc, f, pp, FALSE);
	break;
    case pptForEachCall:
        codeForEachCall(pfc, f, pp);
	break;
    case pptFor:
	codeFor(pfc, f, pp);
	break;
    case pptWhile:
        codeWhile(pfc, f, pp);
	break;
    case pptCase:
        codeCase(pfc, f, pp);
	break;
    case pptIf:
        codeIf(pfc, f, pp);
	break;
    case pptTry:
        codeTry(pfc, f, pp);
	break;
    case pptArrayAppend:
	codeArrayAppend(pfc, f, pp);
	break;
    case pptNop:
        break;
    case pptBreak:
        fprintf(f, "break;\n");
        break;
    case pptContinue:
        fprintf(f, "continue;\n");
        break;
    case pptReturn:
        fprintf(f, "goto _pf_cleanup;\n");
        break;
    case pptInclude:
    case pptImport:
        fprintf(f, "_pf_entry_%s(%s);\n", mangledModuleName(pp->name), 
		stackName);
	break;
    default:
        fprintf(f, "[%s statement];\n", pfParseTypeAsString(pp->type));
	break;
    }
}

static void printPrototype(FILE *f, struct pfParse *funcDec, struct pfParse *class,
	boolean activeModule, boolean printSemi)
/* Print prototype for function call. */
{
if (class)
    {
    fprintf(f, "void %s_%s(", class->ty->base->methodPrefix, funcDec->name);
    fprintf(f, "%s *%s)",  stackType, stackName);
    }
else
    {
    enum pfAccessType access = funcDec->ty->access;
    if (access != paGlobal)
	{
	if (activeModule)
	    fprintf(f, "static ");
	else
	    return;
	}
    fprintf(f, "void %s%s(%s *%s)", globalPrefix, funcDec->name, 
    	stackType, stackName);
    }
if (printSemi)
    fprintf(f, ";\n");
}

static void rPrintPrototypes(FILE *f, struct pfParse *pp, struct pfParse *class,
	boolean activeModule)
/* Recursively print out function prototypes in C. */
{
switch (pp->type)
    {
    case pptClass:
        class = pp;
	break;
    case pptToDec:
    case pptFlowDec:
	printPrototype(f, pp, class, activeModule, TRUE);
        break;
    }
for (pp=pp->children; pp != NULL; pp = pp->next)
    rPrintPrototypes(f, pp, class, activeModule);
}

static void declareStaticVars(struct pfCompile *pfc, FILE *f, 
	struct pfParse *funcDec, struct pfParse *pp,
	struct pfParse *class)
/* Declare static variables inside of function */
{
if (pp->type == pptVarInit)
    {
    struct pfType *type = pp->ty;
    if (type->access == paStatic)
        {
	fprintf(f, "static ");
	codeBaseType(pfc, f, type->base);
	fprintf(f, " ");
	printVarName(pfc, f, pp->var);
	fprintf(f, ";\n");
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    declareStaticVars(pfc, f, funcDec, pp, class);
}

static void codeFunction(struct pfCompile *pfc, FILE *f, 
	struct pfParse *funcDec, struct pfParse *class)
/* Emit C code for function.  If class is non-null
 * then decorate it with 'self' so as to be a method. */
{
struct pfVar *funcVar = funcDec->var;
struct ctar *ctar = funcDec->var->ctar;
struct pfType *funcType = funcVar->ty;
struct pfType *inTuple = funcType->children;
struct pfType *outTuple = inTuple->next;
struct pfParse *body = funcDec->children->next->next->next;
bool oldIsFunc = pfc->isFunc;

if (body != NULL)
    {
    pfc->isFunc = TRUE;
    declareStaticVars(pfc, f, funcDec, body, class);
    printPrototype(f, funcDec, class, TRUE, FALSE);
    fprintf(f, "\n{\n");

    /* Print out activation record. */
	{
	ctarCodeLocalStruct(ctar, pfc, f);
	ctarCodePush(ctar, pfc, f);
	}

    /* Print out input parameters. */
	{
	struct pfType *in;
	int inIx = 0;
	if (class)
	    {
	    fprintf(f, "%sself = ", localPrefix);
	    codeParamAccess(pfc, f, class->ty->base, 0);
	    fprintf(f, ";\n");
	    inIx += 1;
	    if (pfBaseIsDerivedClass(class->ty->base))
		{
		fprintf(f, "%sparent = ", localPrefix);
		codeParamAccess(pfc, f, class->ty->base, 0);
		fprintf(f, ";\n");
		/* No +1 here, we just reuse self as parent. */
		}
	    }
	for (in = inTuple->children; in != NULL; in = in->next)
	    {
	    fprintf(f, "%s%s = ", localPrefix, in->fieldName);
	    codeParamAccess(pfc, f, in->base, inIx);
	    fprintf(f, ";\n");
	    inIx += 1;
	    }
	}

    /* Print out output parameters. */
	{
	/* Nothing to do here, since did bulk local zero init. */
	}

    /* Print out body (which is a compound statement) */
    codeStatement(pfc, f, body);

    /* Print exit label for returns. */
    fprintf(f, "_pf_cleanup: ;\n");

    /* Decrement ref counts on input variables. */
	{
	struct pfType *in;
	struct dyString *name = dyStringNew(0);
	for (in = inTuple->children; in != NULL; in = in->next)
	    {
	    dyStringClear(name);
	    dyStringPrintf(name, "%s%s", localPrefix, in->fieldName);
	    codeCleanupVarNamed(pfc, f, in, name->string);
	    }
	dyStringFree(&name);
	}

    /* Save the output. */
	{
	int outIx = 0;
	struct pfType *out;
	for (out = outTuple->children; out != NULL; out = out->next)
	    {
	    codeParamAccess(pfc, f, out->base, outIx);
	    fprintf(f, " = %s%s;\n", localPrefix, out->fieldName);
	    outIx += 1;
	    }
	}

    /* Close out function. */
    ctarCodePop(ctar, pfc, f);
    fprintf(f, "}\n\n");
    pfc->isFunc = oldIsFunc;
    }
}

static void printPolyFunTable(struct pfCompile *pfc, FILE *f, 
	struct pfBaseType *base)
/* Print polymorphic function table. */
{
if (base->polyList != NULL)
    {
    struct pfPolyFunRef *pfr;
    fprintf(f, "static _pf_polyFunType _pf_pf%d_%s[] = {\n", base->scope->id, base->name);
    for (pfr = base->polyList; pfr != NULL; pfr = pfr->next)
        {
	struct pfBaseType *b = pfr->class;
	fprintf(f, "  %s_%s,\n", b->methodPrefix,
		pfr->method->fieldName);
	}
    fprintf(f, "};\n");
    }
}

static void codeStaticCleanups(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Print out any static assignments in parse tree. */
{
if (pp->type == pptVarInit && pp->ty->access == paStatic)
    codeCleanupVar(pfc, f, pp->var);
for (pp = pp->children; pp != NULL; pp = pp->next)
    codeStaticCleanups(pfc, f, pp);
}

static void codeStaticInits(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Print out any static assignments in parse tree. */
{
if (pp->type == pptVarInit && pp->ty->access == paStatic)
    codeStatement(pfc, f, pp);
for (pp = pp->children; pp != NULL; pp = pp->next)
    codeStaticInits(pfc, f, pp);
}


static void rPrintClasses(struct pfCompile *pfc, FILE *f, struct pfParse *pp,
	boolean printPolyFun)
/* Print out class definitions. */
{
if (pp->type == pptClass)
    {
    struct pfBaseType *base = pp->ty->base;
    fprintf(f, "struct %s {\n", base->cName);
    fprintf(f, "int _pf_refCount;\n");
    fprintf(f, "void (*_pf_cleanup)(struct %s *obj, int typeId);\n", 
    	base->cName);
    fprintf(f, "_pf_polyFunType *_pf_polyFun;\n");
    rPrintFields(pfc, f, base); 
    fprintf(f, "};\n");
    if (printPolyFun)
	printPolyFunTable(pfc, f, base);
    fprintf(f, "\n");
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rPrintClasses(pfc, f, pp, printPolyFun);
}

static void rPrintFuncPointersInInterface(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Print out function declarations in parse tree as function pointers. */
{
switch (pp->type)
    {
    case pptToDec:
    case pptFlowDec:
	fprintf(f, "void (*%s)(%s *%s);\n", pp->name, stackType, stackName);
        break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rPrintFuncPointersInInterface(pfc, f, pp);
}

static void rPrintInterfaceElements(struct pfCompile *pfc, FILE *f, struct pfBaseType *base)
/* Print out function pointer declarations for all elements in interface and it's parents. */
{
/* Print out elements from parent interfaces if any first. */
if (base->parent != NULL && base->parent->def != NULL)
    rPrintInterfaceElements(pfc, f, base->parent);
rPrintFuncPointersInInterface(pfc, f, base->def);
}

static void rPrintInterfaces(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Print out interface definition. */
{
if (pp->type == pptInterface)
     {
     struct pfBaseType *base = pp->ty->base;
     struct pfParse *ppType = pp->children;
     struct pfParse *ppCompound = ppType->next;
     fprintf(f, "struct %s {\n", base->cName);
     fprintf(f, "int _pf_refCount;\n");
     fprintf(f, "void (*_pf_cleanup)(void *obj, int typeId);\n");
     fprintf(f, "void *_pf_obj;\n");
     fprintf(f, "int _pf_objTypeId;\n");
     rPrintInterfaceElements(pfc, f, base);
     fprintf(f, "};\n\n");
     }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rPrintInterfaces(pfc, f, pp);
}


static boolean isInside(struct pfParse *outside, struct pfParse *inside)
/* Return TRUE if inside is a child of outside. */
{
while (inside != NULL)
    {
    if (inside == outside)
	return TRUE;
    inside = inside->parent;
    }
return FALSE;
}

static void rPrintFuncDeclarations(struct pfCompile *pfc, FILE *f, 
	struct pfParse *pp, struct pfParse *class)
/* Print out function declarations. */
{
switch (pp->type)
    {
    case pptToDec:
    case pptFlowDec:
	codeFunction(pfc, f, pp, class);
	break;
    case pptClass:
        class = pp;
	break;
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rPrintFuncDeclarations(pfc, f, pp, class);
}

static void codeAllButStaticInits(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Code everything *but* the static initializations. */
{
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    {
    switch (p->type)
        {
	case pptToDec:
	case pptFlowDec:
	case pptNop:
	case pptClass:
	case pptInterface:
	    break;
	case pptTuple:
	    codeAllButStaticInits(pfc, f, p);
	    break;
	case pptVarInit:
	    if (p->access != paStatic)
		codeStatement(pfc, f, p);
	    break;
	default:
	    codeStatement(pfc, f, p);
	    break;
	}
    }
}

static void codeScope(struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean isModuleScope, struct ctar *ctarList)
/* Print types and then variables from scope. */
{
struct pfScope *scope = pp->scope;
struct hashEl *hel, *helList;
struct pfParse *p;
boolean gotFunc = FALSE, gotVar = FALSE;


/* Get declaration list and sort it. */
helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);

/* Print out variables. */
if (isModuleScope)
    {
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	struct pfVar *var = hel->val;
	var->isExternal = !isInside(pp, var->parse);
	}
    }
codeVarsInHelList(pfc, f, helList, !isModuleScope, isModuleScope);

/* Print out function declarations */
rPrintFuncDeclarations(pfc, f, pp, NULL);

/* Print out other statements */
if (isModuleScope)
    {
    fprintf(f, "void _pf_entry_%s(%s *%s)\n{\n", mangledModuleName(pp->name), 
    	stackType, stackName);
    fprintf(f, "static int firstTime = 1;\n");
    fprintf(f, "if (firstTime)\n");
    fprintf(f, "{\n");
    fprintf(f, "firstTime = 0;\n");
    fprintf(f, "%s.next = %s;\n", moduleRuntimeName, moduleRuntimeList);
    fprintf(f, "%s = &%s;\n", moduleRuntimeList, moduleRuntimeName);
    ctarCodeStartupCall(ctarList, pfc, f);
    codeStaticInits(pfc, f, pp);
    }
codeAllButStaticInits(pfc, f, pp);

if (isModuleScope)
    {
    fprintf(f, "}\n");
    fprintf(f, "}\n\n");

    printModuleCleanupPrototype(f);
    fprintf(f,"\n{\n");
    codeCleanupVarsInHelList(pfc, f, helList);
    codeStaticCleanups(pfc, f, pp);
    fprintf(f, "}\n");
    }
else
    {
    /* Print out any needed cleanups. */
    codeCleanupVarsInHelList(pfc, f, helList);
    }

hashElFreeList(&helList);
}

static void printPolyFuncConnections(struct pfCompile *pfc,
	struct slRef *scopeRefs, struct pfParse *module, FILE *f)
/* Print out poly_info table that connects polymorphic function
 * tables to the classes they belong to. */
{
struct slRef *ref;
fprintf(f, "struct _pf_poly_info _pf_poly_info_%s[] = {\n", 
	mangledModuleName(module->name));
for (ref = scopeRefs; ref != NULL; ref = ref->next)
    {
    struct pfScope *scope = ref->val;
    struct pfBaseType *class = scope->class;
    if (class != NULL && class->polyList != NULL && isInside(module, class->def))
        {
	fprintf(f, "  {\"%s\", _pf_pf%d_%s},\n", class->name, class->scope->id, class->name);
	}
    }
fprintf(f, "  {0, 0},\n");  /* Make sure have at least one. */
fprintf(f, "};\n");
}

static void printConclusion(struct pfCompile *pfc, FILE *f, struct pfParse *mainModule)
/* Print out C code for end of program. */
{
fprintf(f, 
"\n"
"int main(int argc, char *argv[], char *environ[])\n"
"{\n"
"static _pf_Stack stack[16*1024];\n"
"_pf_init_types(_pf_base_info, _pf_base_info_count,\n"
"               _pf_type_info, _pf_type_info_count,\n"
"               _pf_field_info, _pf_field_info_count,\n"
"               _pf_module_info, _pf_module_info_count);\n"
"_pf_init_args(argc, argv, &%sprogramName, &%sargs, environ);\n"
"_pf_punt_init();\n"
"_pf_entry_%s(stack);\n"
"while (%s != 0)\n"
"    {\n"
"    %s->cleanup(%s, stack);\n"
"    %s = %s->next;\n"
"    }\n"
"return 0;\n"
"}\n", 
globalPrefix, globalPrefix, mangledModuleName(mainModule->name),
moduleRuntimeList, moduleRuntimeList, moduleRuntimeList,
moduleRuntimeList, moduleRuntimeList);
}

static void printLocalTypeInfo(struct pfCompile *pfc, char *moduleName, FILE *f)
/* Print out local type info table. */
{
codeLocalTypeTableName(f, moduleName);
fprintf(f, "[] = {\n");
printModuleTypeTable(pfc, f);
fprintf(f, "};\n");
fprintf(f, "\n");
}

static void printModuleTable(struct pfCompile *pfc, FILE *f, struct pfParse *program)
/* Print out table with basic info on each module */
{
struct pfParse *module;
int moduleCount = 0;
for (module = program->children; module != NULL; module = module->next)
    {
    if (module->name[0] != '<')
        {
	char *mangledName = mangledModuleName(module->name);
	fprintf(f, "extern struct %s %s_%s[];\n", 
	    localTypeTableType, localTypeTableName, mangledName);
        fprintf(f, "extern struct _pf_poly_info _pf_poly_info_%s[];\n",
		mangledName);
	fprintf(f, "void _pf_entry_%s(%s *%s);\n", 
		mangledName, stackType, stackName);
	}
    }
fprintf(f, "\n");
fprintf(f, "struct _pf_module_info _pf_module_info[] = {\n");
for (module = program->children; module != NULL; module = module->next)
    {
    if (module->name[0] != '<')
        {
	char *mangledName = mangledModuleName(module->name);
	fprintf(f, "  {\"%s\", %s_%s, _pf_poly_info_%s, _pf_entry_%s,},\n",
	    mangledName, localTypeTableName, mangledName, mangledName,
	    mangledName);
	++moduleCount;
	}
    }
fprintf(f, "};\n");
fprintf(f, "int _pf_module_info_count = %d;\n\n", moduleCount);
}

static void rAddCompileTimeActivationRecords(struct pfParse *pp, 
	struct ctar **pCtar)
/* Print out function declarations. */
{
switch (pp->type)
    {
    case pptToDec:
    case pptFlowDec:
	{
	struct pfParse *body = pp->children->next->next->next;
	if (body)
	    {
	    struct ctar *ctar = ctarOnFunction(pp);
	    slAddHead(pCtar, ctar);
	    }
	break;
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rAddCompileTimeActivationRecords(pp, pCtar);
}

static void printGlobalVarsInScope(struct pfCompile *pfc, FILE *f, 
	struct pfScope *scope)
/* Print all the global variables in scope. */
{
struct hashEl *hel = hel, *varList = hashElListHash(scope->vars);

slSort(&varList, hashElCmp);
for (hel = varList; hel != NULL; hel = hel->next)
    {
    struct pfVar *var = hel->val;
    struct pfType *type = var->ty;
    if (type->tyty == tytyVariable)
        {
	enum pfAccessType access = var->ty->access;
	if (access == paGlobal || access == paWritable)
	    {
	    fprintf(f, "extern ");
	    codeBaseType(pfc, f, type->base);
	    fprintf(f, " ");
	    printVarName(pfc, f, var);
	    fprintf(f, ";\n");
	    }
	}
    }
slFreeList(&varList);
}

void pfCodeC(struct pfCompile *pfc, struct pfParse *program, char *baseDir, 
	char *mainName)
/* Generate C code for program. */
{
FILE *f;
struct pfParse *toCode, *module;
struct pfScope *scope;
struct pfParse *mainModule = NULL;

pfc->runTypeHash = hashNew(0);

/* Generate code for each module that is not already compiled. */
for (toCode = program->children; toCode != NULL; toCode = toCode->next)
    {
    struct ctar *ctarList = NULL;
    if (toCode->type == pptModule || toCode->type == pptMainModule)
	{
	struct pfModule *mod = hashMustFindVal(pfc->moduleHash, toCode->name);
	char *fileName = replaceSuffix(mod->fileName, ".pf", ".c");
	char *moduleName = mangledModuleName(toCode->name);
	if (toCode->type == pptMainModule)
	    mainModule = toCode;
	f = mustOpen(fileName, "w");

	pfc->moduleTypeHash = hashNew(0);
	printPreamble(pfc, f, fileName, TRUE);
	fprintf(f, "extern struct %s %s_%s[];\n", localTypeTableType, localTypeTableName,
		moduleName);
	fprintf(f, "static struct %s *%s = %s_%s;\n\n",
		localTypeTableType, localTypeTableName, localTypeTableName, moduleName);
	printModuleCleanupPrototype(f);
	fprintf(f, ";\n");
	fprintf(f, "static struct %s %s = {0, \"%s\", %s};\n",
		moduleRuntimeType, moduleRuntimeName, moduleName,
		moduleCleanupName);
	fprintf(f, "\n");

	/* Print function prototypes and class definitions for all modules */
	for (module = program->children; module != NULL; module = module->next)
	    {
	    boolean activeModule = (toCode == module);
	    fprintf(f, "/* Prototypes in ParaFlow module %s */\n", module->name);
	    if (module->name[0] != '<')
		fprintf(f, "void _pf_entry_%s(%s *stack);\n", 
			mangledModuleName(module->name), stackType);
	    rPrintPrototypes(f, module, NULL, activeModule);
	    fprintf(f, "\n");
	    fprintf(f, "/* Class definitions in module %s */\n", module->name);
	    rPrintClasses(pfc, f, module, activeModule);
	    fprintf(f, "/* Interface definitions in module %s */\n", module->name);
	    rPrintInterfaces(pfc, f, module);
	    if (module != toCode)
		{
		fprintf(f, "/* Global variables in module %s */\n", module->name);
		printGlobalVarsInScope(pfc, f, module->scope);
		}
	    fprintf(f, "\n");
	    }


	for (module = program->children; module != NULL; module = module->next)
	    {
	    if (module == toCode)
		{
		rAddCompileTimeActivationRecords(module, &ctarList);
		slReverse(&ctarList);
		verbose(3, "Coding %s\n", module->name);
		fprintf(f, "/* ParaFlow module %s */\n\n", module->name);
		fprintf(f, "\n");
		ctarCodeFixedParts(ctarList, pfc, f);
		fprintf(f, "\n");
		codeScope(pfc, f, module, TRUE, ctarList);
		fprintf(f, "\n");
		printPolyFuncConnections(pfc, pfc->scopeRefList, module, f);
		}
	    }
	printLocalTypeInfo(pfc, toCode->name, f);
	freeHashAndVals(&pfc->moduleTypeHash);
	carefulClose(&f);
	freeMem(fileName);
	}
    }
f = mustOpen(mainName, "w");
printPreamble(pfc, f, mainName, FALSE);
printSysVarsAndPrototypes(f);
printModuleTable(pfc, f, program);
codedTypesCalcAndPrintAsC(pfc, program, f);
printConclusion(pfc, f, mainModule);
carefulClose(&f);
}

