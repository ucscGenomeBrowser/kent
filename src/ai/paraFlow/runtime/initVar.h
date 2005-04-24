/* initVar - handle variable initialization from tuples */

_pf_Object  _pf_tuple_to_class(_pf_Stack *stack, int typeId, char *encoding);
/* Convert tuple on stack to class. */

_pf_Array _pf_tuple_to_array(_pf_Stack *stack, int typeId, char *encoding);
/* Convert tuple on stack to array. */

_pf_Dir _pf_tuple_to_dir(_pf_Stack *stack, int typeId, char *encoding);
/* Convert tuple on stack to array. */

void _pf_class_cleanup(struct _pf_object *obj, int typeId);
/* Clean up all class fields, and then class itself. */

void _pf_array_cleanup(struct _pf_array *array, int id);
/* Clean up all elements of array, and then array itself. */

_pf_Array _pf_dim_array(int size, int elTypeId);
/* Return array of given type and size, initialized to zeroes. */

_pf_Array _pf_bit_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_byte_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_short_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_int_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_long_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_float_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_double_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_string_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Array _pf_class_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId,
	char *encoding, _pf_Stack **retStack, char **retEncoding);
_pf_Array _pf_var_array_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);

_pf_List _pf_bit_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_List _pf_byte_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_List _pf_short_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_List _pf_int_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_List _pf_long_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_List _pf_float_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_List _pf_double_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_List _pf_string_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_List _pf_class_list_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);

_pf_Tree _pf_bit_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Tree _pf_byte_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Tree _pf_short_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Tree _pf_int_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Tree _pf_long_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Tree _pf_float_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Tree _pf_double_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Tree _pf_string_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Tree _pf_class_tree_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);

_pf_Dir _pf_bit_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Dir _pf_byte_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Dir _pf_short_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Dir _pf_int_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Dir _pf_long_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Dir _pf_float_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Dir _pf_double_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Dir _pf_string_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);
_pf_Dir _pf_class_dir_from_tuple(_pf_Stack *stack, int count, int typeId, int elTypeId);

