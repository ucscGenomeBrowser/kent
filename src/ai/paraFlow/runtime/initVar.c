/* initVar - handle variable initialization from tuples */
#include "common.h"
#include "runType.h"
#include "../compiler/pfPreamble.h"
#include "object.h"
#include "initVar.h"

static _pf_Array tuple_to_array(_pf_Stack *stack, struct _pf_type *type, 
	char *encoding, _pf_Stack **retStack, char **retEncoding);

static _pf_Object tuple_to_class(_pf_Stack *stack, struct _pf_type *type, 
	char *encoding, _pf_Stack **retStack, char **retEncoding);

static void suckItemOffStack(_pf_Stack **pStack, char **pEncoding, 
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
	{
	char *encoding = *pEncoding;
	boolean goDeep = (*encoding == '(');
	if (goDeep)
	    {
	    switch (st)
	        {
		case pf_stArray:
		    *((_pf_Array *)output) = 
		    	tuple_to_array(stack, fieldType, encoding, pStack, pEncoding);
		    break;
		case pf_stClass:
		    *((_pf_Object *)output) = 
		    	tuple_to_class(stack, fieldType, encoding, pStack, pEncoding);
		    break;
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
free(obj);
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
if (encoding[0] != '(')
    errAbort("Expecting ( in class tuple encoding, got %c", encoding[0]);
if (encoding[1] == ')')	/* Empty tuple are just requests for all zero. */
    return obj;
encoding += 1;
for (fieldType = base->fields; fieldType != NULL; fieldType = fieldType->next)
    {
    int offset = fieldType->offset;
    suckItemOffStack(&stack, &encoding, fieldType, s + offset);
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

int countOurLevel(char *encoding)
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

static void _pf_array_cleanup(struct _pf_array *array, int id)
/* Clean up all elements of array, and then array itself. */
{
struct _pf_type *elType = array->elType;
verbose(2, "_pf_array_cleanup of %d elements\n", array->size);
if (elType->base->needsCleanup)
    {
    struct _pf_object **objs = (struct _pf_object **)(array->elements);
    int i;
    for (i=0; i<array->size; ++i)
	{
	struct _pf_object *obj = objs[i];
	if (obj != NULL && --obj->_pf_refCount <= 0)
	    obj->_pf_cleanup(obj, array->elType->typeId);
	    /* FIXME - in some cases id is 0 here when it shouldn't be
	     * for arrays...  I think the fix is in setting up the
	     * type tables. */
	}
    }
freeMem(array->elements);
free(array);
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

_pf_Array _pf_dim_array(int size, int arrayTypeId, int elTypeId)
/* Return array of given type and size, initialized to zeroes. */
{
struct _pf_type *elType = _pf_type_table[elTypeId];
int elSize = elType->base->size;
char *elements = needLargeZeroedMem(elSize*size);
return array_of_type(size, size, elType, elSize, elements);
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

_pf_Array tuple_to_array(_pf_Stack *stack, struct _pf_type *type, 
	char *encoding, _pf_Stack **retStack, char **retEncoding)
/* Convert tuple to array. */
{
struct _pf_type *elType = type->children;
int elSize = elType->base->size;
int count = countOurLevel(encoding);
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
    suckItemOffStack(&stack, &encoding, elType, buf);
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

