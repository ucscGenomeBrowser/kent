/* runType - run time type system for paraFlow. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"

struct _pf_type **_pf_type_table;	
static int _pf_type_table_size;
static struct _pf_base *_pf_base_table;
static int _pf_base_table_size;

static struct hash *singleTypeHash()
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
hashAddInt(hash, "char", pf_stChar);
hashAddInt(hash, "string", pf_stString);
hashAddInt(hash, "dyString", pf_stString);
hashAddInt(hash, "array", pf_stArray);
hashAddInt(hash, "list", pf_stList);
hashAddInt(hash, "dir", pf_stDir);
hashAddInt(hash, "tree", pf_stTree);
hashAddInt(hash, "var", pf_stVar);
hashAddInt(hash, "class", pf_stClass);
hashAddInt(hash, "to", pf_stTo);
hashAddInt(hash, "flow", pf_stFlow);
hashAddInt(hash, "<toPt>", pf_stToPt);
hashAddInt(hash, "<flowPt>", pf_stFlowPt);
return hash;
}


static struct _pf_type *findChildTypeInHash(struct hash *typeHash, char **parenCode)
/* Find type in hash */
{
char *start = *parenCode;
char *end = start, c;
int level = 0;
char *childCode;
struct _pf_type *type;

/* Find end: either a ',', or a ')' at right nesting level. */
for (;;)
    {
    c = *end;
    if (c == 0)
	internalErr();
    else if (c == '(')
        ++level;
    else if (c == ',' || c == ')')
        {
	if (level == 0)
	    break;
	if (c == ')')
	    --level;
	}
    end += 1;
    }
*parenCode = end;

/* Look up just child code in hash.  If can't find it die,
 * else return it. */
childCode = cloneStringZ(start, end-start);
type = hashFindVal(typeHash, childCode);
freeMem(childCode);
if (type == NULL)
    internalErr();
return type;
}


struct _pf_type *_pf_type_from_paren_code(struct hash *typeHash, 
	char *parenCode, struct hash *baseHash, struct dyString *dy)
