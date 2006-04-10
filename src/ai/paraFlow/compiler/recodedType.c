/* recodedType - Types in form that can be used at run time. 
 * This deals with the local type table inside of each 
 * paraflow module, as opposed to the global type table
 * that applies to all modules. */
#include "common.h"
#include "pfType.h"
#include "pfParse.h"
#include "pfCompile.h"
#include "codedType.h"
#include "recodedType.h"
#include "backEnd.h"

struct recodedType
/* Local reference to a type */
    {
    struct recodedType *next;
    char *encoding;	/* Paren/comma/name coding. */
    int localTypeId;	/* Offset into local type table. */
    int label;		/* Label used for assembly code. */
    };

int recodedTypeCmp(const void *va, const void *vb)
/* Compare two recodedTypes. */
{
const struct recodedType *a = *((struct recodedType **)va);
const struct recodedType *b = *((struct recodedType **)vb);
return a->localTypeId - b->localTypeId;
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

static struct recodedType *moduleRecodedTypes(struct pfCompile *pfc)
/* Get sorted list of all recoded types in module. */
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
return rtList;
}


void recodedTypeTableToC(struct pfCompile *pfc, char *moduleName, FILE *f)
/* Print out type table for one module in C. */
{
struct recodedType *rt, *rtList = moduleRecodedTypes(pfc);
fprintf(f, "struct %s %s_%s", recodedStructType, recodedTypeTableName, 
	mangledModuleName(moduleName));
fprintf(f, "[] = {\n");
for (rt = rtList; rt != NULL; rt = rt->next)
    fprintf(f, "  {0, \"%s\"},\n", rt->encoding);
fprintf(f, "  {0, 0},\n");
fprintf(f, "};\n");
fprintf(f, "\n");
}

void recodedTypeTableToBackend(struct pfCompile *pfc, char *moduleName,
	FILE *f)
/* Write type table for module to back end (assembler). */
{
uglyf("Starting recodedTypeTableToBackend\n");
struct recodedType *rt, *rtList = moduleRecodedTypes(pfc);
uglyf("ok 1\n");
struct pfBackEnd *back = pfc->backEnd;
char label[64];
back->stringSegment(back, f);
for (rt = rtList; rt != NULL; rt = rt->next)
    {
    rt->label = ++pfc->isxLabelMaker;
    safef(label, sizeof(label), "L%d", rt->label);
    back->emitLabel(back, label, 1, FALSE, f);
    back->emitAscii(back, rt->encoding, strlen(rt->encoding), f);
    }
back->dataSegment(back, f);
safef(label, sizeof(label), "%s_%s", recodedTypeTableName, 
	mangledModuleName(moduleName) );
back->emitLabel(back, label, 16, TRUE, f);
for (rt = rtList; rt != NULL; rt = rt->next)
    {
    back->emitInt(back, 0, f);
    safef(label, sizeof(label), "L%d", rt->label);
    back->emitPointer(back, label, f);
    }
back->emitInt(back, 0, f);
back->emitInt(back, 0, f);
uglyf("Done recodedTypeTableToBackend\n");
}

