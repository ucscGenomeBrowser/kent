/* Convert calls to older style library */

#include "common.h"
#include "../compiler/pfPreamble.h"

extern void pf_print(_pf_Stack *stack);

void print(_pf_Stack stack)
/* Print out single variable where type is determined at run time. 
 * Add newline. */
{
pf_print(&stack);
}

struct _pf_object *_zx_tuple_to_class(int type, ...)
/* Create an object of given type initialized from params on stack. */
{
uglyf("Theoretially zx_tuple_to_class on %d\n", type);
return NULL;
}