/* Create a little tree of type from parenCode.  Here's an example of the paren code:
 *      6(string,int,int,int)
 * The numbers are indexes into the bases array.  Parenthesis indicate children,
 * commas siblings. */
{
struct _pf_type *type;
char *s = parenCode, c;
int val = 0;
AllocVar(type);
dyStringClear(dy);
while ((c = *s) != 0)
    {
    if (c == ',' || c == '(' || c == ')')
        break;
    dyStringAppendC(dy, c);
    s += 1;
    }
type->base = hashFindVal(baseHash, dy->string);
if (type->base == NULL)
    internalErr();
if (c == '(')
    {
    struct _pf_type *child;
    s += 1;
    for (;;)
	{
	child = findChildTypeInHash(typeHash, &s);
	child = CloneVar(child);
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
return type;
}

static void fillInLocalTypeInfo(struct _pf_local_type_info *lti, struct hash *typeHash)
/* Fill in local type info's ID field. */
{
char *parenCode;
while ((parenCode = lti->parenCode) != NULL)
    {
    struct _pf_type *type = hashFindVal(typeHash, parenCode);
    if (type == NULL)
	{
        internalErr();
	}
    lti->id = type->typeId;
    lti += 1;
    }
}

static void fillInPolyInfo(struct _pf_poly_info *polyInfo, struct hash *baseHash)
/* Fill in polymorphic function tables. */
{
char *className;
while ((className = polyInfo->className) != NULL)
    {
    struct _pf_base *base = hashFindVal(baseHash, className);
    if (base == NULL)
        internalErr();
    base->polyTable = polyInfo->polyTable;
    polyInfo += 1;
    }
}


void _pf_init_types( struct _pf_base_info *baseInfo, int baseCount,
		     struct _pf_type_info *typeInfo, int typeCount,
		     struct _pf_field_info *fieldInfo, int fieldCount,
		     struct _pf_module_info *moduleInfo, int moduleCount)
/* Build up run-time type information from initialization. */
{
struct hash *singleIds = singleTypeHash();
struct hash *baseHash = hashNew(10);
struct hash *typeHash = hashNew(0);
struct _pf_base *bases, *base;
struct _pf_type **types, *type;
struct dyString *dy = dyStringNew(0);
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
    hashAdd(baseHash, base->name, base);
    base->singleType = hashIntValDefault(singleIds, s, 0);
    if (base->singleType == 0)
	{
	if (s[0] != '<')
	    base->singleType = pf_stClass;
	}
    if (base->singleType != 0)
	{
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
	    case pf_stChar:
		{
		static struct x {char b1; _pf_Char var;} x;
		base->size = sizeof(_pf_Char);
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
	    case pf_stToPt:
	    case pf_stFlowPt:
		{
		static struct x {char b1; void (*var)();} x;
		base->size = sizeof(void *);
		base->alignment = (char *)&x.var - (char *)&x;
		break;
		}
	    case pf_stTo:
	    case pf_stFlow:
		break;
	    default:
		internalErr();
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
    char *s = info->parenCode;
    struct _pf_type *type = _pf_type_from_paren_code(typeHash, 
    		info->parenCode, baseHash, dy);
    type->typeId = info->id;
    types[info->id] = type;
    hashAdd(typeHash, info->parenCode, type);
    }

/* Process fieldInfo into fields. */
    {
    for (i=0; i<fieldCount; ++i)
	{
	struct _pf_field_info *info = &fieldInfo[i];
	int offset = sizeof(struct _pf_object);
	int j, fieldCount;
	char **fields;
	char *typeValList = cloneString(info->typeValList);
	base = &bases[info->classId];
	assert(base->singleType == pf_stClass);
	if (typeValList[0] != 0)
	    {
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
	    }
	base->objSize = offset;
	}
    }

/* Process moduleInfo . */
    {
    struct _pf_module_info *info;
    for (i=0; i<moduleCount; ++i)
        {
	info = &moduleInfo[i];
	fillInLocalTypeInfo(info->lti, typeHash);
	fillInPolyInfo(info->polyInfo, baseHash);
	}
    }

/* Clean up and go home. */
dyStringFree(&dy);
hashFree(&singleIds);
hashFree(&typeHash);
_pf_type_table = types;
_pf_type_table_size = typeCount;
_pf_base_table = bases;
_pf_base_table_size = baseCount;
}

/* The type IDs 0,1,2,3 are set up by the compiler's
 * codedType.c routine to always be int,string,seriousError,error*/

int _pf_find_int_type_id()
/* Return int type ID. */
{
return 0;
}

int _pf_find_string_type_id()
/* Return string type ID. */
{
return 1;
}

int _pf_find_serious_error_type_id()
/* Return id of seriousError. */
{
return 2;
}

int _pf_find_error_type_id()
/* Return id of error */
{
return 3;
}

_pf_Bit _pf_base_is_ancestor(struct _pf_base *base, 
	struct _pf_base *ancestor)
/* Return TRUE if ancestor really is same as base or one of bases's
 * ancestors. */
{
while (base != NULL)
    {
    if (base == ancestor)
        return TRUE;
    base = base->parent;
    }
return FALSE;
}


_pf_Bit _pf_check_types(int destType, int sourceType)
/* Check that sourceType can be converted to destType. */
{
struct _pf_type *tDest = _pf_type_table[destType];
struct _pf_type *tSource = _pf_type_table[sourceType];
for (;;)
    {
    struct _pf_base *bDest = tDest->base;
    struct _pf_base *bSource = tSource->base;
    if (bDest->singleType != bSource->singleType)
        return FALSE;
    if (bSource->singleType == pf_stClass)
        {
	boolean gotMatch = FALSE;
	while (bSource != NULL)
	    {
	    if (bSource == bDest)
	        {
		gotMatch = TRUE;
		break;
	    	}
	    bSource = bSource->parent;
	    }
	if (!gotMatch)
	    return FALSE;
	}
    tDest = tDest->children;
    tSource = tSource->children;
    if (tDest == NULL && tSource == NULL)
        return TRUE;
    if (tDest == NULL || tSource == NULL)
        return FALSE;
    }
}

void _pf_rtar_init_tables(struct _pf_functionFixedInfo **table,
	int tableSize, struct _pf_local_type_info *lti)
/* Convert local type IDs to global type IDs, and fill in local
 * var field offset in fixed part of run-time activation records. 
 * Called at startup of each module. */
{
int tableIx;
struct _pf_functionFixedInfo *fixed;
for (tableIx=0; tableIx<tableSize; ++tableIx)
    {
    int i;
    int offset = 0;
    fixed = table[tableIx];
    if (fixed->classType >= 0)
        fixed->classType = lti[fixed->classType].id;
    fixed->typeId = lti[fixed->typeId].id;
    for (i=0; i<fixed->varCount; ++i)
        {
	struct _pf_localVarInfo *var = &fixed->vars[i];
	struct _pf_type *type;
	struct _pf_base *b;
	var->type = lti[var->type].id;
	type = _pf_type_table[var->type];
	b = type->base;
	offset = ((offset + b->aliAdd) & b->aliMask);
	var->offset = offset;
	offset += b->size;
	}
    fixed->dataSize = offset;
    }
}
