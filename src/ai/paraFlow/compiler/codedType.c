/* codedType - Types in form that can be used at run time. */

#include "common.h"
#include "pfType.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "codedType.h"

struct codedBaseType 
/* Base type in form that can be saved in static structure. */
    {
    struct codedBaseType *next;
    int id;	  /* Unique identifier. */
    int scope;	  /* Scope this is declared in. */
    char *name;   /* Class or builtin type name. */
    int parentId; /* Id of parent. */
    bool needsCleanup;	/* True if it's an object or string. */
    bool size;		/* Type size. */
    struct pfBaseType *base;	/* Unencoded version */
    };

struct codedType *codedTypeNew(char *code, struct pfType *type)
/* Make new coded type */
{
static int cotId = 0;
struct codedType *cot;
AllocVar(cot);
cot->id = cotId++;
cot->type = type;
return cot;
}

static void rEncodeType(struct pfType *type, struct dyString *dy)
/* Encode type recursively into dy. */
{
dyStringPrintf(dy, "%s", type->base->name);
if (type->children != NULL)
    {
    dyStringAppendC(dy, '(');
    for (type = type->children; type != NULL; type = type->next)
        {
	rEncodeType(type, dy);
	if (type->next != NULL)
	    dyStringAppendC(dy, ',');
	}
    dyStringAppendC(dy, ')');
    }
}

void encodeType(struct pfType *type, struct dyString *dy)
/* Encode type into dy. */
{
dyStringClear(dy);
rEncodeType(type, dy);
}


static struct codedType *codedTypeLookup(struct hash *hash, 
	struct dyString *dy, struct pfType *type)
/* Find compOutType that corresponds to type in hash */
{
encodeType(type, dy);
return hashMustFindVal(hash, dy->string);
}

int codedTypeId(struct pfCompile *pfc, struct pfType *type)
/* Return run time type ID associated with type */
{
struct dyString *dy = dyStringNew(256);
struct codedType *cot = codedTypeLookup(pfc->runTypeHash, dy, type);
int id = cot->id;
dyStringFree(&dy);
return id;
}

static void addTypesAndChildTypes(FILE *f, struct hash *hash,
	struct dyString *dy, struct pfType *ty)
{
struct pfType *t;
for (t = ty->children; t != NULL; t = t->next)
    addTypesAndChildTypes(f, hash, dy, t);
encodeType(ty, dy);
if (!hashLookup(hash, dy->string))
    {
    struct codedType *cot = codedTypeNew(dy->string, ty);
    hashAddSaveName(hash, dy->string, cot, &cot->code);
    fprintf(f, "  {%d, \"%s\"},\n", cot->id, cot->code);
    }
}

static void rFillCompHash(FILE *f,
	struct hash *hash, struct dyString *dy, struct pfParse *pp)
/* Fill in hash with uniq types.  Print encodings of unique
 * types as we find them to file. */
{
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    rFillCompHash(f, hash, dy, p);
if (pp->ty)
    {
    encodeType(pp->ty, dy);
    if (!hashLookup(hash, dy->string))
	addTypesAndChildTypes(f, hash, dy, pp->ty);
    }
}

static struct codedBaseType *cbtFind(struct codedBaseType *list,
	int scope, char *name)
/* Search through list to find one that matches scope and name. */
{
struct codedBaseType *el;
for (el = list; el != NULL; el = el->next)
    if (el->scope == scope && sameString(el->name, name))
        return el;
errAbort("Can't cbtFind %d:%s", scope, name);
return NULL;
}

static void printAndSaveCompType(FILE *f, struct hash *hash,
	struct pfType *type, char *encoding)
/* Save to hash and print out to file run-time encoding of type. */
{
struct codedType *cot = codedTypeNew(encoding, type);
hashAddSaveName(hash, encoding, cot, &cot->code);
fprintf(f, "  {%d, \"%s\"},\n", cot->id, cot->code);
}

static void saveSimpleTypes(FILE *f, struct hash *hash,
	struct dyString *dy, struct codedBaseType *cbt)
/* Make a type out of base type, and print and save it.
 * Also do this for arrays and dirs of that type. */
{
struct pfType *type = pfTypeNew(cbt->base), *dirType, *arrayType;
encodeType(type, dy);
printAndSaveCompType(f, hash, type, dy->string);
}

static struct hash *hashCompTypes(struct pfCompile *pfc, 
	struct codedBaseType *cbtList,
	struct pfParse *program, struct dyString *dy, FILE *f)
/* Create a hash full of codedTypes.  Also print out type 
 * encodings. */
{
struct hash *hash = hashNew(0);
struct codedBaseType *cbt;
struct codedBaseType *intCbt = cbtFind(cbtList, 1, "int");
struct codedBaseType *stringCbt = cbtFind(cbtList, 1, "string");
struct codedBaseType *seriousErrCbt = cbtFind(cbtList, 1, "seriousError");
struct codedBaseType *errCbt = cbtFind(cbtList, 1, "error");

/* Make up int and string types, serious error, and error types. The runtime 
 * depends on these being in this order as the first elements of the 
 * type array. */
saveSimpleTypes(f, hash, dy, intCbt);
saveSimpleTypes(f, hash, dy, stringCbt);
saveSimpleTypes(f, hash, dy, seriousErrCbt);
saveSimpleTypes(f, hash, dy, errCbt);

rFillCompHash(f, hash, dy, program);
return hash;
}

