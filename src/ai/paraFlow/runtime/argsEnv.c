/* argsEnv - set up command line argument array and
 * environment dir. */

#include "common.h"
#include "portable.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "string.h"
#include "initVar.h"


void _pf_init_args(int argc, char **argv, _pf_String *retProg, _pf_Array *retArgs)
/* Set up command line arguments. */
{
_pf_Array args;
int stringTypeId = _pf_find_string_type_id();
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

void keyIn(_pf_Stack *stack)
/* Get next key press. */
{
char buf[2];
buf[0] = rawKeyIn();
buf[1] = 0;
stack[0].String = _pf_string_from_const(buf);
}

void milliTicks(_pf_Stack *stack)
/* Return a long value that counts up pretty
 * quickly.  Units are milliseconds.   It may
 * either be starting since program startup or
 * relative to some other absolute number depending
 * on the system it is running on. */
{
stack[0].Long = clock1000();
}

void randInit(_pf_Stack *stack)
/* Change random number stream by initializing it with the current time. */
{
srand(clock1000());
rand();
}

void randNum(_pf_Stack *stack)
/* Return random number between 0 and 1 */
{
static double scale = 1.0/RAND_MAX;
stack[0].Double = rand()*scale;
}

