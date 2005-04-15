/* runType - run time type system for paraFlow. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "runType.h"
#include "../compiler/pfPreamble.h"

struct hash *singleTypeHash()
/* Create hash keyed by name with singleType as value */
{
struct hash *hash = hashNew(6);
hashAddInt(hash, "bit", pf_stBit);
hashAddInt(hash, "byte", pf_stByte);
hashAddInt(hash, "short", pf_stShort);
hashAddInt(hash, "int", pf_stInt);
hashAddInt(hash, "long", pf_stLong);
hashAddInt(hash, "float", pf_stFloat);
hashAddInt(hash, "double", pf_stDouble);
hashAddInt(hash, "string", pf_stString);
hashAddInt(hash, "array", pf_stArray);
hashAddInt(hash, "list", pf_stList);
hashAddInt(hash, "dir", pf_stDir);
hashAddInt(hash, "tree", pf_stTree);
hashAddInt(hash, "var", pf_stVar);
hashAddInt(hash, "class", pf_stClass);
hashAddInt(hash, "to", pf_stTo);
hashAddInt(hash, "para", pf_stPara);
hashAddInt(hash, "flow", pf_stFlow);
return hash;
}

struct _pf_type *_pf_type_from_paren_code(char **parenCode, struct _pf_base *bases)
/* Create a little tree of type from parenCode.  Here's an example of the paren code:
 *      6(19,17,17,17)
 * The numbers are indexes into the bases array.  Parenthesis indicate children,
 * commas siblings. */
{
struct _pf_type *type;
char *s = *parenCode, c;
int val = 0;
AllocVar(type);
while ((c = *s) != NULL)
    {
    if (isdigit(c))
        {
	val *= 10;
	val += (c - '0');
	}
    else
        break;
    s += 1;
    }
type->base = &bases[val];
if (c == '(')
    {
    struct _pf_type *child;
    s += 1;
    for (;;)
	{
	child = _pf_type_from_paren_code(&s, bases);
	slAddHead(&type->children, child);
	if (*s == ',')
	    s += 1;
	else
	    break;
	}
    if (*s == ')')
        s += 1;
    else
        errAbort("Problem in paren code '%s'", *parenCode);
    slReverse(&type->children);
    }
*parenCode = s;
return type;
}

void _pf_init_types( struct _pf_base_info *baseInfo, int baseCount,
		     struct _pf_type_info *typeInfo, int typeCount,
		     struct _pf_field_info *structInfo, int structCount)
/* Build up run-time type information from initialization. */
{
struct hash *singleIds = singleTypeHash();
struct _pf_base *bases, *base;
struct _pf_type **types, *type;
int i;
char *s;


/* Process baseInfo into bases. */
AllocArray(bases, baseCount+1);
for (i=0; i<baseCount; ++i)
    {
    struct _pf_base_info *info = &baseInfo[i];
    base = &bases[info->id];
    base->parent = &bases[info->parentId];
    base->scope = atoi(info->name);
    s = strchr(info->name, ':');
    assert(s != NULL);
    s += 1;
    base->name = s;
    base->needsCleanup = info->needsCleanup;
    base->size = info->size;
    if (s[0] != '<')
	{
	base->singleType = hashIntValDefault(singleIds, s, 0);
	if (base->singleType == 0)
	    {
	    base->singleType = pf_stClass;
	    }
	switch (base->singleType)
	    {
	    case pf_stBit:
		{
		static struct x {char b1; _pf_Bit var;} x;
	        base->size = sizeof(_pf_Bit);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stByte:
		{
		static struct x {char b1; _pf_Byte var;} x;
		base->size = sizeof(_pf_Byte);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stShort:
		{
		static struct x {char b1; _pf_Short var;} x;
		base->size = sizeof(_pf_Short);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stInt:
		{
		static struct x {char b1; _pf_Int var;} x;
		base->size = sizeof(_pf_Int);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stLong:
		{
		static struct x {char b1; _pf_Long var;} x;
		base->size = sizeof(_pf_Long);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stFloat:
		{
		static struct x {char b1; _pf_Float var;} x;
		base->size = sizeof(_pf_Float);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stDouble:
		{
		static struct x {char b1; _pf_Double var;} x;
		base->size = sizeof(_pf_Double);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stString:
		{
		static struct x {char b1; _pf_String var;} x;
		base->size = sizeof(_pf_String);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stArray:
		{
		static struct x {char b1; _pf_Array var;} x;
		base->size = sizeof(_pf_Array);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stList:
		{
		static struct x {char b1; _pf_List var;} x;
		base->size = sizeof(_pf_List);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stDir:
		{
		static struct x {char b1; _pf_Dir var;} x;
		base->size = sizeof(_pf_Dir);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stTree:
		{
		static struct x {char b1; _pf_Tree var;} x;
		base->size = sizeof(_pf_Tree);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stVar:
		{
		static struct x {char b1; _pf_Var var;} x;
		base->size = sizeof(_pf_Var);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stClass:
		{
		static struct x {char b1; void *var;} x;
		base->size = sizeof(void *);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stTo:
	    case pf_stPara:
	    case pf_stFlow:
	        break;
	    }
	base->aliAdd = base->alignment-1;
	base->aliMask = ~base->aliAdd;
	}
    }

/* Process typeInfo into types. */
AllocArray(types, typeCount+1);
for (i=0; i<typeCount; ++i)
    {
    struct _pf_type_info *info = &typeInfo[i];
    struct _pf_type *type = _pf_type_from_paren_code(&info->parenCode, bases);
    type->typeId = info->id;
    types[info->id] = type;
    }

/* Process fieldInfo into fields. */
    {
    for (i=0; i<structCount; ++i)
	{
	struct _pf_field_info *info = &structInfo[i];
	int offset = sizeof(struct _pf_object);
	int j, fieldCount;
	char **fields;
	char *typeValList = cloneString(info->typeValList);
	base = &bases[info->classId];
	assert(base->singleType == pf_stClass);
	fieldCount = countChars(typeValList, ',') + 1;
	AllocArray(fields, fieldCount);
	chopByChar(typeValList, ',', fields, fieldCount);
	for (j=0; j<fieldCount; ++j)
	    {
	    char *spec = fields[j];
	    int typeId = atoi(spec);
	    char *name = strchr(spec, ':') + 1;
	    struct _pf_type *type = CloneVar(types[typeId]);
	    struct _pf_base *b = type->base;
	    type->name = name;
	    offset = ((offset + b->aliAdd) & b->aliMask);
	    type->offset = offset;
	    offset += b->size;
	    slAddHead(&base->fields, type);
	    }
	slReverse(&base->fields);
	base->objSize = offset;
	}
    }


/* Clean up and go home. */
hashFree(&singleIds);
_pf_type_table = types;
}
