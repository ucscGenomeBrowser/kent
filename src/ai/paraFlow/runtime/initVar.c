/* initVar - handle variable initialization from tuples */
#include "common.h"
#include "../compiler/pfPreamble.h"
#include "object.h"
#include "initVar.h"

static void _pf_array_cleanup(struct _pf_array *array)
{
uglyf("_pf_array_cleanup of %d elements\n", array->count);
freeMem(array->elements);
free(array);
}

static _pf_Array array_of_type(int count, int allocated, 
	int arrayTypeId, int elTypeId, int elSize, void *elements)
/* Create an array of string initialized from tuple on stack. 
 * You still need to fill in array->elements. */
{
struct _pf_array *array;
AllocVar(array);
array->_pf_refCount = 1;
array->_pf_cleanup = _pf_array_cleanup;
array->_pf_typeId = arrayTypeId;
array->elements = elements;
array->count = count;
array->allocated = count;
array->elSize = elSize;
array->elType = elTypeId;
return array;
}

_pf_Array _pf_bit_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Bit *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Bit;
return array;
}

_pf_Array _pf_byte_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Byte *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Byte;
return array;
}

_pf_Array _pf_short_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Short *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Short;
return array;
}

_pf_Array _pf_int_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Int *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Int;
return array;
}

_pf_Array _pf_long_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Long *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Long;
return array;
}

_pf_Array _pf_float_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Float *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Float;
return array;
}

_pf_Array _pf_double_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Double *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Double;
return array;
}

_pf_Array _pf_string_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_String *strings;
int i;

AllocArray(strings, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(strings[0]), strings);

for (i=0; i<count; ++i)
    strings[i] = stack[i].String;
return array;
}

_pf_Array _pf_var_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Var *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Var;
return array;
}

#ifdef TEMPLATE
_pf_Array _pf_xyz_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Xyz *elements;
int i;

AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Xyz;
return array;
}

#endif /* TEMPLATE */
