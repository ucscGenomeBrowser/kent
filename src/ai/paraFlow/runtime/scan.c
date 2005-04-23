/* scan - read back a variable of a given type in the same
 * format that it was printed in. */

#include "common.h"
#include "hash.h"
#include "dyString.h"
#include "../compiler/pfPreamble.h"


void _pf_sca(FILE *f, _pf_Stack *stack)
/* Read in variable from file. */
{
struct _pf_type *type = _pf_type_table[stack[1].Var.typeId];
struct _pf_base *base = type->base;
union _pf_varless val = stack[1].Var.val;


uglyf("_pf_sca\n");

/* Clean up input. */
if (base->needsCleanup)
    {
    if (val.Obj != NULL && --val.Obj->_pf_refCount <= 0)
	val.Obj->_pf_cleanup(val.Obj, stack->Var.typeId);
    }

/* Return output. */
stack[0].Var.typeId = 0;
stack[0].Var.val.Int = 0;
}
