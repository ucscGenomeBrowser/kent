/* initVar - handle variable initialization from tuples */
#include "common.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "initVar.h"
#include "dir.h"

static _pf_Object tuple_to_class(_pf_Stack *stack, struct _pf_type *type, 
	char *encoding, _pf_Stack **retStack, char **retEncoding);

_pf_Dir _pf_r_tuple_to_dir(_pf_Stack *stack, struct _pf_type *type, 
	char *encoding, _pf_Stack **retStack, char **retEncoding);

static _pf_Array tuple_to_array(_pf_Stack *stack, struct _pf_type *type, 
	char *encoding, _pf_Stack **retStack, char **retEncoding);

void _pf_suckItemOffStack(_pf_Stack **pStack, char **pEncoding, 
	struct _pf_type *fieldType, void *output)
/* Suck data off of stack and puts it in output. 
 * Parameters:
 *   pStack - Pointer to stack.  Updated to point to next item to
 *            consume.
 *   pEncoding - points to something that looks like ((xx)x(xxx)x)
 *            where the x's indicate that a single data item is to be
 *            read, and the parens indicate that an array or object
 *            containing that many fields is to be read.  This is
 *            updated by call to point to info on next item.
 *   fieldType - Describes type of this field.
 *   output - Where to put result. */
{
_pf_Stack *stack = *pStack;
enum _pf_single_type st = fieldType->base->singleType;
switch (st)
    {
    case pf_stBit:
	*((_pf_Bit *)output) = stack->Bit;
	break;
    case pf_stByte:
	*((_pf_Byte *)output) = stack->Byte;
	break;
    case pf_stShort:
	*((_pf_Short *)output) = stack->Short;
	break;
    case pf_stInt:
	*((_pf_Int *)output) = stack->Int;
	break;
    case pf_stLong:
	*((_pf_Long *)output) = stack->Long;
	break;
    case pf_stFloat:
	*((_pf_Float *)output) = stack->Float;
	break;
    case pf_stDouble:
	*((_pf_Double *)output) = stack->Double;
	break;
    case pf_stString:
	*((_pf_String *)output) = stack->String;
	break;
    case pf_stVar:
	*((_pf_Var *)output) = stack->Var;
	break;
    case pf_stArray:
    case pf_stClass:
    case pf_stDir:
	{
	char *encoding = *pEncoding;
	boolean goDeep = (*encoding == '(');
	if (goDeep)
	    {
	    switch (st)
	        {
		case pf_stArray:
		    *((_pf_Array *)output) = 
		    	tuple_to_array(stack, fieldType->children, encoding, pStack, pEncoding);
		    break;
		case pf_stClass:
		    *((_pf_Object *)output) = 
		    	tuple_to_class(stack, fieldType, encoding, pStack, pEncoding);
		    break;
		case pf_stDir:
		     *((_pf_Dir *)output) = 
		    	_pf_r_tuple_to_dir(stack, fieldType->children, encoding, pStack, pEncoding);
		default:
		    internalErr();
		    break;
		}
	    return;	/* We updated *pStack and *pEncoding already... */
	    }
	else
	    *((_pf_Object *)output) = stack->Obj;
	break;
	}
    case pf_stFlowPt:
    case pf_stToPt:
	*((_pf_FunctionPt *)output) = stack->FunctionPt;
        break;
    default:
        internalErr();
	break;
    }
*pStack = stack+1;
*pEncoding = *pEncoding + 1;
}

void _pf_class_cleanup(struct _pf_object *obj, int typeId)
/* Clean up all class fields, and then class itself. */
{
struct _pf_type *type = _pf_type_table[typeId], *fieldType;
struct _pf_base *base = type->base;
char *s = (char *)(obj);
verbose(2, "_pf_class_cleanup (%s)\n", base->name);
for (fieldType = base->fields; fieldType != NULL; fieldType = fieldType->next)
    {
    if (fieldType->base->needsCleanup)
        {
	struct _pf_object *subObj = *((_pf_Object *)(s + fieldType->offset));
	if (subObj != NULL && --subObj->_pf_refCount <= 0)
	    subObj->_pf_cleanup(subObj, fieldType->typeId);
	}
    }
freeMem(obj);
}

