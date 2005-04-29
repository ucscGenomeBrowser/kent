/* argsEnv - set up command line argument array and
 * environment dir. */

#include "common.h"
#include "portable.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "string.h"
#include "initVar.h"

static char **cl_env;

void _pf_init_args(int argc, char **argv, _pf_String *retProg, _pf_Array *retArgs, 
	char *environ[])
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
cl_env = environ;
}

void pf_getEnvArray(_pf_Stack *stack)
/* Return an array of string containing the environment
 * int var=val format.   We'll leave it to paraFlow 
 * library to turn this into a hash. */
{
int stringTypeId = _pf_find_string_type_id();
int envSize = 0;
int i;
struct _pf_string **a;
_pf_Array envArray;
for (i=0; ; ++i)
    {
    if (cl_env[i] == NULL)
        break;
    envSize += 1;
    }
envArray = _pf_dim_array(envSize, stringTypeId);
a = (struct _pf_string **)(envArray->elements);

for (i=0; i<envSize; ++i)
    a[i] = _pf_string_from_const(cl_env[i]);

stack[0].Array = envArray;
}

void pf_die(_pf_Stack *stack)
/* Print  message and die. */
{
_pf_String string = stack[0].String;
errAbort(string->s);
}

void pf_keyIn(_pf_Stack *stack)
/* Get next key press. */
{
char buf[2];
buf[0] = rawKeyIn();
buf[1] = 0;
stack[0].String = _pf_string_from_const(buf);
}

void pf_milliTicks(_pf_Stack *stack)
/* Return a long value that counts up pretty
 * quickly.  Units are milliseconds.   It may
 * either be starting since program startup or
 * relative to some other absolute number depending
 * on the system it is running on. */
{
stack[0].Long = clock1000();
}

void pf_randInit(_pf_Stack *stack)
/* Change random number stream by initializing it with the current time. */
{
srand(clock1000());
rand();
}

void pf_randNum(_pf_Stack *stack)
/* Return random number between 0 and 1 */
{
static double scale = 1.0/RAND_MAX;
stack[0].Double = rand()*scale;
}

