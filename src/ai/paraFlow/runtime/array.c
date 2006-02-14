/* Built in array handling. */

#include "common.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "array.h"

void _pf_array_cleanup(struct _pf_array *array, int id)
/* Clean up all elements of array, and then array itself. */
{
struct _pf_type *elType = array->elType;
struct _pf_base *elBase = elType->base;
verbose(2, "_pf_array_cleanup of %d elements\n", array->size);
if (elBase->singleType == pf_stVar)
    {
    struct _pf_var *vars = (struct _pf_var *)(array->elements);;
    int i;
    for (i=0; i<array->size; ++i)
        _pf_var_cleanup(vars[i]);
    }
else if (elType->base->needsCleanup)
    {
    struct _pf_object **objs = (struct _pf_object **)(array->elements);
    int i;
    for (i=0; i<array->size; ++i)
	{
	struct _pf_object *obj = objs[i];
	if (obj != NULL && --obj->_pf_refCount <= 0)
	    obj->_pf_cleanup(obj, array->elType->typeId);
	}
    }
freeMem(array->elements);
freeMem(array);
}

_pf_Array _pf_array_of_type(int count, int allocated, 
	struct _pf_type *elType, int elSize, void *elements)
/* Create an array of string initialized from tuple on stack. 
 * You still need to fill in array->elements. */
{
struct _pf_array *array;
AllocVar(array);
array->_pf_refCount = 1;
array->_pf_cleanup = _pf_array_cleanup;
array->elements = elements;
array->size = count;
array->allocated = count;
array->elSize = elSize;
array->elType = elType;
return array;
}

_pf_Array _pf_dim_array(long size, int elTypeId)
/* Return array of given type and size, initialized to zeroes. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
int elSize = elType->base->size;
char *elements = NULL;
if (size > 0)
    elements = needLargeZeroedMem(elSize*size);
return _pf_array_of_type(size, size, elType, elSize, elements);
}

static _pf_Array rMultiDimArray(_pf_Stack *stack, int dimCount, 
	struct _pf_type *type)
/* Return multidimensional array initialized to zero, recursively
 * constructed if need be. */
{
_pf_Array a;
struct _pf_type *elType = type->children;
long elCount = stack[0].Long;
a = _pf_dim_array(elCount,  elType->typeId);
if (dimCount > 1)
    {
    long i;
    _pf_Array *elements = (_pf_Array *)a->elements;
    for (i=0; i<elCount; ++i)
	elements[i] = rMultiDimArray(stack+1, dimCount-1, type->children);
    }
return a;
}


_pf_Array _pf_multi_dim_array(_pf_Stack *stack, int dimCount,
    int typeId)
/* Return multi-dimensional array where individual dimensions are on stack */
{
struct _pf_type *type = _pf_type_table[typeId];
return rMultiDimArray(stack, dimCount, type);
}


_pf_Array _pf_bit_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Bit *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Bit;
return array;
}

_pf_Array _pf_byte_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Byte *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Byte;
return array;
}

_pf_Array _pf_short_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Short *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Short;
return array;
}

_pf_Array _pf_int_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Int *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Int;
return array;
}

_pf_Array _pf_long_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Long *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Long;
return array;
}

_pf_Array _pf_float_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Float *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Float;
return array;
}

_pf_Array _pf_double_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Double *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Double;
return array;
}

_pf_Array _pf_string_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_String *strings = NULL;
int i;

if (count > 0) AllocArray(strings, count);
array = _pf_array_of_type(count, count, elType, sizeof(strings[0]), strings);

for (i=0; i<count; ++i)
    strings[i] = stack[i].String;
return array;
}

_pf_Array _pf_var_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Var *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Var;
return array;
}

void _pf_array_append(_pf_Array array, void *elBuf)
/* Contains array, element-to-append on stack. */
{
if (array->size >= array->allocated)
    {
    size_t oldSize = array->size * array->elSize;
    size_t newSize, newAllocated;
    if (array->allocated == 0)
	array->allocated = newAllocated = 4;
    else
        array->allocated = newAllocated = (array->allocated + array->allocated);
    newSize = newAllocated * array->elSize;
    array->elements = needMoreMem(array->elements, oldSize, newSize);
    }
memcpy(array->elements + array->size * array->elSize,  elBuf, array->elSize);
array->size += 1;
}

void _pf_cm_array_push(_pf_Stack *stack)
/* Push one on stack. */
{
_pf_Array array = stack[0].Array;
enum _pf_single_type st = array->elType->base->singleType;
void *v = NULL;
switch (st)
    {
    case pf_stBit:
	v = &stack[1].Bit;
	break;
    case pf_stByte:
	v = &stack[1].Byte;
	break;
    case pf_stShort:
	v = &stack[1].Short;
	break;
    case pf_stInt:
	v = &stack[1].Int;
	break;
    case pf_stLong:
	v = &stack[1].Long;
	break;
    case pf_stFloat:
	v = &stack[1].Float;
	break;
    case pf_stDouble:
	v = &stack[1].Double;
	break;
    case pf_stVar:
        v = &stack[1].Var;
	break;
    default:
	v = &stack[1].v;
	break;
    }
_pf_array_append(array, v);
}