_pf_Object tuple_to_class(_pf_Stack *stack, struct _pf_type *type,
	char *encoding, _pf_Stack **retStack, char **retEncoding)
/* Convert tuple on stack to class. */
{
struct _pf_type *fieldType;
struct _pf_base *base = type->base;
struct _pf_object *obj = needMem(base->objSize);
char *s = (char *)(obj);
_pf_Stack *field;

obj->_pf_refCount = 1;
obj->_pf_cleanup = _pf_class_cleanup;
obj->_pf_polyTable = base->polyTable;
if (encoding[0] != '(')
    errAbort("Expecting ( in class tuple encoding, got %c", encoding[0]);
if (encoding[1] == ')')	/* Empty tuple are just requests for all zero. */
    return obj;
encoding += 1;
for (fieldType = base->fields; fieldType != NULL; fieldType = fieldType->next)
    {
    int offset = fieldType->offset;
    _pf_suckItemOffStack(&stack, &encoding, fieldType, s + offset);
    }
if (encoding[0] != ')')
    errAbort("Expecting ) in class tuple encoding, got %c", encoding[0]);
encoding += 1;
*retStack = stack;
*retEncoding = encoding;
obj->_pf_refCount = 1;
obj->_pf_cleanup = _pf_class_cleanup;
return obj;
}

_pf_Object _pf_tuple_to_class(_pf_Stack *stack, int typeId, char *encoding)
/* Convert tuple on stack to class. */
{
return tuple_to_class(stack, _pf_type_table[typeId], encoding, &stack, &encoding);
}

int _pf_countOurLevel(char *encoding)
/* Count up items in our level of encoding.  For
 * (x(xxx)) for instance would count two items: x and (xxx) */
{
int count = 0;
char c;
int level = -1;
for (;;)
    {
    c = *encoding++;
    if (c == ')')
	{
	--level;
	if (level < 0)
	   return count;
	}
    else
        {
	if (level == 0)
	    count += 1;
	if (c == '(')
	    ++level;
	else if (c == 0)
	    internalErr();
	}
    }
}

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

static _pf_Array array_of_type(int count, int allocated, 
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
return array_of_type(size, size, elType, elSize, elements);
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


_pf_Array _pf_bit_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
struct _pf_array *array;
_pf_Bit *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = array_of_type(count, count, elType, sizeof(elements[0]), elements);

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
array = array_of_type(count, count, elType, sizeof(elements[0]), elements);

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
array = array_of_type(count, count, elType, sizeof(elements[0]), elements);

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
array = array_of_type(count, count, elType, sizeof(elements[0]), elements);

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
array = array_of_type(count, count, elType, sizeof(elements[0]), elements);

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
array = array_of_type(count, count, elType, sizeof(elements[0]), elements);

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
array = array_of_type(count, count, elType, sizeof(elements[0]), elements);

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
array = array_of_type(count, count, elType, sizeof(strings[0]), strings);

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
array = array_of_type(count, count, elType, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Var;
return array;
}

_pf_Array tuple_to_array(_pf_Stack *stack, struct _pf_type *elType, 
	char *encoding, _pf_Stack **retStack, char **retEncoding)
/* Convert tuple to array. */
{
int elSize = elType->base->size;
int count = _pf_countOurLevel(encoding);
char *buf = NULL;
_pf_Array array;
int i;

if (count > 0)
    buf = needLargeMem(elSize * count);
array = array_of_type(count, count, elType, elSize, buf);

if (encoding[0] != '(')
    errAbort("Expecting ( in array tuple encoding, got %c", encoding[0]);
encoding += 1;
for (i=0; i<count; ++i)
    {
    _pf_suckItemOffStack(&stack, &encoding, elType, buf);
    buf += elSize;
    }
if (encoding[0] != ')')
    errAbort("Expecting ) in array tuple encoding, got %c", encoding[0]);
encoding += 1;
*retStack = stack;
*retEncoding = encoding;
return array;
}

_pf_Array _pf_tuple_to_array(_pf_Stack *stack, int typeId, char *encoding)
/* Convert tuple to array. */
{
return tuple_to_array(stack, _pf_type_table[typeId], encoding, &stack, &encoding);
}

