/* object - handle runtime objects.  Do reference
 * counting and automatic cleanup. */

#ifndef OBJECT_H
#define OBJECT_H


struct _pf_object
/* Common heading for all objects. */
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct _pf_array *obj); /* Called when refCount <= 0 */
    int _pf_typeId; 		     	        /* Index of type in _pf_type_table. */
    };

struct _pf_array
/* An array object. */
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct _pf_array *obj); /* Called when refCount <= 0 */
    int _pf_typeId; 		     	        /* Index of type in _pf_type_table. */
    char *elements;		/* Pointer to elements. */
    int count;			/* Count of elements used. */
    int allocated;		/* Count of elements allocated. */
    int elSize;			/* Size of each element. */
    int	elType;			/* Type of each element. */
    };


#endif /* OBJECT_H */

