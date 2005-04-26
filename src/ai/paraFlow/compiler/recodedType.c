/* recodedType - Types in form that can be used at run time. */
#include "common.h"
#include "pfType.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "recodedType.h"

struct recodedType
    {
    struct recodedType *next;
    char *encoding;	/* Paren/comma/name coding. */
    int localTypeId;	/* Offset into local type table. */
    };

int recodedTypeCmp(const void *va, const void *vb)
/* Compare two recodedTypes. */
{
const struct recodedType *a = *((struct recodedType **)va);
const struct recodedType *b = *((struct recodedType **)vb);
return a->localTypeId - b->localTypeId;
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

int recodedTypeId(struct pfCompile *pfc, struct pfType *type)
/* Return run time type ID associated with type */
{
struct dyString *dy = dyStringNew(256);
struct recodedType *rt;
struct hash *hash = pfc->moduleTypeHash;
encodeType(type, dy);
rt = hashFindVal(hash, dy->string);
if (rt == NULL)
    {
    AllocVar(rt);
    rt->localTypeId = hash->elCount;
    hashAddSaveName(hash, dy->string, rt, &rt->encoding);
    }
dyStringFree(&dy);
return rt->localTypeId;
}

void printModuleTypeTable(struct pfCompile *pfc, FILE *f)
/* Print out type table for one module. */
{
struct hash *hash = pfc->moduleTypeHash;
struct hashEl *hel;
struct hashCookie iterator;
struct recodedType *rtList = NULL, *rt;

/* Build up list of all elements of moduleTypeHash. */
iterator = hashFirst(hash);
while ((hel = hashNext(&iterator)) != NULL)
    {
    rt = hel->val;
    slAddHead(&rtList, rt);
    }

/* Sort it and print it out. */
slSort(&rtList, recodedTypeCmp);
for (rt = rtList; rt != NULL; rt = rt->next)
    fprintf(f, "  {0, \"%s\"},\n", rt->encoding);
fprintf(f, "  {0, 0},\n");
}

