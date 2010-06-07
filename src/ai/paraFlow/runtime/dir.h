/* dir - Implement ParaFlow dir - a collection keyed by strings
 * which internally is a hash table. */

#ifndef DIR_H
#define DIR_H

struct _pf_dir
    {
    int _pf_refCount;	     	      /* Number of references. */
    void (*_pf_cleanup)(struct _pf_dir *dir, int id); /* Called when refCount <= 0 */
    struct _pf_type *elType;	/* Type of each element. */
    struct hash *hash;
    };

struct _pf_dir *pfDirNew(int estimatedSize, struct _pf_type *type);
/* Create a dir.  The estimatedSize is just a guideline.
 * Generally you want this to be about the same as the
 * number of things going into the dir for optimal
 * performance.  If it's too small it will go slower.
 * If it's too big it will use up more memory.
 * Still, it's fine to be pretty approximate with it. */

struct _pf_dir *_pf_dir_new(int estimatedSize, int elTypeId);
/* Like pfDirNew, but takes typeId instead of type. */

int _pf_dir_size(struct _pf_dir *dir);
/* Return size of dir. */

struct _pf_object *_pf_dir_lookup_object(_pf_Stack *stack, _pf_Bit addRef);
/* Stack contains directory, keyword.  Return value associated
 * with keyword on top of stack.  Neither one of the inputs is
 * pushed with a reference, so you don't need to deal with 
 * decrementing the ref counts on the input side.  The output
 * does get an extra refcount though. */

void _pf_dir_add_num(struct _pf_dir  *dir, char *key, _pf_Stack *stack);
/* Add numerical object to directory.  Here stack[0] contains numerical
 * value. */

void _pf_dir_lookup_number(_pf_Stack *stack);
/* Stack contains directory, keyword.  Return number of
 * some sort back on the stack. */

void _pf_dir_add_obj(struct _pf_dir  *dir, char *key, _pf_Stack *stack);
/* Add non-numerical object to directory.  Here stack[0] contains object
 * value. */

void _pf_dir_add_object(_pf_Stack *stack, int dirOffset);
/* Stack contains object, directory, keyword.  Add object to
 * directory. */

void _pf_dir_add_number(_pf_Stack *stack, int dirOffset);
/* Stack contains number, directory, keyword.  Add number to
 * directory. */

void _pf_cm_dir_keys(_pf_Stack *stack);
/* implements to dir.keys() into array of string . */

#endif /* DIR_H */
