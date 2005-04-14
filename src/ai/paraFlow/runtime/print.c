/* print - Implements print function for variable types. */
#include "common.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"

void printField(FILE *f, void *data, struct _pf_base *base);

void printClass(FILE *f, struct _pf_object *obj, struct _pf_base *base)
/* Print out each field of class. */
{
struct _pf_type *field;
char *s = (char *)obj;
fprintf(f, "(");
for (field = base->fields; field != NULL; field = field->next)
    {
    printField(f, s+field->offset, field->base);
    if (field->next != NULL)
         fprintf(f, ",");
    }
fprintf(f, ")");
}

void printArray(FILE *f, struct _pf_array *array, struct _pf_base *base)
/* Print out each field of class. */
{
struct _pf_type *elType = _pf_type_table[array->elType];
int i;
boolean needsComma = FALSE;
char *s = array->elements;
fprintf(f, "(");
for (i=0; i<array->size; ++i)
    {
    if (needsComma)
         fprintf(f, ",");
    needsComma = TRUE;
    printField(f, s, elType->base);
    s += array->elSize;
    }
fprintf(f, ")");
}


void printField(FILE *f, void *data, struct _pf_base *base)
/* Print out a data from a single field of given type. */
{
switch (base->singleType)
    {
    case pf_stBit:
	{
	_pf_Bit *p = data;
        fprintf(f, "%d", *p);
	break;
	}
    case pf_stByte:
	{
        _pf_Byte *p = data;
	fprintf(f, "%d", *p);
	break;
	}
    case pf_stShort:
	{
        _pf_Short *p = data;
	fprintf(f, "%d", *p);
	break;
	}
    case pf_stInt:
	{
        _pf_Int *p = data;
	fprintf(f, "%d", *p);
	break;
	}
    case pf_stLong:
	{
        _pf_Long *p = data;
	fprintf(f, "%lld", *p);
	break;
	}
    case pf_stFloat:
	{
        _pf_Float *p = data;
	fprintf(f, "%f", *p);
	break;
	}
    case pf_stDouble:
	{
        _pf_Double *p = data;
	fprintf(f, "%f", *p);
	break;
	}
    case pf_stString:
	{
        _pf_String *p = data;
	fprintf(f, "%s", (*p)->s);
	break;
	}
    case pf_stClass:
	{
	struct _pf_object **p = data;
        printClass(f, *p, base);
	break;
	}
    case pf_stArray:
        {
	struct _pf_array **p = data;
        printArray(f, *p, base);
	break;
	}
    default:
	internalErr();
	break;
    }
}


void prin(_pf_Stack *stack)
/* Print out single variable where type is determined at run time. */
{
struct _pf_type *type = _pf_type_table[stack->Var.typeId];
struct _pf_base *base = type->base;
union _pf_varless val = stack->Var.val;
FILE *f = stdout;
switch (base->singleType)
    {
    case pf_stBit:
        fprintf(f, "%d", val.Bit);
	break;
    case pf_stByte:
        fprintf(f, "%d", val.Byte);
	break;
    case pf_stShort:
        fprintf(f, "%d", val.Short);
	break;
    case pf_stInt:
        fprintf(f, "%d", val.Int);
	break;
    case pf_stLong:
        fprintf(f, "%lld", val.Long);
	break;
    case pf_stFloat:
        fprintf(f, "%f", val.Float);
	break;
    case pf_stDouble:
        fprintf(f, "%f", val.Double);
	break;
    case pf_stString:
        fprintf(f, "%s", val.String->s);
	if (--val.Obj->_pf_refCount <= 0)
	    val.Obj->_pf_cleanup(val.Obj, stack->Var.typeId);
	break;
    case pf_stClass:
        printClass(f, val.Obj, base);
	if (--val.Obj->_pf_refCount <= 0)
	    val.Obj->_pf_cleanup(val.Obj, stack->Var.typeId);
	break;
    case pf_stArray:
        printArray(f, val.Array, base);
	if (--val.Obj->_pf_refCount <= 0)
	    val.Obj->_pf_cleanup(val.Obj, stack->Var.typeId);
	break;
    default:
        fprintf(f, "<type %d>\n", base->singleType);
	break;
    }
}

void print(_pf_Stack *stack)
/* Print out single variable where type is determined at run time. 
 * Add newline. */
{
prin(stack);
fputc('\n', stdout);
}
