/* dir - Implement ParaFlow dir - a collection keyed by strings
 * which internally is a hash table. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "string.h"

int _pf_countOurLevel(char *encoding);

static void dirCleanup(struct _pf_dir *dir, int typeId)
/* Clean up resources associated with dir. */
{
struct hash *hash = dir->hash;
struct _pf_type *elType = dir->elType;
struct _pf_base *base = elType->base;
if (base->needsCleanup)
    {
    // uglyf("deep cleaning dir\n");
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
    // uglyf("shallow cleaning dir\n");
    freeHashAndVals(&hash);
    }
freeMem(dir);
}


struct _pf_dir *pfDirNew(int estimatedSize, struct _pf_type *type)
/* Create a dir.  The estimatedSize is just a guideline.
 * Generally you want this to be about the same as the
 * number of things going into the dir for optimal
 * performance.  If it's too small it will go slower.
 * If it's too big it will use up more memory.
 * Still, it's fine to be pretty approximate with it. */
{
int sizeLog2 = digitsBaseTwo(estimatedSize + (estimatedSize>>1));
struct _pf_dir *dir;
AllocVar(dir);
dir->_pf_refCount = 1;
dir->_pf_cleanup = dirCleanup;
dir->elType = type;
dir->hash = hashNew(sizeLog2);
return dir;
}

struct _pf_dir *_pf_dir_new(int estimatedSize, int elTypeId)
/* Like pfDirNew, but takes typeId instead of type. */
{
return pfDirNew(estimatedSize, _pf_type_table[elTypeId]);
}


int _pf_dir_size(struct _pf_dir *dir)
/* Return size of dir. */
{
return dir->hash->elCount;
}

/* The next 7 functions are all of the form
 *    _pf_XXX_dir from tuple
 * where XXX is bit,byte,short,int,long,float,double
 * They are virtually identical, and if I were clever
 * I'd figure out a macro that could reduce them
 * to one declaration and k macro uses. */

struct _pf_dir *_pf_bit_dir_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Bit val = stack[1].Bit;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}

struct _pf_dir *_pf_byte_dir_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Byte val = stack[1].Byte;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}

struct _pf_dir *_pf_short_dir_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Short val = stack[1].Short;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}


struct _pf_dir *_pf_int_dir_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
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

struct _pf_dir *_pf_long_dir_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Long val = stack[1].Long;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}

struct _pf_dir *_pf_float_dir_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Float val = stack[1].Float;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}

struct _pf_dir *_pf_double_dir_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Double val = stack[1].Double;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}


struct _pf_dir *_pf_char_dir_from_tuple(_pf_Stack *stack, int count, 
	int typeId, int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Char val = stack[1].Char;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}


/* Done with the block of 8 nearly identical functions.
 * Try to get your eyes to unglaze now.... */

