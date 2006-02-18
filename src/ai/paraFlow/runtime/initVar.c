/* initVar - handle variable initialization from tuples */
#include "common.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "initVar.h"
#include "dir.h"
#include "array.h"

static _pf_Object tuple_to_class(_pf_Stack *stack, struct _pf_type *type, 
	char *encoding, _pf_Stack **retStack, char **retEncoding);

static _pf_Array tuple_to_array(_pf_Stack *stack, struct _pf_type *elType, 
	char *encoding, _pf_Stack **retStack, char **retEncoding);

_pf_Dir _pf_r_tuple_to_dir(_pf_Stack *stack, struct _pf_type *type, 
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


static _pf_Array tuple_to_array(_pf_Stack *stack, struct _pf_type *elType, 
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
array = _pf_array_of_type(count, count, elType, elSize, buf);

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
