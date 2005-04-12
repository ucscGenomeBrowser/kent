/* runType - run time type system for paraFlow. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "runType.h"

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
		     struct _pf_field_info *fieldInfo, int fieldCount)
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
    if (s[0] != '<')
	{
	base->singleType = hashIntValDefault(singleIds, s, 0);
	if (base->singleType == 0)
	    {
	    base->singleType = pf_stClass;
	    assert(sameString(base->parent->name, "<class>"));
	    }
	}
    }

/* Process typeInfo into types. */
AllocArray(types, typeCount+1);
for (i=0; i<typeCount; ++i)
    {
    struct _pf_type_info *info = &typeInfo[i];
    types[info->id] = _pf_type_from_paren_code(&info->parenCode, bases);
    }

/* Process fieldInfo into fields. */
for (i=0; i<fieldCount; ++i)
    {
    struct _pf_field_info *info = &fieldInfo[i];
    base = &bases[info->classId];
    assert(base->singleType == pf_stClass);
    /* todo - parse info->typeVal */
    }
hashFree(&singleIds);
_pf_type_table = types;
}
