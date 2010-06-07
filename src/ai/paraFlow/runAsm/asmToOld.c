/* Convert calls to older style library */

#include "common.h"
#include "../compiler/pfPreamble.h"

extern void pf_print(_pf_Stack *stack);

void print(_pf_Stack stack)
/* Print out single variable where type is determined at run time. 
 * Add newline. */
{
pf_print(&stack);
}

struct _pf_object *vaTupleToClass(struct _pf_type *type, va_list args)
/* Return object of given type initialized from args. */
{
struct _pf_type *fieldType;
struct _pf_base *base = type->base;
struct _pf_object *obj = needMem(base->objSize);
char *s = (char *)obj;

obj->_pf_refCount = 1;
obj->_pf_cleanup = _pf_class_cleanup;
obj->_pf_cleanup = NULL;	// ugly
obj->_pf_polyTable = base->polyTable;
for (fieldType = base->fields; fieldType != NULL; fieldType = fieldType->next)
    {
    enum _pf_single_type st = fieldType->base->singleType;
    char *output = s + fieldType->offset;
    int i;
    _pf_Long l;
    double d;
    void *v;
    struct _pf_string *string;
    struct _pf_var var;
    switch (st)
        {
	case pf_stBit:
	    i = va_arg(args, int);
	    *((_pf_Bit *)(output)) = i;
	    break;
	case pf_stByte:
	    i = va_arg(args, int);
	    *((_pf_Byte *)(output)) = i;
	    break;
	case pf_stShort:
	    i = va_arg(args, int);
	    *((_pf_Short *)(output)) = i;
	    break;
	case pf_stInt:
	    i = va_arg(args, int);
	    *((_pf_Int *)(output)) = i;
	    break;
	case pf_stLong:
	    l = va_arg(args, _pf_Long);
	    *((_pf_Long *)(output)) = l;
	    break;
	case pf_stFloat:
	    d = va_arg(args, double);
	    *((_pf_Float *)(output)) = d;
	    break;
	case pf_stDouble:
	    d = va_arg(args, double);
	    *((_pf_Double *)(output)) = d;
	    break;
	case pf_stVar:
	    *((_pf_Var *)(output)) = va_arg(args, struct _pf_var);
	    break;
	case pf_stArray:
	case pf_stClass:
	case pf_stDir:
	case pf_stFlowPt:
	case pf_stToPt:
	    v = va_arg(args, void *);
	    *((void **)(output)) = v;
	    break;
	case pf_stString:
	    string = va_arg(args, struct _pf_string *);
	    *((struct _pf_string **)(output)) = string;
	    break;
	default:
	    internalErr();
	    break;
	}
    }

return obj;
}

struct _pf_object *_zx_tuple_to_class(int type, ...)
/* Create an object of given type initialized from params on stack. */
{
struct _pf_object *obj;
va_list args;
va_start(args, type);

obj = vaTupleToClass(_pf_type_table[type], args);
va_end(args);
return obj;
}