struct _pf_dir *_pf_string_dir_from_tuple(_pf_Stack *stack, int count, int typeId,
	int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = pfDirNew(0, _pf_type_table[elTypeId]);
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

 
struct _pf_object *_pf_dir_lookup_object(_pf_Stack *stack, _pf_Bit addRef)
/* Stack contains directory, keyword.  Return value associated
 * with keyword on top of stack.  Neither one of the inputs is
 * pushed with a reference, so you don't need to deal with 
 * decrementing the ref counts on the input side.  The output
 * does get an extra refcount though. */
{
struct _pf_dir *dir = stack[0].Dir;
struct _pf_string *string = stack[1].String;
struct _pf_object *obj = hashFindVal(dir->hash, string->s);
if (addRef && obj != NULL)
    obj->_pf_refCount += 1;
return obj;
}

void _pf_dir_lookup_number(_pf_Stack *stack)
/* Stack contains directory, keyword.  Return number of
 * some sort back on the stack. */
{
struct _pf_dir *dir = stack[0].Dir;
struct _pf_string *string = stack[1].String;
void *v = hashFindVal(dir->hash, string->s);
struct _pf_base *base = dir->elType->base;
switch (base->singleType)
    {
    case pf_stBit:
	if (v == NULL) stack[0].Bit = 0;
	else stack[0].Bit = *((_pf_Bit *)v);
        break;
    case pf_stByte:
	if (v == NULL) stack[0].Byte = 0;
	else stack[0].Byte = *((_pf_Byte *)v);
        break;
    case pf_stShort:
	if (v == NULL) stack[0].Short = 0;
	else stack[0].Short = *((_pf_Short *)v);
        break;
    case pf_stInt:
	if (v == NULL) stack[0].Int = 0;
	else stack[0].Int = *((_pf_Int *)v);
        break;
    case pf_stLong:
	if (v == NULL) stack[0].Long = 0;
	else stack[0].Long = *((_pf_Long *)v);
        break;
    case pf_stFloat:
	if (v == NULL) stack[0].Float = 0;
	else stack[0].Float = *((_pf_Float *)v);
        break;
    case pf_stDouble:
	if (v == NULL) stack[0].Double = 0;
	else stack[0].Double = *((_pf_Double *)v);
        break;
    case pf_stChar:
	if (v == NULL) stack[0].Char = 0;
	else stack[0].Char = *((_pf_Char *)v);
        break;
    }
}

void _pf_dir_add_obj(struct _pf_dir  *dir, char *key, _pf_Stack *stack)
/* Add non-numerical object to directory.  Here stack[0] contains object
 * value. */
{
struct _pf_object *obj = stack[0].Obj;
struct hash *hash;
struct hashEl *hel;
struct _pf_object *oldObj;

hash = dir->hash;
hel = hashLookup(hash, key);
if (hel != NULL)
    {
    oldObj = hel->val;
    if (--oldObj->_pf_refCount <= 0)
        oldObj->_pf_cleanup(oldObj, dir->elType->typeId);
    hel->val = obj;
    }
else
    {
    hashAdd(hash, key, obj);
    }
}

void _pf_dir_add_object(_pf_Stack *stack, int dirOffset)
/* Stack contains object, directory, keyword.  Add object to
 * directory. */
{
struct _pf_dir *dir = stack[dirOffset].Dir;
struct _pf_string *string = stack[dirOffset+1].String;

_pf_nil_check(dir);
_pf_nil_check(string);
_pf_dir_add_obj(dir, string->s, stack);
}

static void *cloneStackNum(_pf_Stack *stack, struct _pf_type *type)
/* Return clone of number. */
{
switch (type->base->singleType)
    {
    case pf_stBit:
	return CloneVar(&stack[0].Bit);
    case pf_stByte:
	return CloneVar(&stack[0].Byte);
    case pf_stShort:
	return CloneVar(&stack[0].Short);
    case pf_stInt:
	return CloneVar(&stack[0].Int);
    case pf_stLong:
	return CloneVar(&stack[0].Long);
    case pf_stFloat:
	return CloneVar(&stack[0].Float);
    case pf_stDouble:
	return CloneVar(&stack[0].Double);
    case pf_stChar:
	return CloneVar(&stack[0].Char);
    default:
    	internalErr();
	return NULL;
    }
}

void _pf_dir_add_num(struct _pf_dir  *dir, char *key, _pf_Stack *stack)
/* Add numerical object to directory.  Here stack[0] contains numerical
 * value. */
{
struct hash *hash = dir->hash;
struct hashEl *hel;
void *clonedNum = cloneStackNum(&stack[0], dir->elType);

hel = hashLookup(hash, key);
if (hel != NULL)
    {
    freeMem(hel->val);
    hel->val = clonedNum;
    }
else
    hashAdd(hash, key, clonedNum);
}

void _pf_dir_add_number(_pf_Stack *stack, int dirOffset)
/* Stack contains number, directory, keyword.  Add number to
 * directory. */
{
struct _pf_dir *dir = stack[dirOffset].Dir;
struct _pf_string *string = stack[dirOffset+1].String;
_pf_nil_check(dir);
_pf_nil_check(string);
_pf_dir_add_num(dir, string->s, stack);
}

_pf_Dir _pf_r_tuple_to_dir(_pf_Stack *stack, struct _pf_type *elType, 
	char *encoding, _pf_Stack **retStack, char **retEncoding)
/* Convert tuple to dir. */
{
struct _pf_base *base = elType->base;
int elSize = elType->base->size;
int i, count = _pf_countOurLevel(encoding);
struct _pf_dir *dir = pfDirNew(count, elType);

if (encoding[0] != '(')
    errAbort("Expecting ( in array tuple encoding, got %c", encoding[0]);
encoding += 1;
for (i=0; i<count; ++i)
    {
    _pf_String key = stack[0].String;
    stack += 1;
    if (base->needsCleanup)
        {
	struct _pf_object *obj;
	_pf_suckItemOffStack(&stack, &encoding, elType, &obj);
	hashAdd(dir->hash, key->s, obj);
	}
    else
        {
	switch (base->singleType)
	    {
	    case pf_stBit:
		{
	        _pf_Bit x;
		_pf_suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stByte:
		{
	        _pf_Byte x;
		_pf_suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stShort:
		{
	        _pf_Short x;
		_pf_suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stInt:
		{
	        _pf_Int x;
		_pf_suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stLong:
		{
	        _pf_Long x;
		_pf_suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stFloat:
		{
	        _pf_Float x;
		_pf_suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stDouble:
		{
	        _pf_Double x;
		_pf_suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stChar:
		{
	        _pf_Char x;
		_pf_suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    default:
		internalErr();
	    }
	}
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
if (encoding[0] != ')')
    errAbort("Expecting ) in array tuple encoding, got %c", encoding[0]);
encoding += 1;
*retStack = stack;
*retEncoding = encoding;
return dir;
}

_pf_Dir _pf_tuple_to_dir(_pf_Stack *stack, int typeId, char *encoding)
/* Convert tuple to dir. */
{
return _pf_r_tuple_to_dir(stack, _pf_type_table[typeId], encoding, &stack, &encoding);
}


static void dir_iterator_cleanup(struct _pf_iterator *it)
/* Clean up iterator.  Unlike object cleanups this does not free
 * iterator itself. */
{
freeMem(it->data);
}

struct dir_iterator
    {
    struct hashCookie cookie;
    struct _pf_base *base;
    };

static int dir_iterator_next(struct _pf_iterator *it, void *output, void *key)
/* Fetch next value. */
{
struct dir_iterator *dit = it->data;
struct hashEl *hel = hashNext(&dit->cookie);
void *val;
if (hel == NULL)
    return FALSE;
val = hel->val;
if (key != NULL)
    {
    char **typedKey = key;
    *typedKey = hel->name;
    }
if (dit->base->needsCleanup)
    {
    struct _pf_object *obj = val;
    struct _pf_object **pObj = output;
    obj->_pf_refCount += 1;
    *pObj = obj;
    }
else
    {
    switch (dit->base->singleType)
	{
	case pf_stBit:
	    memcpy(output, val, sizeof(_pf_Bit));
	    break;
	case pf_stByte:
	    memcpy(output, val, sizeof(_pf_Byte));
	    break;
	case pf_stShort:
	    memcpy(output, val, sizeof(_pf_Short));
	    break;
	case pf_stInt:
	    memcpy(output, val, sizeof(_pf_Int));
	    break;
	case pf_stLong:
	    memcpy(output, val, sizeof(_pf_Long));
	    break;
	case pf_stFloat:
	    memcpy(output, val, sizeof(_pf_Float));
	    break;
	case pf_stDouble:
	    memcpy(output, val, sizeof(_pf_Double));
	    break;
	case pf_stChar:
	    memcpy(output, val, sizeof(_pf_Char));
	    break;
	default:
	    internalErr();
	    return FALSE;
	}
    }
return TRUE;
}

struct _pf_iterator _pf_dir_iterator_init(_pf_Dir dir)
/* Create an iterator object for dir */
{
struct dir_iterator *dit;
struct _pf_iterator it;
AllocVar(dit);
dit->cookie = hashFirst(dir->hash);
dit->base = dir->elType->base;
it.next = dir_iterator_next;
it.cleanup = dir_iterator_cleanup;
it.data = dit;
return it;
}

void _pf_cm_dir_keys(_pf_Stack *stack)
/* implements to dir.keys() into array of string . */
{
struct _pf_dir *dir = stack[0].Dir;
if (dir == NULL)
    _pf_nil_use();
else
    {
    struct hash *hash = dir->hash;
    int elIx = 0;
    struct hashCookie cookie = hashFirst(dir->hash);
    struct hashEl *el;
    _pf_String *elements;
    _pf_Array a = _pf_dim_array(hash->elCount, _pf_find_string_type_id());
    elements = (_pf_String*)(a->elements);
    while ((el = hashNext(&cookie)) != NULL)
         {
	 _pf_String string = _pf_string_from_const(el->name);
	 elements[elIx] = string;
	 elIx += 1;
	 }
    stack[0].Array = a;
    }
}
