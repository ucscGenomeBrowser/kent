/* initVar - handle variable initialization from tuples */
#include "common.h"
#include "runType.h"
#include "../compiler/pfPreamble.h"
#include "object.h"
#include "initVar.h"

static void _pf_string_cleanup(struct _pf_string *string, int typeId)
/* Clean up string->s and string itself. */
{
// uglyf("_pf_string_cleanup %s\n", string->s);
verbose(2, "_pf_string_cleanup %s\n", string->s);
if (string->allocated != 0)
    freeMem(string->s);
freeMem(string);
}

static struct _pf_string *_pf_string_init(char *s)
/* Wrap string around constant. */
{
struct _pf_string *string;
AllocVar(string);
string->_pf_refCount = 1;
string->_pf_cleanup = _pf_string_cleanup;
string->s = s;
string->size = string->allocated = strlen(s);
return string;
}

struct _pf_string *_pf_string_from_const(char *s)
/* Wrap string around constant. */
{
struct _pf_string *string = _pf_string_init(s);
string->allocated = 0;
return string;
}

struct _pf_string *_pf_string_from_long(_pf_Long ll)
/* Wrap string around Long. */
{
char buf[32];
safef(buf, sizeof(buf), "%lld", ll);
return _pf_string_init(cloneString(buf));
}

struct _pf_string *_pf_string_from_double(_pf_Double d)
/* Wrap string around Double. */
{
char buf[32];
safef(buf, sizeof(buf), "%0.2f", d);
return _pf_string_init(cloneString(buf));
}


struct _pf_string *_pf_string_from_Int(_pf_Int i)
/* Wrap string around Int. */
{
return _pf_string_from_long(i);
}

struct _pf_string *_pf_string_from_Float(_pf_Stack *stack)
/* Wrap string around Float. */
{
return _pf_string_from_long(stack->Float);
}


struct _pf_string *_pf_string_cat(_pf_Stack *stack, int count)
/* Create new string that's a concatenation of the above strings. */
{
int i;
size_t total=0;
size_t pos = 0;
_pf_String string;
char *s;
AllocVar(string);
string->_pf_refCount = 1;
string->_pf_cleanup = _pf_string_cleanup;
for (i=0; i<count; ++i)
    total += stack[i].String->size;
string->s = s = needLargeMem(total+1);
for (i=0; i<count; ++i)
    {
    _pf_String ss = stack[i].String;
    memcpy(s, ss->s, ss->size);
    s += ss->size;
    if (--ss->_pf_refCount <= 0)
        ss->_pf_cleanup(ss, 0);
    }
*s = 0;
string->size = string->allocated = total;
return string;
}



static void _pf_class_cleanup(struct _pf_object *obj, int typeId)
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

struct _pf_object *_pf_class_from_tuple(_pf_Stack *stack, int typeId, _pf_Stack **retStack)
/* Convert tuple on stack to class. */
{
struct _pf_type *type = _pf_type_table[typeId], *fieldType;
struct _pf_base *base = type->base;
struct _pf_object *obj = needMem(base->objSize);
char *s = (char *)(obj);
_pf_Stack *field;

// uglyf("_pf_class_from_tuple(%p %d %p)\n", stack, typeId, retStack);
for (fieldType = base->fields; fieldType != NULL; fieldType = fieldType->next)
    {
    int offset = fieldType->offset;
    switch (fieldType->base->singleType)
	{
	case pf_stBit:
	    *((_pf_Bit *)(s + offset)) = stack->Bit;
	    break;
	case pf_stByte:
	    *((_pf_Byte *)(s + offset)) = stack->Byte;
	    break;
	case pf_stShort:
	    *((_pf_Short *)(s + offset)) = stack->Short;
	    break;
	case pf_stInt:
	    *((_pf_Int *)(s + offset)) = stack->Int;
	    break;
	case pf_stLong:
	    *((_pf_Long *)(s + offset)) = stack->Long;
	    break;
	case pf_stFloat:
	    *((_pf_Float *)(s + offset)) = stack->Float;
	    break;
	case pf_stDouble:
	    *((_pf_Double *)(s + offset)) = stack->Double;
	    break;
	case pf_stString:
	    *((_pf_String *)(s + offset)) = stack->String;
	    break;
	case pf_stArray:
	    *((_pf_Array *)(s + offset)) = stack->Array;
	    break;
	case pf_stList:
	    *((_pf_List *)(s + offset)) = stack->List;
	    break;
	case pf_stDir:
	    *((_pf_Dir *)(s + offset)) = stack->Dir;
	    break;
	case pf_stTree:
	    *((_pf_Tree *)(s + offset)) = stack->Tree;
	    break;
	case pf_stVar:
	    *((_pf_Var *)(s + offset)) = stack->Var;
	    break;
	case pf_stClass:
	    {
	    int typeId = fieldType->typeId;
	    *((_pf_Object *)(s + offset)) = _pf_class_from_tuple(stack, typeId, &stack);
	    stack -= 1;	/* Since += it at end of loop. */
	    break;
	    }
	}
    stack += 1;
    }
obj->_pf_refCount = 1;
obj->_pf_cleanup = _pf_class_cleanup;
if (retStack != NULL)
    *retStack = stack;
return obj;
}

static void _pf_array_cleanup(struct _pf_array *array, int id)
/* Clean up all elements of array, and then array itself. */
{
struct _pf_type *elType = _pf_type_table[array->elType];
verbose(2, "_pf_array_cleanup of %d elements\n", array->size);
if (elType->base->needsCleanup)
    {
    struct _pf_object **objs = (struct _pf_object **)(array->elements);
    int i;
    for (i=0; i<array->size; ++i)
	{
	struct _pf_object *obj = objs[i];
	if (obj != NULL && --obj->_pf_refCount <= 0)
	    obj->_pf_cleanup(obj, array->elType);
	}
    }
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
// array->_pf_typeId = arrayTypeId;
array->elements = elements;
array->size = count;
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
_pf_Bit *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
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
_pf_Byte *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
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
_pf_Short *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
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
_pf_Int *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
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
_pf_Long *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
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
_pf_Float *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
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
_pf_Double *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
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
_pf_String *strings = NULL;
int i;

if (count > 0) AllocArray(strings, count);
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
_pf_Var *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Var;
return array;
}

_pf_Array _pf_class_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
struct _pf_object **elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    {
    struct _pf_object *obj = stack[i].Obj;
    elements[i] = obj;
    }
return array;
}

#ifdef TEMPLATE
_pf_Array _pf_xyz_array_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Create an array of string initialized from tuple on stack. */
{
struct _pf_array *array;
_pf_Xyz *elements = NULL;
int i;

if (count > 0) AllocArray(elements, count);
array = array_of_type(count, count, typeId, elTypeId, sizeof(elements[0]), elements);

for (i=0; i<count; ++i)
    elements[i] = stack[i].Xyz;
return array;
}

#endif /* TEMPLATE */
