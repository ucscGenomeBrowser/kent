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

struct interfaceHeader
/* All interfaces begin with these three fields.  (The rest of
 * the fields are function pointers to the methods. */
    {
    int _pf_refCount;
    void (*_pf_cleanup)(void *obj, int typeId);
    struct _pf_object *_pf_obj;
    int _pf_objTypeId;
    };



void _pf_cleanup_interface(void *v, int typeId)
/* Clean up interface of some sort. */
{
struct interfaceHeader *face = v;
struct _pf_object *obj = face->_pf_obj;
if ((obj->_pf_refCount -= 1) <= 0)
    obj->_pf_cleanup(obj, face->_pf_objTypeId);
freeMem(face);
}

