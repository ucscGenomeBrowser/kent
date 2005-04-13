/* object - handle runtime objects.  Do reference
 * counting and automatic cleanup. */

#ifndef OBJECT_H
#define OBJECT_H


struct _pf_object
/* Common heading for all objects. */
    {
    int _pf_refCount;			     	    /* Number of references. */
    void (*_pf_cleanup)(struct _pf_object *obj, int id); /* Called when refCount <= 0 */
//     int _pf_typeId; 		         /* Index of type in _pf_type_table. */
    };

struct _pf_array
/* An array object. */
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct _pf_array *obj, int id); /* Called when refCount <= 0 */
//    int _pf_typeId; 		/* Index of type in _pf_type_table. */
    char *elements;		/* Pointer to elements. */
    int size;			/* Count of elements used. */
    int allocated;		/* Count of elements allocated. */
    int elSize;			/* Size of each element. */
    int	elType;			/* Type of each element. */
    };

void _pf_var_link(_pf_Var var);
/* Increment _pf_refCount if variable needs cleanup. */

void _pf_var_cleanup(_pf_Var var);
/* If variable needs cleanup decrement _pf_refCount and if necessary 
 * call _pf_cleanup */

#endif /* OBJECT_H */

