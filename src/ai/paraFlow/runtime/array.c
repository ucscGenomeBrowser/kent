/* Built in array handling. */

#include "common.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "array.h"
#include "msort.h"

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

_pf_Array _pf_char_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Char *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = _pf_array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Char;
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

_pf_Array _pf_dyString_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Just call pf_string_array_from_tuple since outside of type-checker
 * strings and dyStrings are the same thing. */
{
return _pf_string_array_from_tuple(stack, count, typeId, elTypeId);
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

static void expandArray(_pf_Array array)
/* Double alloated size of array. */
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

void _pf_array_append(_pf_Array array, void *elBuf)
/* Contains array, element-to-append on stack. */
{
if (array->size >= array->allocated)
    expandArray(array);
memcpy(array->elements + array->size * array->elSize,  elBuf, array->elSize);
array->size += 1;
}

void _pf_cm_array_push(_pf_Stack *stack)
/* Push one element on array treating it as a stack. */
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
    case pf_stChar:
	v = &stack[1].Char;
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

void _pf_cm_array_pop(_pf_Stack *stack)
/* Pop one element from array. */
{
_pf_Array array = stack[0].Array;
enum _pf_single_type st = array->elType->base->singleType;
void *dest, *source;
if (array->size <= 0)
    errAbort("Popping from an empty array");
array->size -= 1;
switch (st)
    {
    case pf_stBit:
	dest = &stack[0].Bit;
	break;
    case pf_stByte:
	dest = &stack[0].Byte;
	break;
    case pf_stShort:
	dest = &stack[0].Short;
	break;
    case pf_stInt:
	dest = &stack[0].Int;
	break;
    case pf_stLong:
	dest = &stack[0].Long;
	break;
    case pf_stFloat:
	dest = &stack[0].Float;
	break;
    case pf_stDouble:
	dest = &stack[0].Double;
	break;
    case pf_stChar:
	dest = &stack[0].Char;
	break;
    case pf_stVar:
        dest = &stack[0].Var;
	break;
    default:
	dest = &stack[0].v;
	break;
    }
source = array->elements + array->elSize * array->size;
memcpy(dest, source, array->elSize);
}


static int qCmpBit(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for byte. */
{
const _pf_Bit *a = va, *b = vb;
return *a-*b;
}

static int qCmpByte(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for byte. */
{
const _pf_Byte *a = va, *b = vb;
return *a-*b;
}

static int qCmpShort(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for short. */
{
const _pf_Short *a = va, *b = vb;
return *a-*b;
}

static int qCmpInt(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for int. */
{
const _pf_Int *a = va, *b = vb;
return *a-*b;
}

static int qCmpLong(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for long. */
{
const _pf_Long *a = va, *b = vb;
_pf_Long diff = *a-*b;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else 
    return 0;
}

static int qCmpFloat(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for float. */
{
const _pf_Float *a = va, *b = vb;
_pf_Float diff = *a-*b;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else 
    return 0;
}

static int qCmpDouble(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for double. */
{
const _pf_Double *a = va, *b = vb;
_pf_Double diff = *a-*b;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else 
    return 0;
}

static int qCmpChar(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for byte. */
{
const _pf_Char *a = va, *b = vb;
return *a-*b;
}

static int qCmpString(void *dummy, const void *va, const void *vb)
/* Quick, no ParaFlow callback sort function for string. */
{
const struct _pf_string *a = *((struct _pf_string **)va);
const struct _pf_string *b = *((struct _pf_string **)vb);
if (a == b)
    return 0;
else if (a == NULL)
    return -1;
else if (b == NULL)
    return 1;
return strcmp(a->s, b->s);
}

struct pfCmpState
/* Info we need to communicate with paraFlow in our
 * cmp function. */
     {
     _pf_Stack *stack;
     _pf_FunctionPt cmp;
     };

static int pCmpBit(void *thunk, const void *va, const void *vb)
/* Comparison function for Bit using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
const _pf_Bit *a = va, *b = vb;
pf->stack[0].Bit = *a;
pf->stack[1].Bit = *b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}

static int pCmpByte(void *thunk, const void *va, const void *vb)
/* Comparison function for Byte using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
const _pf_Byte *a = va, *b = vb;
pf->stack[0].Byte = *a;
pf->stack[1].Byte = *b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}

static int pCmpShort(void *thunk, const void *va, const void *vb)
/* Comparison function for Short using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
const _pf_Short *a = va, *b = vb;
pf->stack[0].Short = *a;
pf->stack[1].Short = *b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}

static int pCmpInt(void *thunk, const void *va, const void *vb)
/* Comparison function for Int using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
const _pf_Int *a = va, *b = vb;
pf->stack[0].Int = *a;
pf->stack[1].Int = *b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}

static int pCmpLong(void *thunk, const void *va, const void *vb)
/* Comparison function for Long using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
const _pf_Long *a = va, *b = vb;
pf->stack[0].Long = *a;
pf->stack[1].Long = *b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}

static int pCmpFloat(void *thunk, const void *va, const void *vb)
/* Comparison function for Float using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
const _pf_Float *a = va, *b = vb;
pf->stack[0].Float = *a;
pf->stack[1].Float = *b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}

static int pCmpDouble(void *thunk, const void *va, const void *vb)
/* Comparison function for Double using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
const _pf_Double *a = va, *b = vb;
pf->stack[0].Double = *a;
pf->stack[1].Double = *b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}

static int pCmpChar(void *thunk, const void *va, const void *vb)
/* Comparison function for Char using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
const _pf_Char *a = va, *b = vb;
pf->stack[0].Char = *a;
pf->stack[1].Char = *b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}

static int pCmpObj(void *thunk, const void *va, const void *vb)
/* Comparison function for Obj using paraFlow callback. */
{
struct pfCmpState *pf = thunk;
struct _pf_object *a = *((struct _pf_object **)va);
struct _pf_object *b = *((struct _pf_object **)vb);
a->_pf_refCount += 1;
b->_pf_refCount += 1;
pf->stack[0].Obj = a;
pf->stack[1].Obj = b;
pf->cmp(pf->stack);
return pf->stack[0].Int;
}


void _pf_cm_array_sort(_pf_Stack *stack)
/* Sort array using a user-supplied compare function. */
{
_pf_Array array = stack[0].Array;
_pf_FunctionPt cmp = stack[1].FunctionPt;
enum _pf_single_type st = array->elType->base->singleType;
int (*compare)(void *dummy, const void *, const void *) = NULL;

if (cmp == NULL)
    {
    switch (st)
        {
	case pf_stBit:
	    compare = qCmpBit;
	    break;
	case pf_stByte:
	    compare = qCmpByte;
	    break;
	case pf_stChar:
	    compare = qCmpChar;
	    break;
	case pf_stShort:
	    compare = qCmpShort;
	    break;
	case pf_stInt:
	    compare = qCmpInt;
	    break;
	case pf_stLong:
	    compare = qCmpLong;
	    break;
	case pf_stFloat:
	    compare = qCmpFloat;
	    break;
	case pf_stDouble:
	    compare = qCmpDouble;
	    break;
	case pf_stString:
	    compare = qCmpString;
	    break;
	default:
	    errAbort("Need to supply a comparison function to sort array of complex type.");
	    break;
	}
    _pf_cm_mSort_r(array->elements, array->size, array->elSize, NULL, compare);
    }
else
    {
    struct pfCmpState pf;
    pf.stack = stack;
    pf.cmp = cmp;
    switch (st)
        {
	case pf_stBit:
	    compare = pCmpBit;
	    break;
	case pf_stByte:
	    compare = pCmpByte;
	    break;
	case pf_stShort:
	    compare = pCmpShort;
	    break;
	case pf_stInt:
	    compare = pCmpInt;
	    break;
	case pf_stLong:
	    compare = pCmpLong;
	    break;
	case pf_stFloat:
	    compare = pCmpFloat;
	    break;
	case pf_stDouble:
	    compare = pCmpDouble;
	    break;
	case pf_stChar:
	    compare = pCmpChar;
	    break;
	default:
	    compare = pCmpObj;
	    break;
	}
    _pf_cm_mSort_r(array->elements, array->size, array->elSize, &pf, compare);
    }
}
