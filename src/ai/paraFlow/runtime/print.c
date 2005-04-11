/* print - Implements print function for variable types. */
#include "common.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"

void print(PfStack *stack)
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
        fprintf(f, "%s", val.String);
	break;
    default:
        fprintf(f, "<type %d>\n", base->singleType);
	break;
    }
}