static boolean rPrintTypedFields(FILE *f, struct hash *compTypeHash, 
	struct dyString *dy, struct pfBaseType *base, boolean needComma)
/* Recursively print fields. */
{
struct pfType *field;
if (base->parent != NULL)
    needComma = rPrintTypedFields(f, compTypeHash, dy, base->parent, needComma);
for (field = base->fields; field != NULL; field = field->next)
    {
    struct codedType *cot = codedTypeLookup(compTypeHash, dy, field);
    if (needComma)
	fprintf(f, ",");
    needComma = TRUE;
    fprintf(f, "%d:%s", cot->id, field->fieldName);
    }
return needComma;
}

int codedBaseTypeCmpId(const void *va, const void *vb)
/* Compare to sort based on id. */
{
const struct codedBaseType *a = *((struct codedBaseType **)va);
const struct codedBaseType *b = *((struct codedBaseType **)vb);
return a->id - b->id;
}

struct hash *codedTypesCalcAndPrintAsC(struct pfCompile *pfc, 
	struct pfParse *program, FILE *f)
/* Traverse parse tree and encode all types referenced in it.
 * Also print out the types in C structures that the runtime
 * system can interpret. */
{
struct pfScope *scope;
struct hash *compTypeHash;
struct dyString *dy = dyStringNew(0);
struct codedBaseType *cbtList = NULL, *cbt;
struct slRef *ref;

/* Get list of base types sorted by id. */
for (ref = pfc->scopeRefList; ref != NULL; ref = ref->next)
    {
    struct pfScope *scope = ref->val;
    struct hashEl *hel, *helList = hashElListHash(scope->types);
    int scopeId = scope->id;
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	struct pfBaseType *base = hel->val;
	if (base->scope == scope)  /* Avoid included ones. */
	    {
	    AllocVar(cbt);
	    slAddHead(&cbtList, cbt);
	    cbt->id = base->id;
	    cbt->scope = scopeId;
	    cbt->name = base->name;
	    if (base->parent != NULL)
		cbt->parentId = base->parent->id;
	    cbt->needsCleanup = base->needsCleanup;
	    cbt->size = base->size;
	    cbt->base = base;
	    }
	}
    hashElFreeList(&helList);
    }
slSort(&cbtList, codedBaseTypeCmpId);

/* Write out base types in a C table. */
fprintf(f, "/* All base types */\n");
fprintf(f, "struct _pf_base_info _pf_base_info[] = {\n");
for (cbt = cbtList; cbt != NULL; cbt = cbt->next)
    {
    fprintf(f, "  {%d, ", cbt->id);
    fprintf(f, "\"%d:", cbt->scope);
    fprintf(f, "%s\", ", cbt->name);
    fprintf(f, "%d, ", cbt->parentId);
    fprintf(f, "%d, ", cbt->needsCleanup);
    fprintf(f, "%d, ", cbt->size);
    fprintf(f, "},\n");
    }
fprintf(f, "};\n");
fprintf(f, "int _pf_base_info_count = %d;\n\n", slCount(cbtList));

fprintf(f, "/* All composed types */\n");
fprintf(f, "struct _pf_type_info _pf_type_info[] = {\n");
compTypeHash = hashCompTypes(pfc, cbtList, program, dy, f);
fprintf(f, "};\n");
fprintf(f, "int _pf_type_info_count = sizeof(_pf_type_info)/sizeof(_pf_type_info[0]);\n\n");

fprintf(f, "/* All field lists. */\n");
fprintf(f, "struct _pf_field_info _pf_field_info[] = {\n");
for (ref = pfc->scopeRefList; ref != NULL; ref = ref->next)
    {
    struct pfScope *scope = ref->val;
    struct hashEl *hel, *helList = hashElListHash(scope->types);
    int scopeId = scope->id;
    slSort(&helList, hashElCmp);
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	struct pfBaseType *base = hel->val;
	if (base->isClass)
	    {
	    if (base->scope == scope)
		{
		fprintf(f, "  {%d, ", base->id);
		fprintf(f, "\"");
		rPrintTypedFields(f, compTypeHash, dy, base, FALSE);
		fprintf(f, "\"");
		fprintf(f, "},\n");
		}
	    }
	}
    hashElFreeList(&helList);
    }
fprintf(f, "};\n");
fprintf(f, "int _pf_field_info_count = sizeof(_pf_field_info)/sizeof(_pf_field_info[0]);\n\n");
fprintf(f, "\n");

dyStringFree(&dy);
return compTypeHash;
}

