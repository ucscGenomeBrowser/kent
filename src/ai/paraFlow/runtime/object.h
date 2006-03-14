/* object - handle runtime objects.  Do reference
 * counting and automatic cleanup. */

#ifndef OBJECT_H
#define OBJECT_H


struct _pf_object
/* Common heading for all objects. */
    {
    int _pf_refCount;			     	    /* Number of references. */
    void (*_pf_cleanup)(struct _pf_object *obj, int id); /* Called when refCount <= 0 */
    _pf_polyFunType *_pf_polyTable;
    };

struct _pf_array
/* An array object. */
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct _pf_array *obj, int id); /* Called when refCount <= 0 */
    struct _pf_type *elType;	/* Type of each element. */
    char *elements;		/* Pointer to elements. */
    int size;			/* Count of elements used. */
    int allocated;		/* Count of elements allocated. */
    int elSize;			/* Size of each element. */
    };

struct _pf_string
/* A string object. */
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct _pf_string *obj, int id); /* Called when refCount <= 0 */
    char *s;			/* Pointer to elements. */
    int size;			/* Count of chars used. */
    int allocated;		/* Count of chars allocated. */
    };

void _pf_var_link(_pf_Var var);
/* Increment _pf_refCount if variable needs cleanup. */

void _pf_var_cleanup(_pf_Var var);
/* If variable needs cleanup decrement _pf_refCount and if necessary 
 * call _pf_cleanup */

void _pf_cleanup_interface(void *v, int typeId);
/* Clean up interface of some sort. */

void _pf_nil_use();
/* Complain about use of undefined object and punt. */

void _pf_nil_check(void *v);
/* Punt if v is nil. */
 
#endif /* OBJECT_H */

