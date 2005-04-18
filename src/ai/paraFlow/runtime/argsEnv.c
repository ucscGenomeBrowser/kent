/* argsEnv - set up command line argument array and
 * environment dir. */

#include "common.h"
#include "runType.h"
#include "../compiler/pfPreamble.h"
#include "object.h"
#include "string.h"
#include "initVar.h"


void _pf_init_args(int argc, char **argv, _pf_String *retProg, _pf_Array *retArgs)
/* Set up command line arguments. */
{
_pf_Array args;
int stringTypeId = _pf_find_simple_type("string");
int i;
*retProg = _pf_string_from_const(argv[0]);
struct _pf_string **a;
args = _pf_dim_array(argc-1, stringTypeId);
a = (struct _pf_string **)(args->elements);
for (i=1; i<argc; ++i)
    a[i-1] = _pf_string_from_const(argv[i]);
*retArgs = args;
}

void die(_pf_Stack *stack)
/* Print  message and die. */
{
_pf_String string = stack[0].String;
errAbort(string->s);
}
