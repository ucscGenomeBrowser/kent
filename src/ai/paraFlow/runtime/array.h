/* Built in array handling. */

#ifndef ARRAY_H

void _pf_array_cleanup(struct _pf_array *array, int id);
/* Clean up all elements of array, and then array itself. */

_pf_Array _pf_dim_array(long size, int elTypeId);
/* Return array of given type and size, initialized to zeroes. */

_pf_Array _pf_multi_dim_array(_pf_Stack *stack, int dimCount,
    int typeId);
/* Return multi-dimensional array where individual dimensions are on stack */

_pf_Array _pf_array_of_type(int count, int allocated, 
	struct _pf_type *elType, int elSize, void *elements);
/* Create an array of string initialized from tuple on stack. 
 * You still need to fill in array->elements. */

_pf_Array _pf_bit_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_byte_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_short_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_int_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_long_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_float_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_double_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_string_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_dyString_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_class_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId,
	char *encoding, _pf_Stack **retStack, char **retEncoding);
_pf_Array _pf_var_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
/* Arrays from various fixed-type tuples. */

void _pf_array_append(_pf_Array array, void *elBuf);
/* Append element to array.   elBuf points to first byte of element. */

void _pf_cm_array_push(_pf_Stack *stack);
/* Push one element onto stack. */

void _pf_cm_array_sort(_pf_Stack *stack);
/* Sort array using a user-supplied compare function. */

#endif /* ARRAY_H */
