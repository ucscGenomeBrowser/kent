/* dir - Implement ParaFlow dir - a collection keyed by strings
 * which internally is a hash table. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "runType.h"
#include "../compiler/pfPreamble.h"
#include "object.h"
#include "string.h"

static void dirCleanup(struct _pf_dir *dir, int typeId)
/* Clean up resources associated with dir. */
{
struct hash *hash = dir->hash;
struct _pf_type *elType = dir->elType;
struct _pf_base *base = elType->base;
if (base->needsCleanup)
    {
    uglyf("dirCleanup deep\n");
    struct hashCookie it = hashFirst(hash);
    struct hashEl *hel;
    while ((hel = hashNext(&it)) != NULL)
        {
	struct _pf_object *obj = hel->val;
	if (--obj->_pf_refCount <= 0)
	    obj->_pf_cleanup(obj, elType->typeId);
	}
    }
else
    {
    uglyf("dirCleanup shallow\n");
    freeHashAndVals(&hash);
    }
freeMem(dir);
}


struct _pf_dir *_pf_dir_new(int estimatedSize, int elTypeId)
/* Create a dir.  The estimatedSize is just a guideline.
 * Generally you want this to be about the same as the
 * number of things going into the dir for optimal
 * performance.  If it's too small it will go slower.
 * If it's too big it will use up more memory.
 * Still, it's fine to be pretty approximate with it. */
{
int sizeLog2 = digitsBaseTwo(estimatedSize + (estimatedSize>>1));
struct _pf_type *type = _pf_type_table[elTypeId];
struct _pf_dir *dir;
AllocVar(dir);
dir->_pf_refCount = 1;
dir->_pf_cleanup = dirCleanup;
dir->elType = _pf_type_table[elTypeId];
dir->hash = hashNew(sizeLog2);
return dir;
}

struct _pf_dir *_pf_int_dir_from_tuple(_pf_Stack *stack, int count, int typeId,
	int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = _pf_dir_new(0, elTypeId);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Int val = stack[1].Int;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}

struct _pf_dir *_pf_string_dir_from_tuple(_pf_Stack *stack, int count, int typeId,
	int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = _pf_dir_new(0, elTypeId);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_String val = stack[1].String;
    stack += 2;
    hashAdd(hash, key->s, val);
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}

 
