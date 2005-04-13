/* object - handle runtime objects.  Do reference
 * counting and automatic cleanup. */

#include "common.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"

void _pf_var_link(_pf_Var var)
/* Increment _pf_refCount if variable needs cleanup. */
{
struct _pf_type *type = _pf_type_table[var.typeId];
if (type->base->needsCleanup && var.val.Obj != NULL)
    var.val.Obj->_pf_refCount += 1;
}

void _pf_var_cleanup(_pf_Var var)
/* If variable needs cleanup decrement _pf_refCount and if necessary 
 * call _pf_cleanup */
{
struct _pf_type *type = _pf_type_table[var.typeId];
if (type->base->needsCleanup && var.val.Obj != NULL)
    if ((var.val.Obj->_pf_refCount -= 1) <= 0)
         var.val.Obj->_pf_cleanup(var.val.Obj, var.typeId);
}

